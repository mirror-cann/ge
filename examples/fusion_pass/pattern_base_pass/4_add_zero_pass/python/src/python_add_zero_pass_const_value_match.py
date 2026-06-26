#!/usr/bin/env python3
# -*- coding: UTF-8 -*-
# ----------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------

"""Python PatternFusionPass sample for strict Add(x, 0.0f) matcher-config matching."""

from ge.passes import (
    PassStage,
    PatternFusionPass,
    PatternMatcherConfigBuilder,
    pattern,
    register_fusion_pass,
)


@register_fusion_pass(name="PythonAddZeroConstValueMatchPass", stage=PassStage.BEFORE_INFER_SHAPE)
class PythonAddZeroConstValueMatchPass(PatternFusionPass):
    """Recognize Add(x, 0.0f) with strict const-value-match and replace it with x."""

    def __init__(self):
        super().__init__(PatternMatcherConfigBuilder().enable_const_value_match().build())

    @pattern
    def add_zero(self, inputs):
        return inputs[0] + 0.0

    def meet_requirements(self, match_result):
        captured_input = match_result.get_captured_tensor(0)
        print(
            f"[PythonAddZeroConstValueMatchPass] matched={match_result.get_pattern_graph_name()} "
            f"captured={captured_input.node.name}:{captured_input.index}"
        )
        return True

    def replacement(self, inputs):
        return inputs[0]


if __name__ == "__main__":
    print("PythonAddZeroConstValueMatchPass 已注册。")
    print("请通过 ASCEND_GE_PY_PASS_PATH 指向本文件，例如：")
    print("  export ASCEND_GE_PY_PASS_PATH=$PWD/src/python_add_zero_pass_const_value_match.py")
