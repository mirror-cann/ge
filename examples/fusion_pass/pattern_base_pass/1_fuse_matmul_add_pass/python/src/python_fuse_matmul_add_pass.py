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

"""Python PatternFusionPass equivalent to C++ FuseMatMulAndAddPass (MatMul+Add / BatchMatMulV2+Add -> GEMM)."""

from __future__ import annotations

from ge.passes import (
    PassStage,
    PatternFusionPass,
    pattern,
    register_fusion_pass,
)

try:
    from ge.es.math import Add, GEMM, MatMul
except ImportError:
    try:
        from ge.es.all import Add, GEMM, MatMul
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


@register_fusion_pass(name="PythonFuseMatMulAndAddPass", stage=PassStage.BEFORE_INFER_SHAPE)
class PythonFuseMatMulAndAddPass(PatternFusionPass):

    @pattern
    def matmul_add(self, inputs):
        print("Define pattern for FuseMatMulAndAddPass")
        _require_es_apis()
        a, b, c = inputs[:3]
        return MatMul(a, b) + c

    @pattern
    def batch_matmul_add(self, inputs):
        print("Define pattern for FuseMatMulAndAddPass")
        _require_es_apis()
        a, b, c = inputs[:3]
        return BatchMatMulV2(a, b) + c

    def meet_requirements(self, match_result):
        return True

    def replacement(self, inputs):
        print("Define replacement for FuseMatMulAndAddPass")
        _require_es_apis()
        r_a, r_b, r_c = inputs[:3]
        return GEMM(r_a, r_b, r_c, 1.0, 1.0)


if __name__ == "__main__":
    print("PythonFuseMatMulAndAddPass 已注册。")
    print("请通过 ASCEND_GE_PY_PASS_PATH 指向本文件，例如：")
    print("  export ASCEND_GE_PY_PASS_PATH=$(pwd)/src/python_fuse_matmul_add_pass.py")
