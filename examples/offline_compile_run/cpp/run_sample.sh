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

set -euo pipefail

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

选项:
  -t, --target TARGET       指定执行目标
                            (build_model|run_infer|sample_and_run|
                             build_bundle_model|run_bundle_infer|sample_and_run_bundle)
  -s, --soc-version VER     仅当 target 含离线编译步骤时生效；纯推理目标忽略
  -h, --help                显示帮助
EOF
    exit 0
}

TARGET="sample_and_run"
SOC_VERSION=""

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
            ;;
    esac
done

VALID_TARGETS=(
    "build_model"
    "run_infer"
    "sample_and_run"
    "build_bundle_model"
    "run_bundle_infer"
    "sample_and_run_bundle"
)
if [[ ! " ${VALID_TARGETS[*]} " =~ " ${TARGET} " ]]; then
    echo "错误: 无效目标 '${TARGET}'。有效目标: ${VALID_TARGETS[*]}" >&2
    exit 1
fi

set +u
if [[ -z "${ASCEND_HOME_PATH:-}" ]]; then
    echo "ERROR 环境变量 ASCEND_HOME_PATH 未配置" >&2
    echo "ERROR 请先执行: source /usr/local/Ascend/cann/set_env.sh" >&2
    exit 1
fi

ARCH=$(uname -m)
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

echo "[Info] 目标: ${TARGET}"
echo "[Info] 架构: ${ASCEND_ARCH}"

BUILD_DIR="build"
BIN="${BUILD_DIR}/offline_compile_sample"

build_cpp() {
    echo "[Info] 配置并编译 offline_compile_sample"
    mkdir -p "${BUILD_DIR}"
    export LD_LIBRARY_PATH="${ASCEND_HOME_PATH}/lib64:${LD_LIBRARY_PATH:-}"
    cmake -S . -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
    cmake --build "${BUILD_DIR}" --target offline_compile_sample -j"$(nproc)"
}

run_build_add() {
    local args=("${BIN}" build-model)
    if [[ -n "${SOC_VERSION}" ]]; then
        args+=(--soc-version "${SOC_VERSION}")
        echo "[Info] soc-version: ${SOC_VERSION}"
    fi
    echo "[Info] 单模型离线编译"
    "${args[@]}"
}

run_build_bundle() {
    local args=("${BIN}" build-bundle)
    if [[ -n "${SOC_VERSION}" ]]; then
        args+=(--soc-version "${SOC_VERSION}")
        echo "[Info] soc-version: ${SOC_VERSION}"
    fi
    echo "[Info] Bundle 离线编译"
    "${args[@]}"
}

warn_soc_ignored() {
    if [[ -n "${SOC_VERSION}" && ( "${TARGET}" == "run_infer" || "${TARGET}" == "run_bundle_infer" ) ]]; then
        echo "[Warning] --soc-version 在纯推理 target 下不生效" >&2
    fi
}

run_infer_add() {
    echo "[Info] 单模型推理"
    "${BIN}" run-infer
}

run_infer_bundle() {
    echo "[Info] Bundle 推理"
    "${BIN}" run-bundle-infer
}

case "${TARGET}" in
    build_model)
        build_cpp
        run_build_add
        ;;
    run_infer)
        warn_soc_ignored
        build_cpp
        run_infer_add
        ;;
    sample_and_run)
        build_cpp
        run_build_add
        run_infer_add
        ;;
    build_bundle_model)
        build_cpp
        run_build_bundle
        ;;
    run_bundle_infer)
        warn_soc_ignored
        build_cpp
        run_infer_bundle
        ;;
    sample_and_run_bundle)
        build_cpp
        run_build_bundle
        run_infer_bundle
        ;;
esac

echo "[Success] sample 执行成功"
