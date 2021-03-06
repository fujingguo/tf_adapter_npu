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

"""NPU distributed strategy"""

import os
from tensorflow.python.distribute import distribute_lib
from tensorflow.python.distribute import one_device_strategy

from hccl.manage.api import get_rank_size
from hccl.manage.api import get_rank_id


class NPUExtended(one_device_strategy.OneDeviceExtended):
    """NPU implemented oneDevice strategy"""
    def __init__(self, container_strategy, device):
        super(NPUExtended, self).__init__(container_strategy, device)

    def _experimental_distribute_dataset(self, dataset):
        return dataset.shard(get_rank_size(), get_rank_id())

    @property
    def _num_replicas_in_sync(self):
        rank_size = os.getenv("RANK_SIZE", "1")
        return int(rank_size)


class NPUStrategy(distribute_lib.StrategyV1):
    """NPU distribute strategy"""
    def __init__(self, device="/cpu:0"):
        super(NPUStrategy, self).__init__(NPUExtended(self, device))
