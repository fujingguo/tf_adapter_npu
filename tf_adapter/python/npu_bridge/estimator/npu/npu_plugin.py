#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2019-2021. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================

"""NPU plugin for Tensorflow"""

from hccl.manage.api import get_local_rank_size
from hccl.manage.api import get_rank_id
from npu_bridge import tf_adapter
from npu_bridge.estimator.npu import util
from npu_bridge.estimator.npu import npu_scope
from tensorflow.python.ops import variable_scope
from tensorflow.python.ops import init_ops

__auto_tune_mode = str(tf_adapter.AUTO_TUNE_MODE)
__op_debug_level = str(tf_adapter.OP_DEBUG_LEVEL)
__option_exec_enable_scope_fusion_passes = str(tf_adapter.OPTION_EXEC_ENABLE_SCOPE_FUSION_PASSES)
__option_exec_profiling_mode = str(tf_adapter.OPTION_EXEC_PROFILING_MODE)
__option_exec_profiling_options = str(tf_adapter.OPTION_EXEC_PROFILING_OPTIONS)
__option_graph_run_mode = str(tf_adapter.OPTION_GRAPH_RUN_MODE)
__option_exec_option_exec_hccl_flag = str(tf_adapter.OPTION_EXEC_HCCL_FLAG)


def init_op_compiler_cache_mode(init, op_compiler_cache_mode):
    """Initialize op compiler cache mode"""
    if op_compiler_cache_mode is not None:
        init["ge.op_compiler_cache_mode"] = op_compiler_cache_mode


def init_op_compiler_cache_dir(init, op_compiler_cache_dir):
    """Initialize op compiler cache directory"""
    if op_compiler_cache_dir is not None:
        init["ge.op_compiler_cache_dir"] = op_compiler_cache_dir


def init_debug_dir(init, debug_dir):
    """Initialize debug directory"""
    if debug_dir is not None:
        init["ge.debugDir"] = debug_dir


def check_graph_run_mode(graph_run_mode):
    """Verify graph run mode"""
    if graph_run_mode > 1:
        raise ValueError('"graph_run_mode" value must be 0 or 1')


def npu_resource_init(graph_run_mode=1,
                      op_debug_level=0,
                      enable_profiling=False,
                      profiling_options=None,
                      auto_tune_mode=None,
                      precision_mode=None,
                      enable_scope_fusion_passes=None,
                      enable_exception_dump=0,
                      aoe_mode=None,
                      work_path=None,
                      op_compiler_cache_mode=None,
                      op_compiler_cache_dir=None,
                      debug_dir=None,
                      hcom_multi_mode=False,
                      distribute_config=None):
    """Initialize NPU resource"""
    util.check_nonnegative_integer(graph_run_mode, "graph_run_mode")
    check_graph_run_mode(graph_run_mode)
    util.check_nonnegative_integer(enable_exception_dump, "enable_exception_dump")
    util.check_nonnegative_integer(op_debug_level, "op_debug_level")
    util.check_bool_type(enable_profiling, "enable_profiling")
    enable_profiling = util.convert_bool_to_int(enable_profiling)

    init = {}
    init[__option_graph_run_mode] = str(graph_run_mode)
    init[__op_debug_level] = str(op_debug_level)
    init[__option_exec_profiling_mode] = str(enable_profiling)

    if enable_profiling:
        if profiling_options is None:
            raise ValueError('profiling_options must be set when use profiling')
        init[__option_exec_profiling_options] = str(profiling_options)

    if auto_tune_mode is not None:
        init[__auto_tune_mode] = str(auto_tune_mode)

    if precision_mode is not None:
        init["ge.exec.precision_mode"] = str(precision_mode)
    else:
        if graph_run_mode:
            init["ge.exec.precision_mode"] = str("allow_fp32_to_fp16")
        else:
            init["ge.exec.precision_mode"] = str("force_fp16")

    if enable_scope_fusion_passes is not None:
        init[__option_exec_enable_scope_fusion_passes] = str(enable_scope_fusion_passes)

    init["ge.exec.enable_exception_dump"] = str(enable_exception_dump)
    if aoe_mode is not None:
        util.check_aoe_mode(aoe_mode)
        init["ge.jobType"] = str(aoe_mode)
        if work_path is not None:
            init["ge.tuningPath"] = str(util.check_path(work_path))
        else:
            init["ge.tuningPath"] = str(util.check_path("./"))
        if distribute_config is not None:
            init["distribute_config"] = str(distribute_config)

    init_op_compiler_cache_mode(init, op_compiler_cache_mode)
    init_op_compiler_cache_dir(init, op_compiler_cache_dir)
    init_debug_dir(init, debug_dir)

    util.check_bool_type(hcom_multi_mode, "hcom_multi_mode")
    hcom_multi_mode = util.convert_bool_to_int(hcom_multi_mode)
    init["ge.hcomMultiMode"] = str(hcom_multi_mode)

    init_options = tf_adapter.map_string_string(init)
    tf_adapter.PluginInit(init_options)


def npu_resource_shutdown():
    """Shutdown NPU resource"""
    tf_adapter.PluginFinalize()


def npu_close():
    """NPU closing"""
    tf_adapter.NpuClose()


def init_rdma_pool(mem_size):
    """
    mem_size: ramd pool memory size to be allocated. type:int
    """
    if not isinstance(mem_size, int):
        raise ValueError('{} should be int'.format(mem_size))
    res = tf_adapter.InitRdmaPool(mem_size)
    if res != 0:
        raise RuntimeError('rdma init failed')


def rdma_remote_register(remote_var_list):
    """
    remote_var_list: embedding and opt var list.
    """
    if not isinstance(remote_var_list, (tuple, list)):
        raise ValueError('{} should be tuple or list'.format(remote_var_list))
    var_addr_list = []
    local_rank_size = get_local_rank_size()
    rank_id = get_rank_id()
    server_id = int(rank_id / local_rank_size)
    for var in remote_var_list:
        for line in var:
            var_server_id = int(line[0] / local_rank_size)
            if server_id == var_server_id:
                host_var_info = tf_adapter.HostVarInfo()
                host_var_info.base_addr = line[1]
                host_var_info.var_size = line[2]
                var_addr_list.append(host_var_info)
    res = tf_adapter.RegistRdmaRemoteAddr(var_addr_list)
    if res != 0:
        raise RuntimeError('rdma remote register failed')


def rdma_remote_init(remote_var_list, mem_size):
    """
    remote_var_list: embedding and opt var list.
    mem_size: ramd pool memory size to be allocated. type:int
    """
    if not isinstance(remote_var_list, (tuple, list)):
        raise ValueError('{} should be tuple or list'.format(remote_var_list))
    if not isinstance(mem_size, int):
        raise ValueError('{} should be int'.format(mem_size))
    var_addr_list = []
    local_rank_size = get_local_rank_size()
    rank_id = get_rank_id()
    server_id = int(rank_id / local_rank_size)
    for var in remote_var_list:
        server_var = var[server_id]
        host_var_info = tf_adapter.HostVarInfo()
        host_var_info.base_addr = server_var[1]
        host_var_info.var_size = server_var[2]
        var_addr_list.append(host_var_info)
    res = tf_adapter.RdmaInitAndRegister(var_addr_list, mem_size)
    if res != 0:
        raise RuntimeError('rdma init and register failed')


def get_var_addr_and_size(var_name):
    """
    var_name: variable name.
    """
    if not isinstance(var_name, str):
        raise ValueError('{} should be str'.format(var_name))
    res = tf_adapter.GetVarAddrAndSize(var_name)
    if res[0] != 0:
        raise RuntimeError('{} get var addr and size failed'.format(var_name))
    return res[1], res[2]


def malloc_shared_memory(var_name, shape, data_type):
    """
    var_name: variable name.
    shape: variable shape.
    data_type: variable dtype.
    """
    tensor_info = tf_adapter.TensorInfo()
    tensor_info.var_name = var_name
    tensor_info.dims = tf_adapter.int64_vec(shape)
    tensor_info.data_type = data_type
    res = tf_adapter.MallocSharedMem(tensor_info)
    if res[0] != 0:
        raise RuntimeError('{} malloc shared memory failed'.format(var_name))
    return res[1], res[2]


def get_rdma_cache(data_type, shape, name="rdma_w"):
    """Get rdma cache"""
    with npu_scope.npu_mem_type_scope():
        return variable_scope.get_variable(name=name, shape=shape, dtype=data_type,
                                           initializer=init_ops.zeros_initializer())
