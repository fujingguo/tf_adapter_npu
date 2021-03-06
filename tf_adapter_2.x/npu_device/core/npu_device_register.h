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

#ifndef NPU_DEVICE_CORE_NPU_DEVICE_REGISTER_H
#define NPU_DEVICE_CORE_NPU_DEVICE_REGISTER_H

#include <map>
#include <string>

#include "tensorflow/c/eager/c_api.h"

namespace npu {
std::string CreateDevice(TFE_Context *context, const char *name, int device_index,
                         const std::map<std::string, std::string> &device_options);

void ReleaseDeviceResource();
}  // namespace npu

#endif  // NPU_DEVICE_CORE_NPU_DEVICE_REGISTER_H
