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

# BmmToMulReduce Pass 一键式验证脚本
# 验证开启融合pass后BatchMatMulV2被替换为Mul+ReduceSumD
# 用法: ./quick_verify.sh [batch] [m] [k]

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PASS_DIR=$(dirname "$SCRIPT_DIR")

if [ -n "$ASCEND_HOME_PATH" ]; then
    CANN_DIR=$ASCEND_HOME_PATH
else
    CANN_DIR=/usr/local/Ascend/cann
fi

cleanup() {
    installed_so=${CANN_DIR}/opp/vendors/pass_so_dir/custom_fusion_passes/libbmm_to_mul_reduce_pass.so
    if [ -f "$installed_so" ] && [ -w "$installed_so" ]; then
        rm -f "$installed_so"
        echo "  ✓ CANN安装的pass so已清理"
    elif [ -f "$installed_so" ]; then
        echo "  ⚠ CANN安装的pass so需要sudo权限清理，跳过（可手动执行: sudo rm $installed_so）"
    fi
}

trap cleanup EXIT

# 默认参数
BATCH=${1:-100}
M=${2:-2333}
K=${3:-4}

echo "========================================"
echo "BmmToMulReduce Pass 快速验证"
echo "========================================"
echo ""
echo "【测试配置】"
echo "  Shape: batch=$BATCH, m=$M, k=$K"
echo "  n=1 (固定)"
echo ""

# ========================================
# 步骤0：检查Pass编译状态
# ========================================
echo "========================================"
echo "步骤0：检查Pass编译状态"
echo "========================================"
echo ""

if [ ! -f ${PASS_DIR}/build/libbmm_to_mul_reduce_pass.so ]; then
    echo "  Pass未编译，开始自动编译..."
    echo "  预计时间：约10分钟"
    echo ""

    cd ${PASS_DIR}
    if [ ! -d build ]; then
        mkdir -p build
    fi
    cd build

    echo "  【执行cmake】"
    cmake .. > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "  ✗ cmake失败"
        cd ${SCRIPT_DIR}
        exit 1
    fi
    echo "  ✓ cmake成功"

    echo "  【编译es_all_obj（耗时较长）】"
    make es_all_obj > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "  ✗ es_all_obj编译失败"
        cd ${SCRIPT_DIR}
        exit 1
    fi
    echo "  ✓ es_all_obj编译成功"

    echo "  【执行make】"
    make -j8 > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "  ✗ make失败"
        cd ${SCRIPT_DIR}
        exit 1
    fi
    echo "  ✓ make成功"

    echo "  【执行make install】"
    make install > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "  ✗ make install失败"
        cd ${SCRIPT_DIR}
        exit 1
    fi
    echo "  ✓ make install成功"

    echo "  ✓ Pass编译完成"
    echo ""
    cd ${SCRIPT_DIR}
else
    echo "  ✓ Pass已编译"
    echo ""
fi

# 设置LD_LIBRARY_PATH（依赖libes_all.so）
export LD_LIBRARY_PATH=${PASS_DIR}/build/es_output/lib64:${CANN_DIR}/lib64:$LD_LIBRARY_PATH

# ========================================
# 步骤1：生成ONNX模型
# ========================================
echo "========================================"
echo "步骤1：生成ONNX模型"
echo "========================================"

source ${CANN_DIR}/set_env.sh

python3 gen_onnx.py --batch $BATCH --m $M --k $K

if [ ! -f model.onnx ]; then
    echo "  ✗ ONNX模型生成失败"
    exit 1
fi
echo ""

# ========================================
# 步骤2：ATC编译OM模型（开启融合pass）
# ========================================
echo "========================================"
echo "步骤2：ATC编译OM模型（开启融合pass）"
echo "========================================"

rm -f *.om

echo "  编译融合模型..."
export DUMP_GE_GRAPH=1
# soc_version需根据实际芯片型号修改，可通过 npu-smi info 命令查看，如910B3对应Ascend910B3
atc --model=./model.onnx --framework=5 --output=./model_fused --soc_version=Ascend910B3 2>&1 | grep -E "Define pattern|MeetRequirements|Created node|InferShape|success|fail|Replacement" | tail -10

if [ ! -f model_fused.om ]; then
    echo "  ✗ OM模型编译失败"
    exit 1
fi
echo "  ✓ model_fused.om已生成"
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
    batch_matmul_count=$(grep -c "type: \"BatchMatMul" "$pre_run_file" 2>/dev/null || true)
    batch_matmul_count=${batch_matmul_count:-0}
    echo "  BatchMatMulV2节点数: $batch_matmul_count"
    if [ "$batch_matmul_count" -eq 0 ]; then
        echo "  ✗ 融合前图未发现BatchMatMul节点，当前模型不符合本样例验证目标"
        exit 1
    fi
else
    echo "  ✗ 未找到PreRunBegin dump图"
    exit 1
fi

# 查找RunCustomPass_AfterInferShape图（融合后）
after_fusion_file=$(ls ge_proto_*_RunCustomPass_AfterInferShape.txt 2>/dev/null | head -1)
if [ -n "$after_fusion_file" ]; then
    echo ""
    echo "  【融合后图】 $after_fusion_file"
    mul_count=$(grep -c "type: \"Mul\"" "$after_fusion_file" 2>/dev/null || true)
    mul_count=${mul_count:-0}
    reduce_sum_count=$(grep -c "type: \"ReduceSum" "$after_fusion_file" 2>/dev/null || true)
    reduce_sum_count=${reduce_sum_count:-0}
    batch_matmul_after=$(grep -c "type: \"BatchMatMul" "$after_fusion_file" 2>/dev/null || true)
    batch_matmul_after=${batch_matmul_after:-0}
    echo "  Mul节点数: $mul_count"
    echo "  ReduceSumD节点数: $reduce_sum_count"
    echo "  BatchMatMulV2节点数: $batch_matmul_after"

    if [ "$mul_count" -ge 1 ] && [ "$reduce_sum_count" -ge 1 ] && [ "$batch_matmul_after" -eq 0 ]; then
        echo ""
        echo "  ✓✓✓ 融合验证通过：BatchMatMulV2已被Mul+ReduceSumD替换"
    else
        echo ""
        echo "  ✗✗✗ 融合验证失败：未发现预期的Mul+ReduceSumD替换"
        exit 1
    fi
else
    echo "  ✗ 未找到RunCustomPass_AfterInferShape dump图"
    exit 1
fi
echo ""

echo "✓✓✓ 验证完成"
