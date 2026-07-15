#!/bin/bash
# ----------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------

# MmoeBmmSplitPass 一键式验证脚本
# 用法: ./quick_verify.sh [experts]

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PASS_DIR=$(dirname "$SCRIPT_DIR")

if [ -n "$ASCEND_HOME_PATH" ]; then
    CANN_DIR=$ASCEND_HOME_PATH
else
    CANN_DIR=/usr/local/Ascend/cann
fi

EXPERTS=${1:-4}

cleanup() {
    installed_so=${CANN_DIR}/opp/vendors/pass_so_dir/custom_fusion_passes/libmmoe_bmm_split_pass.so
    if [ -f "$installed_so" ] && [ -w "$installed_so" ]; then
        rm -f "$installed_so"
        echo "  [OK] CANN安装的pass so已清理"
    elif [ -f "$installed_so" ]; then
        echo "  [WARN] CANN安装的pass so需要sudo权限清理，跳过（可手动执行: sudo rm $installed_so）"
    fi
}

trap cleanup EXIT

echo "========================================"
echo "MmoeBmmSplitPass 快速验证"
echo "========================================"
echo ""
echo "【测试配置】"
echo "  experts=$EXPERTS (m=64, k1=128, n1=256, n2=128)"
echo ""

# ========================================
# 步骤0：检查Pass编译状态
# ========================================
echo "========================================"
echo "步骤0：检查Pass编译状态"
echo "========================================"
echo ""

if [ ! -f ${PASS_DIR}/build/libmmoe_bmm_split_pass.so ]; then
    echo "  Pass未编译，开始自动编译..."
    echo ""

    cd ${PASS_DIR}
    if [ ! -d build ]; then
        mkdir -p build
    fi
    cd build

    echo "  【执行cmake】"
    cmake .. > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "  [FAIL] cmake失败"
        cd ${SCRIPT_DIR}
        exit 1
    fi
    echo "  [OK] cmake成功"

    echo "  【执行make】"
    make -j8 > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "  [FAIL] make失败"
        cd ${SCRIPT_DIR}
        exit 1
    fi
    echo "  [OK] make成功"

    echo "  【执行make install】"
    make install > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "  [FAIL] make install失败"
        cd ${SCRIPT_DIR}
        exit 1
    fi
    echo "  [OK] make install成功"

    echo ""
    echo "  [OK] Pass编译完成"
    echo ""
    cd ${SCRIPT_DIR}
else
    echo "  Pass已编译，执行make install确保最新..."
    cd ${PASS_DIR}/build
    make install > /dev/null 2>&1
    cd ${SCRIPT_DIR}
    echo "  [OK] Pass已就绪"
    echo ""
fi

# ========================================
# 步骤1：生成ONNX模型
# ========================================
echo "========================================"
echo "步骤1：生成ONNX模型"
echo "========================================"

source ${CANN_DIR}/set_env.sh

python3 gen_onnx.py --experts $EXPERTS

if [ ! -f model.onnx ]; then
    echo "  [FAIL] ONNX模型生成失败"
    exit 1
fi
echo "  [OK] model.onnx已生成"
echo ""

# ========================================
# 步骤2：ATC编译OM模型（开启融合pass）
# ========================================
echo "========================================"
echo "步骤2：ATC编译OM模型（开启融合pass）"
echo "========================================"

rm -f *.om
rm -f ge_proto_*.txt

echo "  编译融合模型..."
export DUMP_GE_GRAPH=1
# soc_version需根据实际芯片型号修改，可通过 npu-smi info 命令查看
atc --model=./model.onnx --framework=5 --output=./model_fused --soc_version=Ascend910_9362 2>&1 | grep -E "MmoeBmmSplitPass|success|fail" | tail -10

if [ ! -f model_fused.om ]; then
    echo "  [FAIL] OM模型编译失败"
    exit 1
fi
echo "  [OK] model_fused.om已生成"
echo ""

# ========================================
# 步骤3：检查融合效果（dump图验证）
# ========================================
echo "========================================"
echo "步骤3：检查融合效果（dump图验证）"
echo "========================================"
echo ""

# 查找PreRunBegin图（融合前）
pre_run_file=$(ls ge_proto_*_PreRunBegin.txt 2>/dev/null | head -1)
if [ -n "$pre_run_file" ]; then
    echo "  【融合前图】 $pre_run_file"
    matmul_before=$(grep -c 'type: ".*MatMul' "$pre_run_file" 2>/dev/null || true)
    matmul_before=${matmul_before:-0}
    echo "  MatMul/BatchMatMul节点数: $matmul_before (预期: $((EXPERTS * 2)))"
    if [ "$matmul_before" -lt "$((EXPERTS * 2))" ]; then
        echo "  [FAIL] 融合前图MatMul节点数($matmul_before)少于预期($((EXPERTS * 2)))，当前模型不符合本样例验证目标"
        exit 1
    fi
else
    echo "  [FAIL] 未找到PreRunBegin dump图"
    exit 1
fi

# 查找RunCustomPass_AfterInferShape图（融合后）
after_fusion_file=$(ls ge_proto_*_RunCustomPass_AfterInferShape.txt 2>/dev/null | head -1)
if [ -n "$after_fusion_file" ]; then
    echo ""
    echo "  【融合后图】 $after_fusion_file"
    matmul_after=$(grep -c 'type: ".*MatMul' "$after_fusion_file" 2>/dev/null || true)
    matmul_after=${matmul_after:-0}
    pack_count=$(grep -c 'type: "Pack"' "$after_fusion_file" 2>/dev/null || true)
    pack_count=${pack_count:-0}
    bmm_count=$(grep -c 'type: "BatchMatMul' "$after_fusion_file" 2>/dev/null || true)
    bmm_count=${bmm_count:-0}
    split_count=$(grep -c 'type: "Split"' "$after_fusion_file" 2>/dev/null || true)
    split_count=${split_count:-0}
    squeeze_count=$(grep -c 'type: "Squeeze"' "$after_fusion_file" 2>/dev/null || true)
    squeeze_count=${squeeze_count:-0}
    echo "  MatMul/BatchMatMul节点数: $matmul_after (预期: $((EXPERTS + 1))，首层$EXPERTS个+合并后1个)"
    echo "  Pack节点数: $pack_count (预期: 2)"
    echo "  BatchMatMul节点数: $bmm_count (预期: $((EXPERTS + 1)))"
    echo "  Split节点数: $split_count (预期: 1)"
    echo "  Squeeze节点数: $squeeze_count (预期: $EXPERTS)"

    if [ "$matmul_after" -le "$((EXPERTS + 1))" ] && [ "$pack_count" -ge 2 ] && [ "$split_count" -ge 1 ] && [ "$squeeze_count" -ge "$EXPERTS" ]; then
        echo ""
        echo "  [PASS] 融合验证通过：非首层 $EXPERTS 个MatMul已被合并为 Pack+BatchMatMul+Split+Squeeze"
    else
        echo ""
        echo "  [FAIL] 融合验证失败：未发现预期的替换结构"
        exit 1
    fi
else
    echo "  [FAIL] 未找到RunCustomPass_AfterInferShape dump图"
    exit 1
fi
echo ""

echo "[DONE] 验证完成"
