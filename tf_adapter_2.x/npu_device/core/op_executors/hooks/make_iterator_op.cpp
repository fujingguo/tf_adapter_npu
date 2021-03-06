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

#include "tensorflow/core/graph/algorithm.h"

#include "op_executors/npu_kernel_registry.h"
#include "npu_utils.h"

namespace npu {
namespace {
class MakeIteratorGraphBuilder {
 public:
  static tensorflow::GraphDef GetGraph(const std::string &container_name, const std::string &shared_name,
                                       const TensorPartialShapes &shapes, const TensorDataTypes &types,
                                       TF_Status *status) {
    tensorflow::GraphDef gdef;

    tensorflow::Graph graph(tensorflow::OpRegistry::Global());
    tensorflow::Node *device_queue;
    tensorflow::Node *make_iterator;
    tensorflow::Node *iterator_v2;
    NPU_CTX_REQUIRES_OK_RETURN(status,
                               tensorflow::NodeBuilder("DeviceQueue_" + shared_name, "DeviceQueueDataset")
                                 .Attr("channel_name", shared_name)
                                 .Attr("output_types", types)
                                 .Attr("output_shapes", shapes)
                                 .Attr("_iterator_name", shared_name)
                                 .Finalize(&graph, &device_queue),
                               gdef);
    NPU_CTX_REQUIRES_OK_RETURN(status,
                               tensorflow::NodeBuilder("IteratorV2_" + shared_name, "IteratorV2")
                                 .Attr("container", container_name)
                                 .Attr("shared_name", shared_name)
                                 .Attr("output_types", types)
                                 .Attr("output_shapes", shapes)
                                 .Finalize(&graph, &iterator_v2),
                               gdef);
    NPU_CTX_REQUIRES_OK_RETURN(status,
                               tensorflow::NodeBuilder("InitMakeIterator_" + shared_name, "MakeIterator")
                                 .Attr("_kernel", "dp")
                                 .Attr("_iterator_name", shared_name)
                                 .Input(device_queue, 0)
                                 .Input(iterator_v2, 0)
                                 .Finalize(&graph, &make_iterator),
                               gdef);

    if (kDumpExecutionDetail || kDumpGraph) {
      std::string file_name = "dp_init_" + shared_name + ".inner.pbtxt";
      DLOG() << "NPU Dump mirrored resource init graph inner graph to: " << file_name;
      WriteTextProto(tensorflow::Env::Default(), file_name, graph.ToGraphDefDebug());
    }

    // Tensorflow model parser bug?????????????????????dpop?????????????????????remove???
    std::string func_name = "dpop_init_func_" + shared_name;
    tensorflow::FunctionDefLibrary fdef_lib;
    tensorflow::FunctionDef *fdef = fdef_lib.add_function();
    tensorflow::GraphToFunctionDef(graph, func_name, fdef);

    tensorflow::Graph dpop_graph(tensorflow::OpRegistry::Global());

    tensorflow::AttrValue function_attr;
    function_attr.mutable_func()->set_name(func_name);

    tensorflow::Node *dpop_node;
    NPU_CTX_REQUIRES_OK_RETURN(status,
                               tensorflow::NodeBuilder(func_name, "DPOP")
                                 .Input(std::vector<tensorflow::NodeBuilder::NodeOut>{})
                                 .Attr("Tin", tensorflow::DataTypeVector{})
                                 .Attr("Tout", tensorflow::DataTypeVector{})
                                 .Attr("function", function_attr)
                                 .Finalize(&dpop_graph, &dpop_node),
                               gdef);
    AssembleOpDef(dpop_node);
    dpop_node->AddAttr("func_def", fdef_lib.SerializeAsString());
    tensorflow::FixupSourceAndSinkEdges(&dpop_graph);
    dpop_graph.ToGraphDef(&gdef);
    return gdef;
  }
};
}  // namespace

static auto kernel = [](TFE_Context *context, NpuDevice *dev, const tensorflow::NodeDef &ndef, int num_inputs,
                        TFE_TensorHandle **inputs, int num_outputs, TFE_TensorHandle **outputs, TF_Status *status) {
  TF_UNUSED_VARIABLE(ndef);
  TF_UNUSED_VARIABLE(num_outputs);
  TF_UNUSED_VARIABLE(outputs);
  for (int j = 0; j < num_inputs; ++j) {
    TFE_TensorHandle *input = inputs[j];
    if (tensorflow::unwrap(input)->DataType() == tensorflow::DT_RESOURCE) {
      const tensorflow::Tensor *tensor;
      NPU_CTX_REQUIRES_OK(status, GetTensorHandleTensor(input, &tensor));
      auto handle = tensor->scalar<tensorflow::ResourceHandle>()();
      TensorPartialShapes shapes;
      TensorDataTypes types;
      NPU_CTX_REQUIRES_OK(status, dev->GetMirroredIteratorShapesAndTypes(handle, shapes, types));
      auto dp_init_graph = MakeIteratorGraphBuilder::GetGraph(handle.container(), handle.name(), shapes, types, status);
      if (TF_GetCode(status) != TF_OK) { return; }
      if (kDumpExecutionDetail || kDumpGraph) {
        std::string file_name = "dp_init_" + handle.name() + ".pbtxt";
        DLOG() << "NPU Dump mirrored resource init graph to: " << file_name;
        WriteTextProto(tensorflow::Env::Default(), file_name, dp_init_graph);
      }
      dev->RunGeGraphPin2CpuAnonymous(context, "dp_init_" + handle.name(), dp_init_graph, num_inputs, inputs, 0,
                                      nullptr, status);
      if (TF_GetCode(status) != TF_OK) { return; }
      // ?????????????????????Provider????????????1???N???????????????????????????????????????Device??????
      dev->CreateIteratorProvider(context, tensor, {dev->device_id}, status);
      if (TF_GetCode(status) != TF_OK) { return; }
    }
  }
};

NPU_REGISTER_FALLBACK_HOOK("MakeIterator", kernel);
NPU_REGISTER_FALLBACK_HOOK("MultiDeviceIteratorInit", kernel);
}  // namespace npu