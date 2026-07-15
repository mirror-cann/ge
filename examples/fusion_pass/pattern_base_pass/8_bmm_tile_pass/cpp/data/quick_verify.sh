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

# BmmTile Pass 一键式验证脚本
# 验证 Tile → BatchMatMulV2 融合 pass：删除冗余 Tile，利用 BMM 广播
# 用法: ./quick_verify.sh [batch] [m] [k] [n] [test_rounds]

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
        echo "  ✓ CANN安装的pass so已清理"
    elif [ -f "$installed_so" ]; then
        echo "  ⚠ CANN安装的pass so需要sudo权限清理，跳过（可手动执行: sudo rm $installed_so）"
    fi
}

trap cleanup EXIT

# 默认参数
BATCH=${1:-2}
M=${2:-64}
K=${3:-128}
N=${4:-256}
TEST_ROUNDS=${5:-3}

echo "========================================"
echo "BmmTile Pass 快速验证"
echo "========================================"
echo ""
echo "【测试配置】"
echo "  Shape: batch=$BATCH, m=$M, k=$K, n=$N"
echo "  权重 w: [1, $K, $N] -> Tile -> [$BATCH, $K, $N]"
echo "  输入 x: [$BATCH, $M, $K]"
echo "  测试轮数: $TEST_ROUNDS轮"
echo ""

# ========================================
# 步骤0a：检查Pass编译状态
# ========================================
echo "========================================"
echo "步骤0a：检查Pass编译状态"
echo "========================================"
echo ""

if [ ! -f ${PASS_DIR}/build/libbmm_tile_pass.so ]; then
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

python3 gen_onnx.py --batch $BATCH --m $M --k $K --n $N

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
atc --model=./model.onnx --framework=5 --output=./model_fused --soc_version=Ascend910B3 2>&1 | grep -E "Define pattern|MeetRequirements|Created node|InferShape|success|fail" | tail -10

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
    tile_count=$(grep -c "type: \"Tile\"" "$pre_run_file" 2>/dev/null || true)
    tile_count=${tile_count:-0}
    bmm_count=$(grep -c "type: \"BatchMatMul" "$pre_run_file" 2>/dev/null || true)
    bmm_count=${bmm_count:-0}
    echo "  Tile节点数: $tile_count"
    echo "  BatchMatMul/BatchMatMulV2节点数: $bmm_count"
    if [ "$tile_count" -eq 0 ] || [ "$bmm_count" -eq 0 ]; then
        echo "  ✗ 融合前图未发现Tile或BatchMatMul节点，当前模型不符合本样例验证目标"
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
    tile_after=$(grep -c "type: \"Tile\"" "$after_fusion_file" 2>/dev/null || true)
    tile_after=${tile_after:-0}
    bmm_after=$(grep -c "type: \"BatchMatMul" "$after_fusion_file" 2>/dev/null || true)
    bmm_after=${bmm_after:-0}
    echo "  Tile节点数: $tile_after"
    echo "  BatchMatMul/BatchMatMulV2节点数: $bmm_after"

    if [ "$tile_after" -eq 0 ] && [ "$bmm_after" -ge 1 ]; then
        echo ""
        echo "  ✓✓✓ 融合验证通过：Tile已被删除，BatchMatMulV2保留并利用广播"
    else
        echo ""
        echo "  ✗✗✗ 融合验证失败：Tile未被删除或BatchMatMulV2丢失"
        exit 1
    fi
else
    echo "  ✗ 未找到RunCustomPass_AfterInferShape dump图"
    exit 1
fi
echo ""

# ========================================
# 步骤4：运行性能测试（多轮，可选）
# ========================================
echo "========================================"
echo "步骤4：运行性能测试（$TEST_ROUNDS轮）"
echo "========================================"
echo ""

if [ ! -f benchmark_model ]; then
    if [ -f benchmark_model.cpp ]; then
        echo "  benchmark_model不存在，开始编译..."
        g++ -O3 benchmark_model.cpp -o benchmark_model \
          -I${CANN_DIR}/include \
          -L${CANN_DIR}/lib64 \
          -lacl_rt -lacl_mdl \
          -Wl,-rpath=${CANN_DIR}/lib64

        if [ ! -f benchmark_model ]; then
            echo "  ✗ benchmark_model编译失败，跳过性能测试"
            echo ""
            echo "✓✓✓ 融合验证完成（跳过性能测试）"
            exit 0
        fi
        echo "  ✓ benchmark_model编译成功"
    else
        echo "  benchmark_model.cpp不存在，跳过性能测试"
        echo ""
        echo "✓✓✓ 融合验证完成（跳过性能测试）"
        exit 0
    fi
fi

echo "  ✓ benchmark_model已存在"
echo ""

results_file="/tmp/bmm_tile_verify_results.txt"
echo "" > $results_file

for round in $(seq 1 $TEST_ROUNDS); do
    echo "【第${round}轮测试】"

    bench_output=$(./benchmark_model 1000 50 2>&1)
    echo "$bench_output" | grep -E "Average time|Total time|Throughput"

    avg_ms=$(echo "$bench_output" | grep "Average time per iteration" | awk -F'[ :]' '{for(i=1;i<=NF;i++) if($i ~ /^[0-9]/) print $i; exit}')

    if [ -n "$avg_ms" ]; then
        echo "  ✓ 平均耗时: ${avg_ms} ms"
        echo "${round}:${avg_ms}" >> $results_file
    else
        echo "  ✗ 性能数据获取失败"
    fi

    echo ""
done

# ========================================
# 步骤5：性能统计总结
# ========================================
echo "========================================"
echo "步骤5：性能统计总结"
echo "========================================"
echo ""

if [ -f "$results_file" ]; then
    python3 << PYTHON
results = []
with open('/tmp/bmm_tile_verify_results.txt', 'r') as f:
    for line in f:
        parts = line.strip().split(':')
        if len(parts) >= 2:
            results.append({'round': int(parts[0]), 'avg_ms': float(parts[1])})

if len(results) > 0:
    print("【融合模型性能统计】")
    print()
    print("| 轮次 | 平均耗时(ms) |")
    print("|------|-------------|")
    for r in results:
        print(f"| 第{r['round']}轮 | {r['avg_ms']:.3f} |")
    print()

    avg = sum([r['avg_ms'] for r in results]) / len(results)
    min_t = min([r['avg_ms'] for r in results])
    max_t = max([r['avg_ms'] for r in results])
    range_t = max_t - min_t

    print(f"  平均耗时: {avg:.3f} ms")
    print(f"  最小耗时: {min_t:.3f} ms")
    print(f"  最大耗时: {max_t:.3f} ms")
    print(f"  波动范围: {range_t:.3f} ms ({range_t/avg*100:.1f}%)")
    print(f"  吞吐量: {1000.0/avg:.1f} iterations/sec")
PYTHON
fi

rm -f /tmp/bmm_tile_verify_results.txt

echo ""
echo "✓✓✓ 验证完成"
