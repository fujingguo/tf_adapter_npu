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
import os
import sys
import getopt
import util_global
from file_op import before_clear
from conver import conver

def para_check_and_set(argv):
    input_dir  = "npu_input"
    support_list = os.path.dirname(os.path.abspath(__file__)) + "/tf1.15_api_support_list.xlsx"
    output = "output" + util_global.get_value('timestap')
    report = "report" + util_global.get_value('timestap')
    report_suffix = report
    main_file = ""
    distributed_mode = ""

    try:
        opts, args = getopt.getopt(argv, "hi:l:o:r:m:d:", ["help", "input=", "list=", "output=", "report=", "main=", "distributed_mode"])
    except getopt.GetoptError:
        print('Parameter error, please check.')
        print('    this tool just support to convert tf-1.15 scripts.')
        print('    main.py -i <input> -l <list> -o <output> -r <report> -m <main>')
        print('or: main.py --input=<input> --list=<list> --output=<output> --report=<report> --main=<main>')
        print('-i or --input:  The source script to be converted.')
        print('-l or --list:  The list of supported api, Default value: tf1.15_api_support_list.xlsx')
        print('-o or --output: The destination script after converted, Default value: output_npu_***/')
        print('-r or --report: Conversion report, Default value: report_npu_***/')
        print('-m or --main: The executed entry *.py file, default:None')
        print('-d or --distributed_mode: The distribute mode to choose, including horovod distributed and tensorflow distributed strategy. '
              'the value should be one of [horovod, tf_strategy]')
        sys.exit(2)

    for opt, arg in opts:
        if opt in ("-h", "--help"):
            print('    this tool just support to convert tf-1.15 scripts.')
            print('    main.py -i <input> -l <list> -o <output> -r <report> -m <main>')
            print('or: main.py --input=<input> --list=<list> --output=<output> --report=<report> --main=<main>')
            print('-i or --input:  The source script to be converted')
            print('-l or --list:  The list of supported api, Default value: tf1.15_api_support_list.xlsx')
            print('-o or --output: The destination script after converted, Default value: output_npu_***/')
            print('-r or --report: Conversion report, Default value: report_npu_***/')
            print('-m or --main: The executed entry *.py file, default:None')
            print('-d or --distributed_mode: The distribute mode to choose, including horovod distributed and tensorflow distributed strategy. '
                  'the value should be one of [horovod, tf_strategy]')
            sys.exit()
        elif opt in ("-i", "--input"):
            input_dir = os.path.abspath(arg)
            if str(input_dir).endswith('/'):
                input_dir = input_dir[0:len(input_dir)-1]
            input_dir = input_dir.replace('\\', '/')
        elif opt in ("-l", "--list"):
            support_list = arg
        elif opt in ("-o", "--output"):
            output = os.path.abspath(arg)
            if str(output).endswith('/'):
                output = output[0:len(output)-1]
            output = output.replace('\\', '/')
        elif opt in ("-r", "--report"):
            report = os.path.abspath(arg)
            if str(report).endswith('/'):
                report = report[0:len(report)-1]
            report = os.path.join(report, report_suffix)
            report = report.replace('\\', '/')
        elif opt in ("-m", "--main"):
            if os.path.isfile(arg):
                main_file = os.path.abspath(arg)
                main_path = os.path.dirname(main_file)
                file = os.path.basename(main_file)
                main_path = main_path.replace('\\', '/')
                main_file = os.path.join(main_path, file)
            else:
                raise ValueError("--main args must be exited files")
        elif opt in ("-d", "--distributed_mode"):
            if arg not in ["horovod", "tf_strategy"]:
                raise ValueError("--distributed_mode or -d must be one of ['horovod', 'tf_strategy']")
            distributed_mode = arg

    if input_dir == "npu_input":
        raise ValueError("Please check -i or --input.")


    if input_dir + '/' in output + '/' or input_dir + '/' in report + '/':
        print("<output> or <report> could not be the subdirectory of <input>, please try another option.")
        sys.exit(2)

    util_global.set_value('input', input_dir)
    util_global.set_value('list', support_list)
    util_global.set_value('output', output)
    util_global.set_value('report', report)
    util_global.set_value('main', main_file)
    util_global.set_value('distributed_mode', distributed_mode)


if __name__ == "__main__":
    util_global._init()
    util_global.set_value('already_check_distributed_mode_arg', False)
    util_global.set_value('already_check_main_arg', False)
    para_check_and_set(sys.argv[1:])
    conver()