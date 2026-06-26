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

set -euo pipefail

script_path="$( cd "$(dirname "$BASH_SOURCE")" ; pwd -P )"

target_kernel=""
target_compiler=""

function detect_target_kernel()
{
  local arch
  arch="$(uname -m | tr '[:upper:]' '[:lower:]')"
  if [[ "${arch}" == "x86_64" || "${arch}" == "i686" || "${arch}" == "i386" ]]; then
    target_kernel="x86"
    target_compiler="g++"
  elif [[ "${arch}" == "aarch64" || "${arch}" == "arm64" ]]; then
    target_kernel="arm"
    target_compiler="g++"
  else
    target_kernel="arm"
    target_compiler="g++"
  fi
  echo "[INFO] target kernel: ${target_kernel}, compiler: ${target_compiler}"
}

function build_sample()
{
  rm -rf "${script_path}/../build/intermediates/host"
  mkdir -p "${script_path}/../build/intermediates/host"
  cmake -S "${script_path}/../src" -B "${script_path}/../build/intermediates/host" \
    -DCMAKE_CXX_COMPILER="${target_compiler}" -DCMAKE_SKIP_RPATH=TRUE
  cmake --build "${script_path}/../build/intermediates/host" -- -j"$(nproc)"
}

echo "[INFO] Sample preparation"
detect_target_kernel
build_sample
echo "[INFO] Sample preparation is complete"