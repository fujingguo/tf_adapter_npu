#include "tensorflow/core/framework/fake_input.h"
#include "tensorflow/core/framework/shape_inference.h"
#include "tensorflow/core/platform/test.h"
#include "tf_adapter/kernels/aicore/non_zero_with_value_ops.cc"
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

TEST(NonZeroWithValueOpTest, TestNonZeroWithValue) {
  DataTypeSlice input_types({DT_INT32});
  MemoryTypeSlice input_memory_types;
  DataTypeSlice output_types({DT_INT32, DT_INT64, DT_INT64});
  MemoryTypeSlice output_memory_types;
  DeviceBase *device = new DeviceBase(Env::Default());
  NodeDef *node_def = new NodeDef();
  OpDef *op_def = new OpDef();
  OpKernelConstruction *context = new OpKernelConstruction(
      DEVICE_CPU, device, nullptr, node_def, op_def, nullptr, input_types,
      input_memory_types, output_types, output_memory_types, 1, nullptr);
  NonZeroWithValueOP<int> non_zero_with_value(context);
  OpKernelContext *ctx = nullptr;
  non_zero_with_value.Compute(ctx);
  non_zero_with_value.IsExpensive();
  delete device;
  delete node_def;
  delete op_def;
  delete context;
}

TEST(NonZeroWithValueOpTest, TestNonZeroWithValueShapeInference) {
  const OpRegistrationData *reg;
  TF_CHECK_OK(OpRegistry::Global()->LookUp("NonZeroWithValue", &reg));
  OpDef op_def = reg->op_def;
  NodeDef def;
  TF_CHECK_OK(NodeDefBuilder("dummy", &op_def)
                  .Attr("T", DT_FLOAT)
                  .Attr("output_type", DT_INT32)
                  .Input(FakeInputStub(DT_FLOAT))
                  .Finalize(&def));
  shape_inference::InferenceContext c(0, &def, op_def, {TShape({3, 4})}, {}, {}, {});
  std::vector<shape_inference::ShapeHandle> input_shapes;
  TF_CHECK_OK(reg->shape_inference_fn(&c));
  ASSERT_EQ("[12]", c.DebugString(c.output(0)));
  ASSERT_EQ("[24]", c.DebugString(c.output(1)));
  ASSERT_EQ("[1]", c.DebugString(c.output(2)));
}
} // namespace
} // namespace tensorflow