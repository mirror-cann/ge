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

from ge.graph import Tensor
from ge.graph.types import DataType
from ge.passes import (
    PassStage,
    PatternFusionPass,
    pattern,
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
            "未找到 ge.es.custom.AddCustom。请先参考 README 的使用方式生成 es_custom Python ES 包，"
            "设置 PYTHONPATH 与 LD_LIBRARY_PATH 后重新执行。"
        )


def _require_es_apis() -> None:
    _require_es_custom_apis()
    if Identity is None:
        raise RuntimeError(
            "未找到 ge.es.math.Identity。请先 source CANN 环境；如仍缺失，请参考 README 的“ES API 说明”后重新执行。"
        )
    if TensorMove is None:
        raise RuntimeError(
            "未找到 ge.es.math.TensorMove。请先 source CANN 环境；如仍缺失，请参考 README 的“ES API 说明”后重新执行。"
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


@register_fusion_pass(name="PythonAddCustomZeroPass", stage=PassStage.BEFORE_INFER_SHAPE)
class PythonAddCustomZeroPass(PatternFusionPass):
    """Recognize AddCustom(AddCustom(x, zero), y) and replace it with AddCustom(x, y)."""

    @pattern
    def addcustom_zero(self, inputs):
        print("Define pattern for PythonAddCustomZeroPass")
        _require_es_apis()
        x, y = inputs[:2]
        builder = x.get_owner_builder()
        const_tensor = builder.create_const_float(0.0)
        identity_tensor = Identity(const_tensor)
        add_zero_tensor = AddCustom(x, identity_tensor)
        return AddCustom(add_zero_tensor, y)

    @pattern
    def addcustom_zero_with_tensormove(self, inputs):
        print("Define pattern for PythonAddCustomZeroPass")
        _require_es_apis()
        x, y = inputs[:2]
        builder = x.get_owner_builder()
        const_tensor = builder.create_const_float(0.0)
        moved_const_tensor = TensorMove(Identity(const_tensor))
        add_zero_tensor = AddCustom(x, moved_const_tensor)
        return AddCustom(add_zero_tensor, y)

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

    def replacement(self, inputs):
        print("Define replacement for PythonAddCustomZeroPass")
        _require_es_custom_apis()
        x, y = inputs[:2]
        return AddCustom(x, y)


if __name__ == "__main__":
    print("PythonAddCustomZeroPass 已注册。")
    print("运行前请先生成 es_custom，并设置 PYTHONPATH 与 LD_LIBRARY_PATH。")
    print("请通过 ASCEND_GE_PY_PASS_PATH 指向本文件，例如：")
    print("  export ASCEND_GE_PY_PASS_PATH=$PWD/src/python_addcustom_zero_pass.py")
