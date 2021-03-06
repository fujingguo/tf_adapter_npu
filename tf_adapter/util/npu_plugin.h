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

#ifndef TENSORFLOW_NPU_PLUGIN_H_
#define TENSORFLOW_NPU_PLUGIN_H_

#include <map>
#include <string>
#include "ge/ge_api_types.h"
#include "ge_plugin.h"
#include "framework/memory/memory_api.h"

const char *const AUTO_TUNE_MODE = "ge.autoTuneMode";
const char *const OP_DEBUG_LEVEL = "ge.opDebugLevel";
const char *const OPTION_EXEC_ENABLE_SCOPE_FUSION_PASSES = ge::OPTION_EXEC_ENABLE_SCOPE_FUSION_PASSES;
const char *const OPTION_EXEC_PROFILING_MODE = ge::OPTION_EXEC_PROFILING_MODE;
const char *const OPTION_EXEC_PROFILING_OPTIONS = ge::OPTION_EXEC_PROFILING_OPTIONS;
const char *const OPTION_GRAPH_RUN_MODE = ge::OPTION_GRAPH_RUN_MODE;
const char* const OPTION_EXEC_HCCL_FLAG = ge::OPTION_EXEC_HCCL_FLAG;
const char* const OPTION_EXEC_PROFILING_FPPONIT_OPTIONS = ge::OPTION_EXEC_PROFILING_FPPONIT_OPTIONS;
const char* const OPTION_EXEC_PROFILING_BPPONIT_OPTIONS = ge::OPTION_EXEC_PROFILING_BPPONIT_OPTIONS;

void PluginInit(std::map<std::string, std::string> &init_options);

void PluginFinalize();

void NpuClose();

int32_t InitRdmaPool(size_t size);

int32_t RegistRdmaRemoteAddr(const std::vector<ge::HostVarInfo> &var_info);

int32_t RdmaInitAndRegister(const std::vector<ge::HostVarInfo> &var_info, size_t size);

int32_t GetVarAddrAndSize(const std::string &var_name, uint64_t &base_addr, uint64_t &var_size);

int32_t MallocSharedMem(const ge::TensorInfo &tensor_info, uint64_t &dev_addr, uint64_t &memory_size);

#endif  // TENSORFLOW_NPU_PLUGIN_H_
