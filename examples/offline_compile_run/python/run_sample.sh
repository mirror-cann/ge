#!/usr/bin/env bash

# ----------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------

set -euo pipefail # 命令执行错误则退出

# ---------- 函数定义 ----------
usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

选项:
  -t, --target TARGET       指定执行目标
                            (build_model|run_infer|sample_and_run_python|
                             build_bundle_model|run_bundle_infer|sample_and_run_bundle_python)
  -s, --soc-version VER     仅当 target 包含离线编译步骤时生效；
                            纯推理目标忽略；默认不传（空）
  -h, --help                显示此帮助信息
EOF
    exit 0
}

TARGET="sample_and_run_python"
SOC_VERSION=""
SHOWCASE_SINGLE_DIR="src/single_model"
SHOWCASE_BUNDLE_DIR="src/bundle_model"

# ---------- 解析命令行参数 ----------
while [[ $# -gt 0 ]]; do
    case "$1" in
        -t|--target)
            TARGET="$2"
            shift 2
            ;;
        -s|--soc-version)
            SOC_VERSION="$2"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "未知选项: $1" >&2
            usage
            exit 1
            ;;
    esac
done

# 验证目标有效性
VALID_TARGETS=(
    "build_model"
    "run_infer"
    "sample_and_run_python"
    "build_bundle_model"
    "run_bundle_infer"
    "sample_and_run_bundle_python"
)
if [[ ! " ${VALID_TARGETS[*]} " =~ " ${TARGET} " ]]; then
    echo "错误: 无效目标 '${TARGET}'。有效目标: ${VALID_TARGETS[*]}" >&2
    exit 1
fi

echo "[Info] 目标设置为: ${TARGET}"

set +u
if [[ -z "${ASCEND_HOME_PATH:-}" ]]; then
    echo "ERROR 环境变量 ASCEND_HOME_PATH 未配置" >&2
    echo "ERROR 请先执行: source /usr/local/Ascend/cann/set_env.sh" >&2
    exit 1
fi

# ---------- 自动获取系统架构 ----------
ARCH=$(uname -m)
# 映射架构名称
case "${ARCH}" in
    x86_64|amd64)
        ASCEND_ARCH="x86_64-linux"
        ;;
    aarch64|arm64)
        ASCEND_ARCH="aarch64-linux"
        ;;
    *)
        echo "WARNING: 未识别的架构 ${ARCH}，使用默认值 x86_64-linux" >&2
        ASCEND_ARCH="x86_64-linux"
        ;;
esac

echo "[Info] 检测到系统架构: ${ARCH}"
echo "[Info] 使用 ASCEND 架构: ${ASCEND_ARCH}"

export PYTHONPATH="$(pwd)/src:${PYTHONPATH:-}"

# --soc-version 仅在含离线编译步骤的 target 下使用；纯推理执行时忽略
warn_soc_ignored_for_infer_targets() {
    if [[ -n "${SOC_VERSION}" && ( "${TARGET}" == "run_infer" || "${TARGET}" == "run_bundle_infer" ) ]]; then
        echo "[Warning] --soc-version 在纯推理 target 下不生效（仅编译步骤使用）" >&2
    fi
}

run_build_model() {
    local cmd=(python3 "${SHOWCASE_SINGLE_DIR}/build_add_model.py")
    if [[ -n "${SOC_VERSION}" ]]; then
        cmd+=(--soc-version "${SOC_VERSION}")
    fi
    echo "[Info] 开始执行单模型离线编译"
    if "${cmd[@]}"; then
        echo "[Success] 单模型离线编译执行成功"
    else
        echo "[Error] 单模型离线编译执行失败，请检查上述错误信息" >&2
        return 1
    fi
}

run_build_bundle_model() {
    local cmd=(python3 "${SHOWCASE_BUNDLE_DIR}/build_bundle_model.py")
    if [[ -n "${SOC_VERSION}" ]]; then
        cmd+=(--soc-version "${SOC_VERSION}")
    fi
    echo "[Info] 开始执行 bundle 离线编译"
    if "${cmd[@]}"; then
        echo "[Success] bundle 离线编译执行成功"
    else
        echo "[Error] bundle 离线编译执行失败，请检查上述错误信息" >&2
        return 1
    fi
}

run_infer() {
    echo "[Info] 开始执行单模型推理"
    if python3 "${SHOWCASE_SINGLE_DIR}/run_add_model.py"; then
        echo "[Success] 单模型推理执行成功"
    else
        echo "[Error] 单模型推理执行失败，请检查上述错误信息" >&2
        return 1
    fi
}

run_bundle_infer() {
    echo "[Info] 开始执行 bundle 推理"
    if python3 "${SHOWCASE_BUNDLE_DIR}/run_bundle_model.py"; then
        echo "[Success] bundle 推理执行成功"
    else
        echo "[Error] bundle 推理执行失败，请检查上述错误信息" >&2
        return 1
    fi
}

if [[ -n "${SOC_VERSION}" && "${TARGET}" != "run_infer" && "${TARGET}" != "run_bundle_infer" ]]; then
    echo "[Info] soc-version: ${SOC_VERSION}"
fi

case "${TARGET}" in
    build_model)
        run_build_model
        ;;
    run_infer)
        warn_soc_ignored_for_infer_targets
        run_infer
        ;;
    sample_and_run_python)
        run_build_model
        run_infer
        ;;
    build_bundle_model)
        run_build_bundle_model
        ;;
    run_bundle_infer)
        warn_soc_ignored_for_infer_targets
        run_bundle_infer
        ;;
    sample_and_run_bundle_python)
        run_build_bundle_model
        run_bundle_infer
        ;;
esac

echo "[Success] sample 执行成功"
