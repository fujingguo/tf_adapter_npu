#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2020 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless REQUIRED by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ============================================================================
import json
import time

def _init():
    global _global_dict
    _global_dict = {}
    with open('mappings/ast.json') as f:
        load_dict = json.load(f)
        items = load_dict.items()
        for key, value in items:
            set_value(key, value)
    value = "_npu_" + time.strftime('%Y%m%d%H%M%S')
    set_value('timestap', value)

def set_value(key, value):
    _global_dict[key] = value

def get_value(key, def_value = None):
    try:
        return _global_dict[key]
    except KeyError:
        return def_value
