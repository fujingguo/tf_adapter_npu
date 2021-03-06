#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2019. All rights reserved.
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

from npu_bridge.helper import helper
import tensorflow as tf

gen_npu_cpu_ops = helper.get_gen_ops()


class LruCache(object):
    """ Lru cache class. """
    def __init__(self, cache_size=100000, load_factor=1, dtype=tf.uint32):
        self._cache_size = cache_size
        self._load_factor = load_factor
        self._dtype = dtype
        self._cache = gen_npu_cpu_ops.lru_cache(
            cache_size=self._cache_size,
            load_factor=self._load_factor,
            dtype=self._dtype
        )

    ## 提供CacheAdd功能
    # @param cache resource类型，保存lruCache资源
    # @param ids int类型，输入索引
    # @return result 对ids执行完swap in/swap out操作后输出的索引张量
    def cache_add(self, ids):
        """ Add caches. """
        result = gen_npu_cpu_ops.cache_add(
            cache=self._cache,
            ids=ids
        )
        return result

    ## 提供CacheRemoteIndexToLocal功能
    # @param cache resource类型，保存lruCache资源
    # @param ids int类型，输入索引
    # return result 输入ids对应cache中的索引
    def cache_remote_index_to_local(self, ids):
        """ Cahce remote index to local. """
        result = gen_npu_cpu_ops.cache_remote_index_to_local(
            cache=self._cache,
            ids=ids
        )
        return result

    ## 提供CacheAllIndexToLocal功能
    # @param cache resource类型，保存lruCache资源
    # return result 输出cache中所有的id
    def cache_all_index_to_local(self):
        """
        cache all index to local
        """
        result = gen_npu_cpu_ops.cache_all_index_to_local(
            cache=self._cache,
            dtype=self._dtype
        )
        return result
