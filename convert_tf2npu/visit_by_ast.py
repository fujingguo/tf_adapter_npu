#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
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

"""Basic class to visit ast node"""

import ast
import util_global


class VisitCall(ast.NodeVisitor):
    """Class for visiting python ast call node"""
    def __init__(self):
        self._in_call = False
        self._current_visit = []
        self.call_nodes = []
        self.line_nos = []

    def visit_Call(self, node):
        """Visit call node"""
        self._in_call = True
        self._current_visit = []
        self.generic_visit(node)

    def visit_Attribute(self, node):
        """Visit attr node"""
        if self._in_call:
            self._current_visit.append(node.attr)
        self.generic_visit(node)

    def visit_Name(self, node):
        """Visit name in call node"""
        if self._in_call:
            self._current_visit.append(node.id)
            self.call_nodes.append('.'.join(self._current_visit[::-1]))
            self.line_nos.append(getattr(node, "lineno", "None"))
            # Reset the state
            self._in_call = False
            self._current_visit = []
        self.generic_visit(node)


class VisitAttr(ast.NodeVisitor):
    """Class for visiting python ast attr node"""
    def __init__(self):
        self._in_attr = False
        self._current_visit = []
        self.attr_nodes = []
        self.line_nos = []

    def visit_Attribute(self, node):
        """Visit attr node"""
        self._in_attr = True
        self._current_visit.append(node.attr)
        self.generic_visit(node)

    def visit_Name(self, node):
        """Visit name in attr node"""
        if self._in_attr:
            self._current_visit.append(node.id)
            self.attr_nodes.append('.'.join(self._current_visit[::-1]))
            self.line_nos.append(getattr(node, "lineno", "None"))
            # Reset the state
            self._in_attr = False
            self._current_visit = []
        self.generic_visit(node)


class VisitUnsupportImport(ast.NodeVisitor):
    """Class for visiting python ast import node"""
    def __init__(self):
        self.imports = []
        self.import_modules = []
        self.modules = []
        self.unsupport = ['cupy', 'cupyx', 'pynvml']

    def visit_ImportFrom(self, node):
        """Visit importfrom node"""
        if node.module is not None:
            self.modules = node.module.split('.')
        for value in node.names:
            if isinstance(value, ast.alias):
                classes = value.name.split('.')
                if len(self.modules) == 0:
                    break
                if self.modules[0] in self.unsupport:
                    self.imports.append(classes[0])
                    self.import_modules.append(self.modules[0])
        self.generic_visit(node)

    def visit_Import(self, node):
        """Visit import node"""
        for value in node.names:
            if isinstance(value, ast.alias):
                self.modules = value.name.split('.')
                if len(self.modules) == 0:
                    break
                if self.modules[0] in self.unsupport:
                    self.import_modules.append(self.modules[0])
                    if value.asname is not None:
                        self.imports.append(value.asname)
                    else:
                        self.imports.append(self.modules[0])
        self.generic_visit(node)


def get_tf_api(file_name):
    """
    Args:
        file_name: script file

    Returns:
        list objects containing all the tensorflow apis extracted from file_name
        and corrosponding line number
    """
    util_global.set_value('use_keras_dropout', False)
    with open(file_name, 'r', encoding='utf-8') as file:
        source = file.read()
    tree = ast.parse(source)
    visitor = VisitCall()
    visitor.visit(tree)

    # get tensorflow related api
    api = []
    lineno = []
    import_list = ['tf', 'hvd']
    keras_dropout_api = ['tf.layers.dropout', 'tf.layers.Dropout', 'tf.keras.layers.Dropout',
                         'tf.keras.layers.SpatialDropout1D', 'tf.keras.layers.SpatialDropout2D',
                         'tf.keras.layers.SpatialDropout3D']
    for module in import_list:
        for i in range(len(visitor.call_nodes)):
            if "".join([module, '.']) in visitor.call_nodes[i] and visitor.call_nodes[i].split('.')[0] == module:
                api.append(visitor.call_nodes[i])
                lineno.append(visitor.line_nos[i])

            # get tf api using keras dropout
            if visitor.call_nodes[i] in keras_dropout_api:
                util_global.set_value('use_keras_dropout', True)
    return api, lineno


def get_tf_enume(file_name, enume_list):
    """
    Args:
        file_name: script file
        enume_list: list of all the Tensorflow enumeration apis

    Returns:
        list objects containing all the tensorflow enumeration apis extracted from file_name
        and corrosponding line number
    """
    with open(file_name, 'r', encoding='utf-8') as file:
        source = file.read()
    tree = ast.parse(source)
    visitor = VisitAttr()
    visitor.visit(tree)

    # get tensorflow enume api
    api = []
    lineno = []
    for i in range(len(visitor.attr_nodes)):
        if visitor.attr_nodes[i] in enume_list:
            api.append(visitor.attr_nodes[i])
            lineno.append(visitor.line_nos[i])
    return api, lineno


def get_unsupport_api(file_name):
    """
    Args:
        file_name: script file

    Returns:
        list objects containing the unsupport apis and module extracted from file_name
        and corrosponding line number
    """
    with open(file_name, 'r', encoding='utf-8') as file:
        source = file.read()
    tree = ast.parse(source)
    visitor = VisitCall()
    visitor.visit(tree)
    unsupportor = VisitUnsupportImport()
    unsupportor.visit(tree)

    # get unsupport api
    api = []
    lineno = []
    module = []
    for i in range(len(visitor.call_nodes)):
        imports = visitor.call_nodes[i].split('.')[0]
        if imports in unsupportor.imports or visitor.call_nodes[i].startswith('nvml'):
            if visitor.call_nodes[i].startswith('nvml'):
                module.append('pynvml')
            else:
                index = unsupportor.imports.index(imports)
                module.append(unsupportor.import_modules[index])
            api.append(visitor.call_nodes[i])
            lineno.append(visitor.line_nos[i])
    return api, module, lineno
