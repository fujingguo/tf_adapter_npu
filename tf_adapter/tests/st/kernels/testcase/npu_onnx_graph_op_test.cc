#include <memory>
#include "tf_adapter/kernels/npu_ops.cc"
#include "gtest/gtest.h"

namespace tensorflow {
class NpuOnnxGraphOpTest : public testing::Test {
 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}
};

TEST_F(NpuOnnxGraphOpTest, TestNpuOnnxGraphOp) {
    DataTypeSlice input_types({DT_FLOAT});
    MemoryTypeSlice input_memory_types;
    DataTypeSlice output_types({DT_FLOAT});
    MemoryTypeSlice output_memory_types;
    DeviceBase *device = new DeviceBase(Env::Default());
    NodeDef *node_def = new NodeDef();
    OpDef *op_def = new OpDef();
    OpKernelConstruction *context = new OpKernelConstruction(DEVICE_CPU, device, nullptr, node_def, op_def, nullptr,
                                                             input_types, input_memory_types, output_types, output_memory_types,
                                                             1, nullptr);
    NpuOnnxGraphOp npu_onnx_graph_conv(context);
    OpKernelContext *ctx = nullptr;
    npu_onnx_graph_conv.Compute(ctx);
    npu_onnx_graph_conv.IsExpensive();
    delete device;
    delete node_def;
    delete op_def;
    delete context;
}
}