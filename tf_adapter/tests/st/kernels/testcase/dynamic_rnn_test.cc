#include "tensorflow/core/framework/attr_value.pb.h"
#include "tensorflow/core/framework/attr_value_util.h"
#include "tensorflow/core/framework/fake_input.h"
#include "tensorflow/core/framework/node_def.pb.h"
#include "tensorflow/core/framework/node_def_builder.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/shape_inference.h"
#include "tensorflow/core/platform/test.h"
#include "tf_adapter/kernels/aicore/dynamic_rnn_ops.cc"
#include "gtest/gtest.h"
#include <memory>

namespace tensorflow {
namespace {
PartialTensorShape TShape(std::initializer_list<int64> dims) {
  return PartialTensorShape(dims);
}

FakeInputFunctor FakeInputStub(DataType dt) {
  return [dt](const OpDef &op_def, int in_index, const NodeDef &node_def,
              NodeDefBuilder *builder) {
    char c = 'a' + (in_index % 26);
    string in_node = string(&c, 1);
    builder->Input(in_node, 0, dt);
    return Status::OK();
  };
}

TEST(DynamicRnnOpTest, TestDynamicRnn) {
  DataTypeSlice input_types(
      {DT_FLOAT, DT_FLOAT, DT_FLOAT, DT_INT32, DT_FLOAT, DT_FLOAT});
  MemoryTypeSlice input_memory_types;
  DataTypeSlice output_types({DT_FLOAT, DT_FLOAT, DT_FLOAT, DT_FLOAT, DT_FLOAT,
                              DT_FLOAT, DT_FLOAT, DT_FLOAT});
  MemoryTypeSlice output_memory_types;
  DeviceBase *device = new DeviceBase(Env::Default());
  NodeDef *node_def = new NodeDef();
  OpDef *op_def = new OpDef();
  OpKernelConstruction *context = new OpKernelConstruction(
      DEVICE_CPU, device, nullptr, node_def, op_def, nullptr, input_types,
      input_memory_types, output_types, output_memory_types, 1, nullptr);
  DynamicRnnOP<int> dynamic_rnn(context);
  OpKernelContext *ctx = nullptr;
  dynamic_rnn.Compute(ctx);
  dynamic_rnn.IsExpensive();
  delete device;
  delete node_def;
  delete op_def;
  delete context;
}

TEST(DynamicRnnOpTest, TestDynamicRnnShapeInference) {
  const OpRegistrationData *reg;
  TF_CHECK_OK(OpRegistry::Global()->LookUp("DynamicRnn", &reg));
  OpDef op_def = reg->op_def;
  NodeDef def;
  TF_CHECK_OK(NodeDefBuilder("dummy", &op_def)
                  .Attr("T", DT_FLOAT)
                  .Attr("direction", "BIDIRECTIONAL")
                  .Input(FakeInputStub(DT_FLOAT))
                  .Input(FakeInputStub(DT_FLOAT))
                  .Input(FakeInputStub(DT_FLOAT))
                  .Input(FakeInputStub(DT_INT32))
                  .Input(FakeInputStub(DT_FLOAT))
                  .Input(FakeInputStub(DT_FLOAT))
                  .Finalize(&def));
  shape_inference::InferenceContext c(0, &def, op_def,
                                      {TShape({1, 16, 16}), TShape({32, 64}),
                                       TShape({64}), TShape({64}),
                                       TShape({1, 16, 16}), TShape({64})},
                                      {}, {}, {});
  TF_CHECK_OK(reg->shape_inference_fn(&c));
}
} // namespace
} // namespace tensorflow
