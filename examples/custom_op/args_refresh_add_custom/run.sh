#!/usr/bin/env bash
# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="${SCRIPT_DIR}"
BUILD_DIR="${PROJECT_DIR}/build"
OUTPUT_DIR="${PROJECT_DIR}/output"

info() {
  echo "[INFO] $*"
}

error() {
  echo "[ERROR] $*" >&2
}

# 在后台显示 spinner 动画，表示正在进行耗时操作
# 用法: start_spinner "消息" ... 执行操作 ... stop_spinner
_spinner_pid=""
start_spinner() {
  local msg="$1"
  (
    local chars='⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏'
    local i=0
    while true; do
      local char="${chars:i%${#chars}:1}"
      printf "\r%s %s" "${char}" "${msg}"
      sleep 0.1
      i=$((i + 1))
    done
  ) &
  _spinner_pid=$!
  disown
}

stop_spinner() {
  if [[ -n "${_spinner_pid}" ]]; then
    kill "${_spinner_pid}" 2>/dev/null || true
    wait "${_spinner_pid}" 2>/dev/null || true
    printf "\r%*s\r" 80 ""
    _spinner_pid=""
  fi
}

detect_opp_os_dir() {
  local os_name
  os_name="$(uname -s | tr '[:upper:]' '[:lower:]')"
  case "${os_name}" in
    mingw*|msys*|cygwin*)
      echo "windows"
      ;;
    *)
      echo "linux"
      ;;
  esac
}

detect_opp_arch_dir() {
  local arch_name
  arch_name="$(uname -m | tr '[:upper:]' '[:lower:]')"
  case "${arch_name}" in
    aarch64|arm64)
      echo "aarch64"
      ;;
    x86_64|amd64)
      echo "x86_64"
      ;;
    *)
      echo "${arch_name}"
      ;;
  esac
}

get_custom_op_library_name() {
  if [[ "$(detect_opp_os_dir)" == "windows" ]]; then
    echo "cust_opapi.dll"
    return
  fi
  echo "libcust_opapi.so"
}

detect_jobs() {
  if command -v nproc >/dev/null 2>&1; then
    nproc
    return
  fi
  echo 8
}

usage() {
  cat <<'EOF'
Usage:
  bash run.sh

Options:
  -h, --help    显示帮助信息
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    *)
      error "Unknown option: $1"
      usage
      exit 1
      ;;
  esac
  shift
done

if [[ -z "${ASCEND_HOME_PATH:-}" ]]; then
  error "ASCEND_HOME_PATH is empty. Please source CANN set_env.sh first."
  exit 1
fi

CUSTOM_OP_DIR="${PROJECT_DIR}/output/op_graph/lib/$(detect_opp_os_dir)/$(detect_opp_arch_dir)"
CUSTOM_OP_LIBRARY_PATH="${PROJECT_DIR}/output/op_graph/lib/$(detect_opp_os_dir)/$(detect_opp_arch_dir)/$(get_custom_op_library_name)"

mkdir -p "${BUILD_DIR}" "${OUTPUT_DIR}" "${CUSTOM_OP_DIR}"

JOBS="$(detect_jobs)"

info "Step 1/2: configure and build sample targets"
cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
BUILD_LOG="${BUILD_DIR}/build.log"
start_spinner "Building custom op library..."
if cmake --build "${BUILD_DIR}" -j"${JOBS}" > "${BUILD_LOG}" 2>&1; then
  stop_spinner
  info "Build completed"
else
  stop_spinner
  error "Build failed, last 20 lines of build log:"
  tail -20 "${BUILD_LOG}" >&2
  exit 1
fi
cmake --install "${BUILD_DIR}"
export ASCEND_CUSTOM_OPP_PATH="${OUTPUT_DIR}:${ASCEND_CUSTOM_OPP_PATH:-}"
info "ASCEND_CUSTOM_OPP_PATH=${ASCEND_CUSTOM_OPP_PATH}"

if [[ ! -f "${CUSTOM_OP_LIBRARY_PATH}" ]]; then
  error "Custom op library was not generated: ${CUSTOM_OP_LIBRARY_PATH}"
  exit 1
fi

info "Step 2/2: run session_run (online perf comparison)"
(
  cd "${BUILD_DIR}"
  ./args_refresh_session_run
)

info "Sample pipeline finished."
