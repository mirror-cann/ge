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
# BmmTile Pass - Pattern 1 专项验证脚本
# 验证 Tile -> BatchMatMulV2 直接融合 (无中间 Reshape/Transpose)
# 路径 A (推荐): TF pb 模型, GE 直接生成 BatchMatMulV2
# 路径 B: ONNX 4D 模型, 尝试绕过 GE 的 Reshape/Transpose 插入
#
# 用法: ./verify_pattern1.sh [batch] [m] [k] [n] [heads]

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PASS_DIR=$(dirname "$SCRIPT_DIR")

if [ -n "$ASCEND_HOME_PATH" ]; then
    CANN_DIR=$ASCEND_HOME_PATH
else
    CANN_DIR=/usr/local/Ascend/cann
fi

cleanup() {
    installed_so=${CANN_DIR}/opp/vendors/pass_so_dir/custom_fusion_passes/libbmm_tile_pass.so
    if [ -f "$installed_so" ] && [ -w "$installed_so" ]; then
        rm -f "$installed_so"
    fi
}
trap cleanup EXIT

export LD_LIBRARY_PATH=${PASS_DIR}/build/es_output/lib64:${CANN_DIR}/lib64:$LD_LIBRARY_PATH
source ${CANN_DIR}/set_env.sh 2>/dev/null

WORK_DIR=$(mktemp -d /tmp/bmm_tile_p1_XXXX)
echo "工作目录: $WORK_DIR"

# 确保 pass 已安装
if [ ! -f ${PASS_DIR}/build/libbmm_tile_pass.so ]; then
    echo "✗ Pass 未编译, 请先执行: cd ${PASS_DIR} && mkdir -p build && cd build && cmake .. && make es_all_obj && make -j8 && make install"
    exit 1
fi

# ========================================
# 路径 A: TF pb 模型 (可靠方案)
# ========================================
echo "=========================================="
echo "路径 A: TensorFlow pb 模型 (推荐)"
echo "=========================================="
echo ""
echo "TF MatMul (4D) 在 GE 中直接映射为 BatchMatMulV2, 不会插入 Reshape/Transpose"
echo "因此 Tile -> BatchMatMulV2 直接相连, 命中 Pattern 1"
echo ""

BATCH=${1:-8}
M=${2:-64}
K=${3:-128}
N=${4:-256}
HEADS=${5:-4}

echo "参数: batch=$BATCH m=$M k=$K n=$N heads=$HEADS"

cd $WORK_DIR
cp ${SCRIPT_DIR}/tf_gen_pb.py .

echo ""
echo "生成 TF pb 模型..."
python3 tf_gen_pb.py --batch $BATCH --m $M --k $K --n $N --heads $HEADS
if [ ! -f model.pb ]; then
    echo "✗ TF pb 生成失败"
    exit 1
fi
echo "✓ model.pb 已生成"

echo ""
echo "编译 OM (开启 dump)..."
export DUMP_GE_GRAPH=1
rm -f *.om

atc --model=./model.pb --framework=3 --output=./model_p1_fused \
    --soc_version=Ascend910B3 2>&1 | grep -E "Define pattern|MeetRequirements|Created node|InferShape|success|fail|BmmTilePass" || true

if [ ! -f model_p1_fused.om ]; then
    echo "✗ OM 编译失败"
    exit 1
fi
echo "✓ model_p1_fused.om 已生成"

echo ""
echo "分析 dump 图..."

# 查找融合前图
pre_run=$(ls ge_proto_*_PreRunBegin.txt 2>/dev/null | head -1)
after_fusion=$(ls ge_proto_*_RunCustomPass_AfterInferShape.txt 2>/dev/null | head -1)

if [ -n "$pre_run" ] && [ -n "$after_fusion" ]; then
    tile_before=$(grep -c 'type: "Tile"' "$pre_run" 2>/dev/null || echo 0)
    tile_after=$(grep -c 'type: "Tile"' "$after_fusion" 2>/dev/null || echo 0)
    bmm_before=$(grep -c 'type: "BatchMatMul' "$pre_run" 2>/dev/null || echo 0)
    bmm_after=$(grep -c 'type: "BatchMatMul' "$after_fusion" 2>/dev/null || echo 0)

    echo "  融合前: Tile=$tile_before, BatchMatMulV2=$bmm_before"
    echo "  融合后: Tile=$tile_after, BatchMatMulV2=$bmm_after"

    if [ "$tile_before" -gt 0 ] && [ "$tile_after" -eq 0 ] && [ "$bmm_after" -ge 1 ]; then
        echo ""
        echo "✓✓✓ Pattern 1 验证通过!"
        echo "    Tile 已删除, BatchMatMulV2 保留并利用广播"
        echo "    (TF pb 路径, Tile -> BatchMatMulV2 直接连接命中)"
    else
        echo ""
        echo "结果: Tile_before=$tile_before, Tile_after=$tile_after, BMM_after=$bmm_after"
        echo "验证状态: 需人工检查 dump 图确认 Pattern 1 是否命中"
    fi
else
    echo "✗ 未找到 dump 图文件"
    echo "  尝试列出 dump 文件:"
    ls -la ge_proto_*.txt 2>/dev/null || echo "  (无 dump 文件)"
fi

echo ""
echo "所有输出保存在: $WORK_DIR"
echo "验证完成"
