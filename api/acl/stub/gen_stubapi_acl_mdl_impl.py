#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

import os
import re
import sys

PATTERN_FUNCTION = re.compile(r'ACL_FUNC_VISIBILITY\s+\n+.+\w+\([^();]*\);|.+\w+\([^();]*\);')
PATTERN_RETURN = re.compile(r'([^ ]+[ *])\w+\([^;]+;')

RETURN_STATEMENTS = {
    'aclDataType':          '    return ACL_DT_UNDEFINED;',
    'aclFormat':            '    return ACL_FORMAT_UNDEFINED;',
    'aclError':             '    return ACL_ERROR_API_NOT_SUPPORT;',
    'void':                 '',
    'size_t':               '    return static_cast<size_t>(0);',
    'uint8_t':              '    return static_cast<uint8_t>(0);',
    'int32_t':              '    return static_cast<int32_t>(0);',
    'uint32_t':             '    return static_cast<uint32_t>(0);',
    'int64_t':              '    return static_cast<int64_t>(0);',
    'uint64_t':             '    return static_cast<uint64_t>(0);',
    'aclFloat16':           '    return static_cast<aclFloat16>(0);',
    'float':                '    return 0.0f;',
    'bool':                 '    return false;',
    'double':               '    return static_cast<double>(0.0f);',
}


def collect_functions(file_path):
    signatures = []
    with open(file_path) as f:
        content = f.read()
        matches = PATTERN_FUNCTION.findall(content)
        for signature in matches:
            signatures.append(signature)
    return signatures


def implement_function(func):
    # remove ';'
    function_def = func[:len(func) - 1]
    function_def = re.sub(r'(\b\w+(?=\())', r'\1Impl', function_def, count=1)
    function_def += '\n'
    function_def += '{\n'
    m = PATTERN_RETURN.search(func)
    if m:
        ret_type = m.group(1).strip()
        if RETURN_STATEMENTS.__contains__(ret_type):
            function_def += RETURN_STATEMENTS[ret_type]
        else:
            if ret_type.endswith('*'):
                function_def += '    return nullptr;'
            else:
                raise RuntimeError("[ERROR] Unhandled return type: " + ret_type)
    else:
        function_def += '    return nullptr;'
    function_def += '\n'
    function_def += '}\n'
    return function_def


def generate_stub_file(acl_mdl_headers):
    mdl_impl_content = generate_function(acl_mdl_headers)
    return mdl_impl_content


def generate_function(header_files):
    includes = []
    includes.append('#include <stdio.h>\n')
    includes.append('#include <stdint.h>\n')
    includes.append('#include "model/acl_model_impl.h"\n')

    content = includes
    total = 0
    content.append('\n')
    # generate implement
    for header in header_files:
        header_basename = os.path.basename(header)
        content.append("// stub for {}\n".format(header_basename))
        functions = collect_functions(header)
        print("inc file:{}, functions numbers:{}".format(header_basename, len(functions)))
        total += len(functions)
        for func in functions:
            content.append("{}\n".format(implement_function(func)))
            content.append("\n")
    print("implement concent build success")
    print('total functions number is {}'.format(total))
    return content


def gen_code(out_stub_cpp, acl_mdl_headers):
    mdl_impl_content = generate_stub_file(acl_mdl_headers)
    print("mdl_impl_content have been generated")
    with open(out_stub_cpp, mode='w') as f:
        f.writelines(mdl_impl_content)

if __name__ == '__main__':
    out_stub_cpp = sys.argv[1]
    in_acl_mdl_header = sys.argv[2]
    in_acl_base_mdl_header = sys.argv[3]
    gen_code(out_stub_cpp, [in_acl_mdl_header, in_acl_base_mdl_header])
