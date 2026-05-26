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

"""Python PatternFusionPass sample equivalent to the C++ AddZeroPass."""

from math import fabs

from ge.es.graph_builder import GraphBuilder
from ge.graph.types import DataType
from ge.passes import (
    PassStage,
    PatternFusionPass,
    capture_tensor,
    create_pattern,
    create_replacement,
    register_fusion_pass,
)


def _extract_scalar_value(value):
    while isinstance(value, list):
        if len(value) != 1:
            return None
        value = value[0]
    return value


def _is_tensor_value_equal_to_zero(tensor):
    scalar_value = _extract_scalar_value(tensor.data)
    if scalar_value is None:
        return False

    if tensor.data_type == DataType.DT_FLOAT:
        return fabs(float(scalar_value)) < 1e-6
    if tensor.data_type == DataType.DT_DOUBLE:
        return fabs(float(scalar_value)) < 1e-15
    if tensor.data_type == DataType.DT_INT32:
        return int(scalar_value) == 0
    return False


@register_fusion_pass(name="PythonAddZeroPass", stage=PassStage.BEFORE_INFER_SHAPE)
class PythonAddZeroPass(PatternFusionPass):
    """Recognize Add(x, 0) and replace it with x using explicit Const validation."""

    def patterns(self):
        pattern_builder = GraphBuilder("add_zero_pattern")
        input_tensor = pattern_builder.create_input(0)
        placeholder_const = pattern_builder.create_const_int32(0)
        add_tensor = input_tensor + placeholder_const
        pattern_builder.set_graph_output(add_tensor, 0)

        pattern = create_pattern(pattern_builder.build_and_reset())
        pattern.capture_tensor(capture_tensor(input_tensor))
        return [pattern]

    def meet_requirements(self, match_result):
        captured_input = match_result.get_captured_tensor(0)
        for node in match_result.get_matched_nodes():
            if node.type != "Const":
                continue
            const_tensor = node.get_attr("value")
            is_zero = _is_tensor_value_equal_to_zero(const_tensor)
            print(
                f"[PythonAddZeroPass] matched={match_result.get_pattern_graph_name()} "
                f"captured={captured_input.node.name}:{captured_input.index} "
                f"const_dtype={const_tensor.data_type.name} zero={is_zero}"
            )
            return is_zero
        return True

    def replacement(self, match_result):
        replacement_builder = GraphBuilder("add_zero_replacement")
        passthrough = replacement_builder.create_input(0)
        replacement_builder.set_graph_output(passthrough, 0)
        return create_replacement(replacement_builder.build_and_reset())


if __name__ == "__main__":
    print("PythonAddZeroPass 已注册。")
    print("请通过 ASCEND_GE_PY_PASS_PATH 指向本文件，例如：")
    print("  export ASCEND_GE_PY_PASS_PATH=$PWD/src/python_add_zero_pass.py")
