#!/bin/bash
# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

# Format: "URL Dir"
DOWNLOAD_LIST=(
    "https://gitcode.com/cann-src-third-party/protobuf/releases/download/v25.1/protobuf-25.1.tar.gz"
    "https://gitcode.com/cann-src-third-party/boost/releases/download/v1.87.0/boost_1_87_0.tar.gz"
    "https://gitcode.com/cann-src-third-party/abseil-cpp/releases/download/20230802.1/abseil-cpp-20230802.1.tar.gz"
    "https://gitcode.com/cann-src-third-party/c-ares/releases/download/v1.19.1/c-ares-1.19.1.tar.gz"
    "https://gitcode.com/cann-src-third-party/benchmark/releases/download/v1.8.3/benchmark-1.8.3.tar.gz"
    "https://gitcode.com/cann-src-third-party/grpc/releases/download/v1.60.0/grpc-1.60.0.tar.gz"
    "https://gitcode.com/cann-src-third-party/googletest/releases/download/v1.14.0/googletest-1.14.0.tar.gz"
    "https://gitcode.com/cann-src-third-party/json/releases/download/v3.11.3/json-3.11.3.tar.gz"
    "https://gitcode.com/cann-src-third-party/openssl/releases/download/openssl-3.0.9/openssl-openssl-3.0.9.tar.gz"
    "https://gitcode.com/cann-src-third-party/re2/releases/download/2024-02-01/re2-2024-02-01.tar.gz"
    "https://gitcode.com/cann-src-third-party/symengine/releases/download/v0.12.0/symengine-0.12.0.tar.gz"
    "https://gitcode.com/cann-src-third-party/zlib/releases/download/v1.2.13/zlib-1.2.13.tar.gz"
    "https://gitcode.com/cann-src-third-party/makeself/releases/download/release-2.5.0-patch1.0/makeself-release-2.5.0-patch1.tar.gz"
    "https://gitcode.com/cann-src-third-party/mockcpp/releases/download/v2.7-h1/mockcpp-2.7.tar.gz"
    "https://gitcode.com/cann-src-third-party/libseccomp/releases/download/v2.5.4/libseccomp-2.5.4.tar.gz"
    # Example:
    # "https://example.com/xxx.tar.gz"
)

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CANN_CMAKE_TAG=$(grep -oP 'set\(CANN_CMAKE_TAG\s+"([^"]+)"\)' "${SCRIPT_DIR}/../cmake/fetch_cann_cmake.cmake" | grep -oP '"[^"]+"' | tr -d '"')
if [ -z "${CANN_CMAKE_TAG}" ]; then
    echo -e "Error: failed to read CANN_CMAKE_TAG from cmake/fetch_cann_cmake.cmake."
    exit 1
fi
DOWNLOAD_LIST+=("https://raw.gitcode.com/cann/cmake/archive/refs/heads/${CANN_CMAKE_TAG}.tar.gz")

# 创建临时工作目录
WORK_DIR="./opensource"
mkdir -p "${WORK_DIR}"
if [ $? -ne 0 ]; then
    echo -e "Error: create temp dir ${WORK_DIR} failed."
    exit 1
fi

# 遍历下载列表进行下载
for ITEM in "${DOWNLOAD_LIST[@]}"; do
    URL=$(echo "${ITEM}" | awk '{print $1}')
    DIR_NAME=$(echo "${ITEM}" | awk '{print $2}')
    FILE_NAME=$(basename "${URL}")

    echo -e "Download from ${URL} to ${WORK_DIR}/${FILE_NAME}"
    wget -q -O "${WORK_DIR}/${FILE_NAME}" "${URL}" --show-progress

    if [ $? -eq 0 ]; then
        echo -e "Download finished: ${DIR_NAME}."
    else
        echo -e "Download failed: ${URL}."
        exit 1
    fi
done

CMAKE_FILE="${CANN_CMAKE_TAG}.tar.gz"
mv "${WORK_DIR}/${CMAKE_FILE}" "${WORK_DIR}/cmake-${CMAKE_FILE}"

# 打包所有下载目录
file_name="opensource.tar.gz"
echo -e "Make tar file: ${file_name}"
tar -zcf "${file_name}" -C ./ "${WORK_DIR}"

if [ $? -eq 0 ]; then
    echo -e "Done."
    # 清理临时目录
    rm -rf "${WORK_DIR}"
    echo -e "Clear temp dir."
else
    echo -e "Error: tar file generate failed."
    exit 1
fi

echo -e "Third party lib source download success: $(readlink -f "$file_name")."
exit 0
