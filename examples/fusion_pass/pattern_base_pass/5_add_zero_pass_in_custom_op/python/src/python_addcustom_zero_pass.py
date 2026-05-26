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

"""Python PatternFusionPass sample for AddCustomZeroPass."""

from math import fabs

from ge.es.graph_builder import GraphBuilder
from ge.graph import Tensor
from ge.graph.types import DataType
from ge.passes import (
    PassStage,
    PatternFusionPass,
    create_pattern,
    create_replacement,
    register_fusion_pass,
)

try:
    from ge.es.custom import AddCustom
except ImportError:
    AddCustom = None

try:
    from ge.es.math import Identity
except ImportError:
    Identity = None

try:
    from ge.es.math import TensorMove
except ImportError:
    TensorMove = None


def _require_es_custom_apis() -> None:
    if AddCustom is None:
        raise RuntimeError(
            "未找到 ge.es.custom.AddCustom。请先根据 python/README.md 生成 es_custom Python ES 包，"
            "并设置 PYTHONPATH 与 LD_LIBRARY_PATH。"
        )


def _require_es_apis() -> None:
    _require_es_custom_apis()
    if Identity is None:
        raise RuntimeError(
            "未找到 ge.es.math.Identity。请先 source CANN 环境，并确认本地 run 包中的 es_math 已正确安装。"
        )
    if TensorMove is None:
        raise RuntimeError(
            "未找到 ge.es.math.TensorMove。请先 source CANN 环境，并确认本地 run 包中的 es_math 已正确安装。"
        )


def _extract_first_scalar_value(value):
    while isinstance(value, list):
        if len(value) == 0:
            return None
        value = value[0]
    return value


def _is_tensor_value_equal_to_zero(tensor: Tensor):
    scalar_value = _extract_first_scalar_value(tensor.data)
    if scalar_value is None:
        return False

    if tensor.data_type == DataType.DT_FLOAT:
        return fabs(float(scalar_value)) < 1e-6
    if tensor.data_type == DataType.DT_DOUBLE:
        return fabs(float(scalar_value)) < 1e-15
    if tensor.data_type == DataType.DT_INT32:
        return int(scalar_value) == 0
    if tensor.data_type == DataType.DT_FLOAT16:
        return fabs(float(scalar_value)) < 1e-3
    return False


def _build_pattern0():
    pattern_builder = GraphBuilder("addcustom_zero_pattern0")
    input_tensor = pattern_builder.create_input(0)
    other_tensor = pattern_builder.create_input(1)
    const_tensor = pattern_builder.create_const_float(0.0)
    identity_tensor = Identity(const_tensor)
    add_zero_tensor = AddCustom(input_tensor, identity_tensor)
    add_tensor = AddCustom(add_zero_tensor, other_tensor)
    pattern_builder.set_graph_output(add_tensor, 0)
    return create_pattern(pattern_builder.build_and_reset())


def _build_pattern1():
    pattern_builder = GraphBuilder("addcustom_zero_pattern1")
    input_tensor = pattern_builder.create_input(0)
    other_tensor = pattern_builder.create_input(1)
    const_tensor = pattern_builder.create_const_float(0.0)
    moved_const_tensor = TensorMove(Identity(const_tensor))
    add_zero_tensor = AddCustom(input_tensor, moved_const_tensor)
    add_tensor = AddCustom(add_zero_tensor, other_tensor)
    pattern_builder.set_graph_output(add_tensor, 0)
    return create_pattern(pattern_builder.build_and_reset())


@register_fusion_pass(name="PythonAddCustomZeroPass", stage=PassStage.BEFORE_INFER_SHAPE)
class PythonAddCustomZeroPass(PatternFusionPass):
    """Recognize AddCustom(AddCustom(x, zero), y) and replace it with AddCustom(x, y)."""

    def patterns(self):
        print("Define pattern for PythonAddCustomZeroPass")
        _require_es_apis()
        return [_build_pattern0(), _build_pattern1()]

    def meet_requirements(self, match_result):
        print("Define MeetRequirements for PythonAddCustomZeroPass")
        for node in match_result.get_matched_nodes():
            if node.type != "Const":
                continue
            const_tensor = node.get_attr("value")
            is_zero = _is_tensor_value_equal_to_zero(const_tensor)
            print(
                f"[PythonAddCustomZeroPass] matched={match_result.get_pattern_graph_name()} "
                f"const_dtype={const_tensor.data_type.name} zero={is_zero}"
            )
            if not is_zero:
                return False
        return True

    def replacement(self, match_result):
        print("Define replacement for PythonAddCustomZeroPass")
        _require_es_custom_apis()

        replacement_builder = GraphBuilder("addcustom_zero_replacement")
        input_tensor = replacement_builder.create_input(0)
        other_tensor = replacement_builder.create_input(1)
        add_tensor = AddCustom(input_tensor, other_tensor)
        replacement_builder.set_graph_output(add_tensor, 0)
        return create_replacement(replacement_builder.build_and_reset())


if __name__ == "__main__":
    print("PythonAddCustomZeroPass 已注册。")
    print("运行前请先生成 es_custom，并设置 PYTHONPATH 与 LD_LIBRARY_PATH。")
    print("请通过 ASCEND_GE_PY_PASS_PATH 指向本文件，例如：")
    print("  export ASCEND_GE_PY_PASS_PATH=$PWD/src/python_addcustom_zero_pass.py")
