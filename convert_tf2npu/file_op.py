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

"""Operations for file processing"""

import os
import re
import shutil
import util_global
import pandas as pd
from visit_by_ast import get_tf_enume
from visit_by_ast import get_unsupport_api
from log import logger_failed_report
from log import logger_api_brief_report

def before_clear():
    """Operations before clear"""
    exit_folder = os.path.exists(util_global.get_value('output'))
    if exit_folder:
        shutil.rmtree(util_global.get_value('output'))
    exit_folder = os.path.exists(util_global.get_value('report'))
    if exit_folder:
        shutil.rmtree(util_global.get_value('report'))


def mkdir(path):
    """Create new directory"""
    folder = os.path.exists(path)
    if not folder:
        os.makedirs(path)


def mkdir_and_copyfile(srcfile, dstpath, file_name):
    """Create directory and copy files"""
    mkdir(dstpath)
    shutil.copyfile(os.path.join(srcfile, file_name), os.path.join(dstpath, file_name))


def write_output_after_conver(out_file, dst_content):
    """Write content to output file"""
    with open(out_file, 'w') as f:
        f.write(dst_content)


def write_report_after_conver(new_file_path, report_file, dst_content):
    """Write content to report file"""
    mkdir(new_file_path)
    with open(os.path.join(new_file_path, report_file), 'w') as f:
        f.write(dst_content)


def get_bit_val(value, index):
    """Return 0 or 1 based on value"""
    if value & (1 << index):
        return 1
    return 0


def write_report_terminator(content):
    """Write content to report and update global variable"""
    report_path = util_global.get_value('report')
    value = util_global.get_value('report_file_status')
    times = value.bit_length()
    while times > 0:
        if get_bit_val(value, times - 1):
            file = util_global.get_value('report_file')[times - 1]
            if os.path.exists(os.path.join(report_path, file)):
                with open(os.path.join(report_path, file), 'a') as file:
                    file.write(content)
                    file.write("\r\n")
                    file.write("\r\n")
        times = times - 1
    util_global.set_value('report_file_status', 0)


def write_conver_report(content, file):
    """Add content to existed report file"""
    report_path = util_global.get_value('report')
    mkdir(report_path)
    with open(os.path.join(report_path, file), 'a') as f:
        f.write(content)
        f.write("\r\n")

def check_warning(lineno, api_msg):
    """Raise warning when api is related to element range check"""
    pattern = r'tf.*.is_finite'
    if re.match(pattern, api_msg):
        doc_msg = "{}, chapter: {}".format('"Tensorflow?????????????????????', '"tf.is_finite??????????????????" and "Loss Scale"')
        content = "".join([util_global.get_value('path', ''), ":", str(lineno), ", You used tensorflow api: ",
                           api_msg, ", It is suggested to use npu api. Please refer to the online document: ",
                           doc_msg])
        os.system("cd .")
        print("".join(["\033[1;33mWARNING\033[0m:", content]), flush=True)
        logger_failed_report.info(content)

def log_failed_api(lineno, api_msg, is_third_party):
    """Log message for NPU unsupported APIs"""
    os.system("cd .")
    if is_third_party:
        content = "".join([util_global.get_value('path', ''), ":", str(lineno), ", NPU Unsupport API: ", api_msg,
                           ", Please modify user scripts manually."])
        print("".join(["\033[1;31mERROR\033[0m:", content]), flush=True)

    elif api_msg.startswith("hvd"):
        doc_msg = "{}, chapter: {}".format('"Tensorflow?????????????????????', '"Horovod??????????????????"')
        content = "".join([util_global.get_value('path', ''), ":", str(lineno), ", NPU Unsupport API: ", api_msg,
                           ", Please refer to the online document: ", doc_msg])
        print("".join(["\033[1;33mWARNING\033[0m:", content]), flush=True)

    elif api_msg.startswith("tf.is_"):
        doc_msg = "{}, chapter: {}".format('"Tensorflow?????????????????????', '"tf.is_finite??????????????????" and "Loss Scale"')
        content = "".join([util_global.get_value('path', ''), ":", str(lineno), ", NPU Unsupport API: ", api_msg,
                           ", Please refer to the online document: ", doc_msg])
        print("".join(["\033[1;33mWARNING\033[0m:", content]), flush=True)

    else:
        content = "".join([util_global.get_value('path', ''), ":", str(lineno), ", NPU Unsupport API: ", api_msg])
        print("".join(["\033[1;31mERROR\033[0m:", content]), flush=True)
    logger_failed_report.info(content)


def abs_join(abs1, abs2):
    """Join path abs1 and abs2"""
    abs2 = os.fspath(abs2)
    abs2 = os.path.splitdrive(abs2)[1]
    abs2 = abs2.strip('\\/') or abs2
    return os.path.join(abs1, abs2)


def scan_file(path, file_name, api, lineno):
    """Scan script file to generate analysis report"""
    api_list = pd.read_excel(util_global.get_value('list'), sheet_name=0)
    api_module = api_list['?????????'].values.tolist()
    api_name = api_list['API???'].values.tolist()
    api_support = api_list['????????????API?????????'].values.tolist()
    api_advice = api_list['??????'].values.tolist()

    script_name = []
    code_line = []
    code_module = []
    code_api = []
    support_type = []
    migrate_advice = []

    for i in range(len(api)):
        name = api[i]
        if name in api_name:
            script_name.append(file_name)
            code_api.append(name)
            code_line.append(lineno[i])
            code_module.append(api_module[api_name.index(name)])
            support_type.append(api_support[api_name.index(name)])
            migrate_advice.append(api_advice[api_name.index(name)])

            api_support_type = api_support[api_name.index(name)]
            # print warning message of npu supported api
            if api_support_type in ('????????????????????????', '?????????'):
                check_warning(lineno[i], name)

            # print error message when api is unsupported on npu
            if api_support_type in ('?????????????????????????????????????????????', '??????????????????????????????????????????????????????'):
                log_failed_api(lineno[i], name, is_third_party=False)

    # search for tf enumeration
    enume_list = pd.read_excel(util_global.get_value('list'), sheet_name=1)
    enume_name = enume_list['API???'].values.tolist()
    (enume, lineno) = get_tf_enume(os.path.join(path, file_name), enume_name)

    for i in range(len(enume)):
        name = enume[i]
        class_name = '.'.join(name.split('.')[:-1])
        if name not in code_api and class_name not in code_api:
            if class_name in api_name:
                script_name.append(file_name)
                code_api.append(class_name)
                code_line.append(lineno[i])
                code_module.append(api_module[api_name.index(class_name)])
                support_type.append(api_support[api_name.index(class_name)])
                migrate_advice.append(api_advice[api_name.index(class_name)])

                # print error message when api is unsupported on npu
                api_support_type = api_support[api_name.index(class_name)]
                if api_support_type in ('?????????????????????????????????????????????', '??????????????????????????????????????????????????????'):
                    log_failed_api(lineno[i], class_name, is_third_party=False)

    # record unsupported api
    (unsupport, unsupport_module, lineno) = get_unsupport_api(os.path.join(path, file_name))
    for i in range(len(unsupport)):
        script_name.append(file_name)
        code_api.append(unsupport[i])
        code_line.append(lineno[i])
        code_module.append(unsupport_module[i])
        support_type.append('??????????????????????????????????????????????????????')
        migrate_advice.append('????????????TF??????API???????????????')

        # print error message for unsupported api
        log_failed_api(lineno[i], unsupport[i], is_third_party=True)

    analyse_result = pd.DataFrame({'???????????????': script_name, '?????????': code_line,
                                   '?????????': code_module, 'API???': code_api,
                                   '????????????API?????????': support_type, '??????': migrate_advice})

    # when there are tf apis used in script, analysis report will be generated
    report = util_global.get_value('generate_dir_report')
    if script_name:
        report = report.append(analyse_result)
        util_global.set_value('generate_dir_report', report)


def adjust_index():
    """Adjust index column for DataFrame"""
    report = util_global.get_value('generate_dir_report')
    index_column = []
    for i in range(len(report)):
        index_column.append(i + 1)
    report.index = index_column
    report.index.name = '??????'
    util_global.set_value('generate_dir_report', report)


def get_api_statistic(analysis_report):
    """Calculate API statistics"""
    code_api = analysis_report['API???'].values.tolist()
    support_type = analysis_report['????????????API?????????'].values.tolist()

    # eliminate duplicated data
    eliminate_dup_api = []
    eliminate_dup_type = []
    for item in code_api:
        if not item in eliminate_dup_api:
            eliminate_dup_api.append(item)
            eliminate_dup_type.append(support_type[code_api.index(item)])

    # api statistics
    api_analysis = "1.In brief: Total API: {}, in which Support: {}, " \
                   "API support after migration: {}, " \
                   "Network training support after migration: {}, " \
                   "Not support but no impact on migration: {}, " \
                   "Not support or recommended: {}, " \
                   "Compatible: {}, " \
                   "Deprecated: {}, " \
                   "Analysing: {}".format(len(code_api),
                                          support_type.count('????????????????????????'),
                                          support_type.count('???????????????API????????????'),
                                          support_type.count('?????????????????????????????????'),
                                          support_type.count('???????????????????????????????????????????????????'),
                                          support_type.count('??????????????????????????????????????????????????????'),
                                          support_type.count('?????????'),
                                          support_type.count('?????????'),
                                          support_type.count('?????????????????????????????????????????????'))

    api_eliminate_dup = "2.After eliminate duplicate: Total API: {}, in which Support: {}, " \
                        "API support after migration: {}, " \
                        "Network training support after migration: {}, " \
                        "Not support but no impact on migration: {}, " \
                        "Not support or recommended: {}, " \
                        "Compatible: {}, " \
                        "Deprecated: {}, " \
                        "Analysing: {}".format(len(eliminate_dup_api),
                                               eliminate_dup_type.count('????????????????????????'),
                                               eliminate_dup_type.count('???????????????API????????????'),
                                               eliminate_dup_type.count('?????????????????????????????????'),
                                               eliminate_dup_type.count('???????????????????????????????????????????????????'),
                                               eliminate_dup_type.count('??????????????????????????????????????????????????????'),
                                               eliminate_dup_type.count('?????????'),
                                               eliminate_dup_type.count('?????????'),
                                               eliminate_dup_type.count('?????????????????????????????????????????????'))
    content = (api_analysis + '\n' + api_eliminate_dup)
    print(content)
    logger_api_brief_report.info(content)
