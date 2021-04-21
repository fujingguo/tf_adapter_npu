/**
 * Copyright (c) Huawei Technologies Co., Ltd. 2021. All rights reserved.
 * Description: Common depends and micro defines for and only for data preprocess module
 */

#include <memory>
#include <utility>

#include "tensorflow/c/c_api.h"
#include "tensorflow/c/eager/c_api.h"
#include "tensorflow/c/eager/c_api_experimental.h"
#include "tensorflow/c/tf_status.h"
#include "tensorflow/core/lib/gtl/cleanup.h"
#include "tensorflow/core/platform/logging.h"

#include "absl/algorithm/container.h"
#include "tensorflow/c/c_api_internal.h"
#include "tensorflow/c/eager/immediate_execution_operation.h"
#include "tensorflow/c/eager/tfe_context_internal.h"
#include "tensorflow/c/eager/tfe_op_internal.h"
#include "tensorflow/c/eager/tfe_tensorhandle_internal.h"

#include "npu_custom_kernel.h"
#include "npu_utils.h"

class ReadVariableGraphBuilder {
 public:
  static tensorflow::GraphDef GetGraph(const tensorflow::ResourceHandle resource, TF_Status *status) {
    const std::string &container_name = resource.container();
    const std::string &shared_name = resource.name();

    TensorDataTypes handle_dtyes;
    TensorPartialShapes handle_shapes;
    const auto &dtypes_and_shapes = resource.dtypes_and_shapes();

    for (auto &dtype_and_shape : dtypes_and_shapes) {
      handle_dtyes.push_back(dtype_and_shape.dtype);
      handle_shapes.push_back(dtype_and_shape.shape);
    }

    tensorflow::GraphDef gdef;

    tensorflow::Graph graph(tensorflow::OpRegistry::Global());
    tensorflow::Node *variable;
    tensorflow::Node *read_variable;
    tensorflow::Node *retval;

    NPU_CTX_REQUIRES_OK_RETURN(status,
                               tensorflow::NodeBuilder(WrapResourceName(shared_name), "VarHandleOp")
                                 .Attr("container", container_name)
                                 .Attr("shared_name", shared_name)
                                 .Attr("dtype", handle_dtyes.front())
                                 .Attr("shape", handle_shapes.front())
                                 .Finalize(&graph, &variable),
                               gdef);

    NPU_CTX_REQUIRES_OK_RETURN(status,
                               tensorflow::NodeBuilder("Read_" + shared_name, "ReadVariableOp")
                                 .Input(variable, 0)
                                 .Attr("dtype", handle_dtyes.front())
                                 .Finalize(&graph, &read_variable),
                               gdef);

    NPU_CTX_REQUIRES_OK_RETURN(status,
                               tensorflow::NodeBuilder("Read_" + shared_name + "_Retval", "_Retval")
                                 .Input(read_variable, 0)
                                 .Attr("index", 0)
                                 .Finalize(&graph, &retval),
                               gdef);

    AssembleOpDef(variable);
    AssembleOpDef(read_variable);

    AssembleOutputDesc(TensorShapes({kScalarShape}), {tensorflow::DT_RESOURCE}, variable);
    AssembleInputDesc(TensorShapes({kScalarShape}), {tensorflow::DT_RESOURCE}, read_variable);
    AssembleOutputDesc(handle_shapes, handle_dtyes, read_variable);

    graph.ToGraphDef(&gdef);
    return gdef;
  }
};

static auto kernel = [](TFE_Context *context, NpuDevice *dev, const npu::OpSpec *spec,
                        const TensorShapes &output_shapes, const tensorflow::NodeDef &parser_ndef, int num_inputs,
                        TFE_TensorHandle **inputs, int num_outputs, TFE_TensorHandle **outputs, TF_Status *status) {
  const tensorflow::Tensor *handle = nullptr;
  NPU_CTX_REQUIRES_OK(status, npu::UnwrapTensor(inputs[0], &handle));

  auto resource = handle->scalar<tensorflow::ResourceHandle>()();
  NPU_CTX_REQUIRES(status, resource.dtypes_and_shapes().size() == 1,
                   tensorflow::errors::Internal(resource.DebugString(), " type and shape size invalid ",
                                                resource.dtypes_and_shapes().size(), " expect 1"));

  auto var_read_graph = ReadVariableGraphBuilder::GetGraph(resource, status);
  if (TF_GetCode(status) != TF_OK) {
    return;
  }
  std::string graph_name = "ReadVariableOp_" + resource.name();
  if (kDumpExecutionDetail && kDumpGraph) {
    std::string file_name = graph_name + ".pbtxt";
    WriteTextProto(tensorflow::Env::Default(), file_name, var_read_graph);
    LOG(INFO) << "NPU Dump variable resource init graph to: " << file_name;
  }

  dev->RunGeGraphPin2CpuAnonymous(context, graph_name, var_read_graph, 0, nullptr, num_outputs, outputs, status);
};

NPU_REGISTER_CUSTOM_KERNEL("ReadVariableOp", kernel);