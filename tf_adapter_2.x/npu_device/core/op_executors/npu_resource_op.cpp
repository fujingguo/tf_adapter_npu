/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "npu_resource_op.h"

#include "tensorflow/core/graph/algorithm.h"

#include "npu_kernel_registry.h"
#include "npu_device.h"

namespace npu {
NpuResourceOp::NpuResourceOp(const tensorflow::OpRegistrationData *op_spec, const tensorflow::NodeDef &ndef,
                             TensorShapes input_shapes)
    : OpExecutor(op_spec, ndef, input_shapes) {
  AssembleInputDesc(input_shapes_, input_dtypes_, &attached_attrs_);
}

std::string NpuResourceOp::AttachedDebugString() const {
  std::stringstream ss;
  return ss.str();
}

NpuResourceOp::HashKey NpuResourceOp::Hash(const std::vector<tensorflow::ResourceHandle> &handles) {
  if (handles.empty()) {
    return 0;
  }
  HashKey hash = tensorflow::Hash64(handles[0].DebugString());
  for (size_t i = 1; i < handles.size(); i++) {
    hash = tensorflow::Hash64Combine(hash, tensorflow::Hash64(handles[i].DebugString()));
  }
  return hash;
}

void NpuResourceOp::GetFuncSpec(const std::vector<tensorflow::ResourceHandle> &handles,
                                std::shared_ptr<NpuConcreteGraph> *spec) const {
  shared_lock.lock_shared();
  HashKey key = Hash(handles);
  auto iter = handles_to_specs_.find(key);
  if (iter != handles_to_specs_.cend()) {
    *spec = iter->second;
  }
  shared_lock.unlock_shared();
}

void NpuResourceOp::CacheFuncSpec(const std::vector<tensorflow::ResourceHandle> &handles,
                                  std::shared_ptr<NpuConcreteGraph> spec) const {
  shared_lock.lock();
  HashKey key = Hash(handles);
  handles_to_specs_[key] = std::move(spec);
  shared_lock.unlock();
}

void NpuResourceOp::RunImpl(TFE_Context *context, NpuDevice *device, int num_inputs, TFE_TensorHandle **inputs,
                            int num_outputs, TFE_TensorHandle **outputs, TF_Status *status) const {
  // ????????????????????????Resource?????????????????????Mirrored???Iterator????????????????????????NPU??????????????????
  std::vector<tensorflow::ResourceHandle> npu_resources;
  std::vector<tensorflow::ResourceHandle> cpu_resources;
  std::set<int> consumed_inputs;
  for (int i = 0; i < num_inputs; ++i) {
    const tensorflow::Tensor *tensor = nullptr;
    NPU_CTX_REQUIRES_OK(status, GetTensorHandleTensor(inputs[i], &tensor));
    if (tensor->dtype() == tensorflow::DT_RESOURCE) {
      if (IsNpuTensorHandle(inputs[i])) {
        npu_resources.emplace_back(tensor->flat<tensorflow::ResourceHandle>()(0));
      } else {
        if (IsCpuTensorHandle(inputs[i])) {
          cpu_resources.emplace_back(tensor->flat<tensorflow::ResourceHandle>()(0));
        }
      }
    } else {
      consumed_inputs.insert(i);
    }
  }

  if (npu_resources.empty()) {
    DLOG() << "NPU Executing op " << Op() << " fallback cpu as no npu resource input";
    device->FallbackCPU(context, NodeDef(), num_inputs, inputs, num_outputs, outputs, status);
    return;
  }

  NPU_CTX_REQUIRES(status, cpu_resources.empty(),
                   tensorflow::errors::Internal(Op(), " has resource from both cpu and npu"));

  std::shared_ptr<NpuConcreteGraph> func_spec = nullptr;
  GetFuncSpec(npu_resources, &func_spec);

  if (func_spec == nullptr) {
    std::unique_ptr<tensorflow::Graph> graph = std::make_unique<tensorflow::Graph>(tensorflow::OpRegistry::Global());

    tensorflow::Status s;
    tensorflow::Node *target_node = graph->AddNode(NodeDef(), &s);
    NPU_CTX_REQUIRES_OK(status, s);
    target_node->set_name(Op());

    const auto &shapes = InputShapes();
    const auto &types = InputTypes();

    int arg_index = 0;
    int resource_index = 0;
    std::unordered_map<std::shared_ptr<tensorflow::NodeDef>, tensorflow::Node *> def_nodes;

    for (int i = 0; i < num_inputs; i++) {
      if (types[i] == tensorflow::DT_RESOURCE) {
        std::shared_ptr<ResourceGenerator> generator = nullptr;
        const auto &handle = npu_resources[resource_index++];
        device->GetResourceGeneratorDef(handle, &generator);
        NPU_CTX_REQUIRES(status, generator != nullptr,
                         tensorflow::errors::Internal("Unknown npu resource ", handle.DebugString()));

        if (def_nodes.find(generator->NodeDef()) == def_nodes.end()) {
          tensorflow::Node *node = graph->AddNode(*generator->NodeDef(), &s);
          NPU_CTX_REQUIRES_OK(status, s);
          def_nodes[generator->NodeDef()] = node;
        }
        NPU_CTX_REQUIRES(status, graph->AddEdge(def_nodes[generator->NodeDef()], generator->Index(), target_node, i),
                         tensorflow::errors::Internal("Failed add edge from ", def_nodes[generator->NodeDef()]->name(),
                                                      " to ", target_node->name()));
      } else {
        tensorflow::Node *node = nullptr;
        NPU_CTX_REQUIRES_OK(status, tensorflow::NodeBuilder("arg_" + std::to_string(i), "_Arg")
                                      .Attr("T", types[i])
                                      .Attr("index", arg_index++)
                                      .Attr("_output_shapes", {shapes[i]})
                                      .Finalize(graph.get(), &node));
        NPU_CTX_REQUIRES(
          status, graph->AddEdge(node, 0, target_node, i),
          tensorflow::errors::Internal("Failed add edge from ", node->name(), " to ", target_node->name()));
      }
    }

    const auto &output_types = OutputTypes();
    for (int i = 0; i < num_outputs; i++) {
      tensorflow::Node *node = nullptr;
      NPU_CTX_REQUIRES_OK(status, tensorflow::NodeBuilder("ret_" + std::to_string(i), "_Retval")
                                    .Input(target_node, i)
                                    .Attr("T", output_types[i])
                                    .Attr("index", i)
                                    .Finalize(graph.get(), &node));
    }

    tensorflow::FixupSourceAndSinkEdges(graph.get());
    AssembleParserAddons(context, graph.get());

    if (kDumpGraph && kDumpExecutionDetail) {
      OptimizeStageGraphDumper graph_dumper(Op());
      std::string suffix = npu_resources[0].name();
      for (int i = 1; i < npu_resources.size(); i++) {
        suffix += ".";
        suffix += npu_resources[i].name();
      }
      graph_dumper.Dump(suffix, graph->ToGraphDefDebug());
    }

    uint64_t ge_graph_id = NextUUID();
    const std::string graph_name = NodeDef().op() + "_" + std::to_string(ge_graph_id);

    auto spec =
      std::make_shared<NpuMutableConcreteGraph>(graph_name, InputTypes(), OutputTypes(), ge_graph_id, std::move(graph));
    spec->SetConsumedInputs(consumed_inputs);
    func_spec = spec;

    CacheFuncSpec(npu_resources, spec);
  }
  func_spec->RunOneShot(context, device, num_inputs, inputs, num_outputs, outputs, status);
}
}  // namespace npu
