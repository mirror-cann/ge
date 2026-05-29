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
AIR_PATH="${PROJECT_DIR}/output/single_add.air"
OM_PATH="${PROJECT_DIR}/output/single_add.om"
ATC_BIN="atc"
CUSTOM_OP_DIR=""
CUSTOM_OP_LIBRARY_PATH=""
CUSTOM_OP_SOURCE_PATH=""
CUSTOM_OP_PROTO_HEADER_PATH=""

info() {
  echo "[INFO] $*"
}

error() {
  echo "[ERROR] $*" >&2
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

run_atc() {
  if [[ -n "${ASCEND_HOME_PATH:-}" && -x "${ASCEND_HOME_PATH}/bin/atc" ]]; then
    ATC_BIN="${ASCEND_HOME_PATH}/bin/atc"
  fi

  if [[ ! -f "${AIR_PATH}" ]]; then
    error "AIR file not found: ${AIR_PATH}"
    error "Please generate ${AIR_PATH} first."
    exit 1
  fi

  local atc_rc=0
  set +e
  "${ATC_BIN}" \
    --model="${AIR_PATH}" \
    --framework=1 \
    --output="${OM_PATH%.*}" \
    --soc_version=Ascend910B1
  atc_rc=$?
  set -e

  if [[ ${atc_rc} -ne 0 ]]; then
    if [[ -s "${OM_PATH}" ]]; then
      echo "[WARN] atc exited with code ${atc_rc}, but OM was generated: ${OM_PATH}. Continue pipeline."
      return 0
    fi
    error "ATC failed with exit code ${atc_rc}, and OM file was not generated: ${OM_PATH}"
    exit "${atc_rc}"
  fi

  info "ATC done: ${OM_PATH}"
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
CUSTOM_OP_SOURCE_PATH="${PROJECT_DIR}/output/op_graph/lib/$(detect_opp_os_dir)/$(detect_opp_arch_dir)/add_custom_kernel.cpp"
CUSTOM_OP_PROTO_HEADER_PATH="${PROJECT_DIR}/output/op_graph/include/add_custom.h"

mkdir -p "${BUILD_DIR}" "${OUTPUT_DIR}" "${CUSTOM_OP_DIR}"

JOBS="$(detect_jobs)"

info "Step 1/4: configure and build sample targets"
cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" -j"${JOBS}"
cmake --install "${BUILD_DIR}"
export ASCEND_CUSTOM_OPP_PATH="${OUTPUT_DIR}:${ASCEND_CUSTOM_OPP_PATH:-}"
info "ASCEND_CUSTOM_OPP_PATH=${ASCEND_CUSTOM_OPP_PATH}"

if [[ ! -f "${CUSTOM_OP_LIBRARY_PATH}" ]]; then
  error "Custom op library was not generated: ${CUSTOM_OP_LIBRARY_PATH}"
  exit 1
fi
if [[ ! -f "${CUSTOM_OP_SOURCE_PATH}" ]]; then
  error "Custom op source file was not generated: ${CUSTOM_OP_SOURCE_PATH}"
  exit 1
fi
if [[ ! -f "${CUSTOM_OP_PROTO_HEADER_PATH}" ]]; then
  error "Custom op proto header was not generated: ${CUSTOM_OP_PROTO_HEADER_PATH}"
  exit 1
fi

info "Step 2/4: run graph_build exporter"
rm -f "${AIR_PATH}"
(
  cd "${OUTPUT_DIR}"
  "${BUILD_DIR}/compilable_add_graph_build"
)

if [[ ! -f "${AIR_PATH}" ]]; then
  error "AIR file was not generated: ${AIR_PATH}"
  exit 1
fi

info "Step 3/4: convert AIR to OM by ATC"
rm -f "${OM_PATH}"
run_atc

if [[ ! -f "${OM_PATH}" ]]; then
  error "OM file was not generated: ${OM_PATH}"
  exit 1
fi

info "Step 4/4: run model_exec"
(
  cd "${BUILD_DIR}"
  ./compilable_add_model_exec "${OM_PATH}"
)

info "Sample pipeline finished."
