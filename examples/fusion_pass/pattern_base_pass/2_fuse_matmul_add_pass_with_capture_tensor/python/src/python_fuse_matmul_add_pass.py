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
"""Python PatternFusionPass aligned with C++ FuseMatMulAndAddPass (capture tensor sample)."""

from __future__ import annotations

from ge.es.graph_builder import GraphBuilder
from ge.graph.types import DataType
from ge.passes import (
    PassStage,
    PatternFusionPass,
    create_pattern,
    create_replacement,
    register_fusion_pass,
)

try:
    from ge.es.math import GEMM, Add, MatMul
except ImportError:
    try:
        from ge.es.all import GEMM, Add, MatMul
    except ImportError:
        MatMul = None
        Add = None
        GEMM = None
try:
    from ge.es.nn import BatchMatMulV2
except ImportError:
    try:
        from ge.es.all import BatchMatMulV2
    except ImportError:
        BatchMatMulV2 = None
_K_MATMUL_CAPTURE_IDX = 0
_K_ADD_CAPTURE_IDX = 1


def _require_es_apis() -> None:
    pairs = [
        ("MatMul", MatMul),
        ("Add", Add),
        ("GEMM", GEMM),
        ("BatchMatMulV2", BatchMatMulV2),
    ]
    missing = [name for name, obj in pairs if obj is None]
    if missing:
        raise RuntimeError(
            "未找到 ES API: "
            + ", ".join(missing)
            + "。请先 source CANN 环境；如仍缺失，请参考 README 的“ES API 缺失时处理（可选）”生成并加载 es_all 后重新执行。"
        )


@register_fusion_pass(
    name="PythonFuseMatMulAndAddCaptureTensorPass",
    stage=PassStage.BEFORE_INFER_SHAPE,
)
class PythonFuseMatMulAndAddCaptureTensorPass(PatternFusionPass):
    def patterns(self):
        print("Define pattern for FuseMatMulAndAddPass in capture tensor sample")
        _require_es_apis()
        pattern_builder0 = GraphBuilder("pattern0")
        a0, b0, c0 = pattern_builder0.create_inputs(3)
        matmul0 = MatMul(a0, b0)
        add0 = Add(matmul0, c0)
        pat0 = create_pattern(pattern_builder0.build_and_reset([add0]))
        pat0.capture_tensor(matmul0).capture_tensor(add0)
        pattern_builder1 = GraphBuilder("pattern1")
        a1, b1, c1 = pattern_builder1.create_inputs(3)
        matmul1 = BatchMatMulV2(a1, b1)
        add1 = Add(matmul1, c1)
        pat1 = create_pattern(pattern_builder1.build_and_reset([add1]))
        pat1.capture_tensor(matmul1).capture_tensor(add1)
        return [pat0, pat1]

    def meet_requirements(self, match_result):
        print("Define MeetRequirements for FuseMatMulAndAddPass in capture tensor sample")
        add_io = match_result.get_captured_tensor(_K_ADD_CAPTURE_IDX)
        add_node = add_io.node
        add_input0_dt = add_node.get_input_desc(0).get_data_type()
        add_input1_dt = add_node.get_input_desc(1).get_data_type()
        if add_input0_dt != DataType.DT_FLOAT or add_input1_dt != DataType.DT_FLOAT:
            print(
                f"Only support Add inputs are {DataType.DT_FLOAT.name}, "
                f"got input0={add_input0_dt.name}, input1={add_input1_dt.name}"
            )
            return False
        return True

    def replacement(self, match_result):
        print("Define replacement for FuseMatMulAndAddPass in capture tensor sample")
        _require_es_apis()
        matmul_io = match_result.get_captured_tensor(_K_MATMUL_CAPTURE_IDX)
        mnode = matmul_io.node
        transpose_a = False
        transpose_b = False
        try:
            transpose_a = bool(mnode.get_attr("transpose_x1"))
        except RuntimeError:
            pass
        try:
            transpose_b = bool(mnode.get_attr("transpose_x2"))
        except RuntimeError:
            pass
        replace_builder = GraphBuilder("replacement")
        r_a, r_b, r_c = replace_builder.create_inputs(3)
        alpha_const = replace_builder.create_scalar_float(1.0)
        beta_const = replace_builder.create_scalar_float(1.0)
        gemm = GEMM(
            r_a,
            r_b,
            r_c,
            alpha_const,
            beta_const,
            transpose_a=transpose_a,
            transpose_b=transpose_b,
        )
        return create_replacement(replace_builder.build_and_reset([gemm]))


if __name__ == "__main__":
    print("PythonFuseMatMulAndAddCaptureTensorPass 已注册。")
    print("请通过 ASCEND_GE_PY_PASS_PATH 指向本文件，例如：")
    print("  export ASCEND_GE_PY_PASS_PATH=$(pwd)/src/python_fuse_matmul_add_pass.py")
