#!/bin/bash
# ----------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------

set -o pipefail

SCRIPT_PATH="$( cd "$(dirname "$BASH_SOURCE")" ; pwd -P )"

function build()
{
  USER_KERNEL="$(arch)"
  if [[ ${TARGET_KERNEL} = "x86" ]] || [[ ${TARGET_KERNEL} = "X86" ]];then
    TARGET_COMPILER="g++"
    TARGET_KERNEL="x86"
  else
    if [[ ${USER_KERNEL} == "x86_64" ]];then
      TARGET_COMPILER="aarch64-linux-gnu-g++"
      TARGET_KERNEL="arm"
    else
      TARGET_COMPILER="g++"
      TARGET_KERNEL="arm"
    fi
  fi
  if [ -d ${SCRIPT_PATH}/../build/intermediates/host ];then
    rm -rf ${SCRIPT_PATH}/../build/intermediates/host
  fi
    
  mkdir -p ${SCRIPT_PATH}/../build/intermediates/host
  cd ${SCRIPT_PATH}/../build/intermediates/host

  # Start compiling
  cmake ../../../src -DCMAKE_CXX_COMPILER=${TARGET_COMPILER} -DCMAKE_SKIP_RPATH=TRUE
  if [ $? -ne 0 ];then
    echo "[ERROR] cmake error, Please check your environment!"
    return 1
  fi
  make
  if [ $? -ne 0 ];then
    echo "[ERROR] build failed, Please check your environment!"
    return 1
  fi
  cd - > /dev/null
}

function target_kernel()
{
    local arch=""

    if command -v lscpu >/dev/null 2>&1; then
        arch_info=$(lscpu 2>/dev/null | grep -i "Architecture" | head -n1)
        if [[ -n "$arch_info" ]]; then
            arch=$(echo "$arch_info" | awk -F: '{print $2}' | tr -d ' \t\n\r' | tr '[:upper:]' '[:lower:]')

            if [[ "$arch" == "aarch64" || "$arch" == "armv8" || "$arch" == "armv7l" || "$arch" == "arm" ]]; then
                TARGET_KERNEL="arm"
                echo "[INFO] Detected ARM architecture via lscpu: '$arch_info'"
                return 0
            elif [[ "$arch" == "x86_64" || "$arch" == "i686" || "$arch" == "i386" ]]; then
                TARGET_KERNEL="x86"
                echo "[INFO] Detected x86 architecture via lscpu: '$arch_info'"
                return 0
            else
                echo "[WARN] Unknown architecture from lscpu: '$arch', will fall back to manual input."
            fi
        fi
    else
        echo "[WARN] 'lscpu' command not found, falling back to manual input."
    fi

    declare -i CHOICE_TIMES=0
    TARGET_KERNEL=""
    while [[ "${TARGET_KERNEL}" == "" ]]; do
        ((CHOICE_TIMES++))
        if [[ ${CHOICE_TIMES} -gt 3 ]]; then
            echo "[ERROR] TARGET_KERNEL entered incorrectly three times. Please input 'arm' or 'x86'."
            return 1
        fi

        read -p "Please input TARGET_KERNEL? [arm/x86]: " input_kernel
        input_kernel=$(echo "$input_kernel" | tr '[:upper:]' '[:lower:]')

        if [[ "$input_kernel" == "arm" || "$input_kernel" == "x86" ]]; then
            TARGET_KERNEL="$input_kernel"
            echo "[INFO] Input is valid, start preparation."
        else
            echo "[WARNING] The ${CHOICE_TIMES}th parameter input error! Please enter 'arm' or 'x86'."
        fi
    done

    return 0
}

function main()
{
  echo "[INFO] Sample preparation"

  target_kernel
  if [ $? -ne 0 ];then
    return 1
  fi
    
  build
  if [ $? -ne 0 ];then
    return 1
  fi
    
  echo "[INFO] Sample preparation is complete"
}
main
