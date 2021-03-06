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

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "tensorflow/c/c_api.h"
#include "tensorflow/c/c_api_internal.h"
#include "tensorflow/c/eager/c_api_experimental.h"
#include "tensorflow/c/eager/c_api_internal.h"
#include "tensorflow/core/framework/common_shape_fns.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/shape_inference.h"
#include "tensorflow/core/kernels/data/iterator_ops.h"
#include "tensorflow/core/util/env_var.h"

#include "npu_hdc.h"

using namespace tensorflow;
namespace npu {
class IteratorH2D : public OpKernel {
 public:
  explicit IteratorH2D(OpKernelConstruction *ctx) : OpKernel(ctx) {
    OP_REQUIRES_OK(ctx, ctx->GetAttr("channel_name", &channel_name_));
    OP_REQUIRES_OK(ctx, ctx->GetAttr("device_ids", &device_ids_));
  }

  ~IteratorH2D() override = default;

  void Compute(OpKernelContext *ctx) override {
    if (!initialized_.exchange(true)) {
      std::stringstream ss;
      for (auto device_id : device_ids_) {
        ss << device_id << " ";
      }
      channels_.resize(device_ids_.size());
      for (size_t i = 0; i < device_ids_.size(); i++) {
        OP_REQUIRES_OK(ctx, npu::HdcChannel::Create(device_ids_[i], channel_name_, &channels_[i]));
      }
      LOG(INFO) << "Hdc channel for iterator resource " << channel_name_ << " to device ["
                << ss.str().substr(0, ss.str().size() - 1) << "] created";
    }

    CancellationManager *cm = ctx->cancellation_manager();
    CancellationToken token = cm->get_cancellation_token();
    bool cancelled = !cm->RegisterCallback(token, [this]() {
      for (const auto &channel : channels_) {
        channel->Destroy();
      }
    });
    if (cancelled) {
      ctx->SetStatus(tensorflow::errors::Internal("Iterator resource ", channel_name_, " consume after destroyed"));
      return;
    }

    data::IteratorResource *iterator;
    OP_REQUIRES_OK(ctx, LookupResource(ctx, HandleFromInput(ctx, 0), &iterator));
    core::ScopedUnref unref_iterator(iterator);
    std::vector<Tensor> components;
    bool end_of_sequence = false;

    int64_t nums = ctx->input(1).flat<int64>()(0);
    OP_REQUIRES(ctx, nums >= 0, tensorflow::errors::InvalidArgument(channel_name_, " invalid consume nums ", nums));

    int64_t consumed = 0;
    while (nums == 0 || consumed++ < nums) {
      components.clear();

      Status status = iterator->GetNext(ctx, &components, &end_of_sequence);
      if (!status.ok()) {
        for (const auto &channel : channels_) {
          OP_REQUIRES_OK(ctx, channel->NotifyAbnormal());
        }
        ctx->SetStatus(status);
        return;
      } else if (end_of_sequence) {
        for (const auto &channel : channels_) {
          OP_REQUIRES_OK(ctx, channel->NotifyFinish());
        }
        ctx->SetStatus(errors::OutOfRange("Iterator resource ", channel_name_, " reach end of sequence"));
        return;
      }

      for (const auto &channel : channels_) {
        status = channel->SendTensors(components);
        if (!status.ok()) {  // suppress warning message for OP_REQUIRES_OK
          ctx->SetStatus(status);
          return;
        }
      }
    }

    cm->DeregisterCallback(token);
  }

 private:
  std::string channel_name_;
  std::vector<int> device_ids_;
  std::vector<std::shared_ptr<npu::HdcChannel>> channels_;
  std::atomic_bool initialized_{false};
};

REGISTER_KERNEL_BUILDER(Name("IteratorH2D").Device(DEVICE_CPU).Priority(3), IteratorH2D);
}  // namespace npu