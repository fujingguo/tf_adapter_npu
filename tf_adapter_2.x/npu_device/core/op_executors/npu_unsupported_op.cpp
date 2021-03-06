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

#include "npu_unsupported_op.h"

#include "npu_device.h"

namespace npu {
std::string NpuUnsupportedOp::AttachedDebugString() const {
  std::stringstream ss;
  ss << "Fallback reason " << fallback_reason_;
  return ss.str();
}

void NpuUnsupportedOp::RunImpl(TFE_Context *context, NpuDevice *device, int num_inputs, TFE_TensorHandle **inputs,
                               int num_outputs, TFE_TensorHandle **outputs, TF_Status *status) const {
  device->FallbackCPU(context, NodeDef(), num_inputs, inputs, num_outputs, outputs, status);
}
}  // namespace npu
