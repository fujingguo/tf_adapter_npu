/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2020. All rights reserved.
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

#include "tensorflow/core/common_runtime/function.h"
#include "tensorflow/core/kernels/data/captured_function.h"
#include "tf_adapter/common/adp_logger.h"
#include "tf_adapter/common/common.h"
#include "tf_adapter/common/compat_tf1_tf2.h"

namespace tensorflow {
namespace data {
namespace {
class DPGroupDatasetOp : public DatasetOpKernel {
public:
  explicit DPGroupDatasetOp(OpKernelConstruction *ctx) : DatasetOpKernel(ctx) {
    CHECK_NOT_NULL(ctx);
    OP_REQUIRES_OK(ctx, ctx->GetAttr("output_types", &output_types_));
    OP_REQUIRES_OK(ctx, ctx->GetAttr("output_shapes", &output_shapes_));
  }
  ~DPGroupDatasetOp() override = default;
  void MakeDataset(OpKernelContext *ctx, DatasetBase **output) override {
    CHECK_NOT_NULL(ctx);
    CHECK_NOT_NULL(output);
    std::vector<DatasetBase *> inputs;
    for (int i = 0; i < ctx->num_inputs(); ++i) {
      DatasetBase *input = nullptr;
      OP_REQUIRES_OK(ctx, GetDatasetFromVariantTensor(ctx->input(i), &input));
      inputs.push_back(input);
    }
    *output = new (std::nothrow) Dataset(ctx, inputs, output_types_, output_shapes_);
    OP_REQUIRES(ctx, *output != nullptr, errors::Internal("Failed new dataset of DPGroupDatasetOp"));
  }

private:
  class Dataset : public DatasetBase {
  public:
    explicit Dataset(OpKernelContext *ctx, const std::vector<DatasetBase *> &inputs, const DataTypeVector &output_types,
                     const std::vector<PartialTensorShape> &output_shapes)
        : DatasetBase(DatasetContext(ctx)), inputs_(inputs) {
      for (const auto &input : inputs_) { input->Ref(); }
      output_types_.insert(output_types_.end(), output_types.begin(), output_types.end());
      output_shapes_.insert(output_shapes_.end(), output_shapes.begin(), output_shapes.end());
    }

    ~Dataset() override {
      for (const auto &input : inputs_) { input->Unref(); }
    }

    std::unique_ptr<IteratorBase> MakeIteratorInternal(const string &prefix) const override {
      return std::unique_ptr<IteratorBase>(new (std::nothrow) Iterator({this, npu::CatStr(prefix, "::GEOP")}));
    }

    const DataTypeVector &output_dtypes() const override { return output_types_; }

    const std::vector<PartialTensorShape> &output_shapes() const override { return output_shapes_; }

    string DebugString() const override { return "DPGroupDatasetOp::Dataset"; }

    STATUS_FUNCTION_ONLY_TF2(CheckExternalState() const override);

  protected:
    Status AsGraphDefInternal(SerializationContext *ctx, DatasetGraphDefBuilder *b, Node **output) const override {
      return Status::OK();
    }

  private:
    class Iterator : public DatasetIterator<Dataset> {
    public:
      explicit Iterator(const Params &params) : DatasetIterator<Dataset>(params) {}
      ~Iterator() override = default;
      Status Initialize(IteratorContext *ctx) override {
        REQUIRES_NOT_NULL(ctx);
        ADP_LOG(INFO) << "Start to initialize iterator of DPGroupDatasetOp";
        mutex_lock l(mu_);
        try {
          input_impls_.resize(dataset()->inputs_.size());
        } catch (...) { return errors::InvalidArgument("input impls resize failed."); }
        for (size_t i = 0; i < input_impls_.size(); ++i) {
          TF_RETURN_IF_ERROR(
#ifdef TF_VERSION_TF2
              dataset()->inputs_[i]->MakeIterator(ctx, this, npu::CatStr(prefix(), "[", i, "]"), &input_impls_[i])
#else
              dataset()->inputs_[i]->MakeIterator(ctx, npu::CatStr(prefix(), "[", i, "]"), &input_impls_[i])
#endif
          );
        }
        return Status::OK();
      }

      Status GetNextInternal(IteratorContext *ctx, std::vector<Tensor> *out_tensors, bool *end_of_sequence) override {
        *end_of_sequence = true;
        return Status::OK();
      }

    protected:
      STATUS_FUNCTION_ONLY_TF2(SaveInternal(SerializationContext *ctx, IteratorStateWriter *writer) override);
      STATUS_FUNCTION_ONLY_TF1(SaveInternal(IteratorStateWriter *writer) override);

      Status RestoreInternal(IteratorContext *ctx, IteratorStateReader *reader) override { return Status::OK(); }

    private:
      mutex mu_;
      std::vector<std::unique_ptr<IteratorBase>> input_impls_ GUARDED_BY(mu_);
    };
    const std::vector<DatasetBase *> inputs_;
    DataTypeVector output_types_;
    std::vector<PartialTensorShape> output_shapes_;
  };
  DataTypeVector output_types_;
  std::vector<PartialTensorShape> output_shapes_;
};

REGISTER_KERNEL_BUILDER(Name("DPGroupDataset").Device(DEVICE_CPU), DPGroupDatasetOp);
}  // namespace
}  // namespace data
}  // namespace tensorflow
