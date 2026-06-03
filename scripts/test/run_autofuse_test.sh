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

set -e

BASEPATH=$(cd "$(dirname $0)"; pwd)/../../

# Source common lcov version detection functions
source ${BASEPATH}/scripts/support_multiple_versions_of_lcov.sh

BUILD_RELATIVE_PATH="build"
BUILD_PATH="${BASEPATH}/${BUILD_RELATIVE_PATH}/"
OUTPUT_PATH="${BASEPATH}/output"
METADEF_LIB_PATH=${OUTPUT_PATH}/metadef/lib/
TESTS_ST_PATH="${BASEPATH}/tests/autofuse/st/"
RUN_V35_TESTS="off"

# TODO(For autofuse): Remove 'export DISABLE_COMPILATION_WERROR=ON' and fix the related compilation errors.
export DISABLE_COMPILATION_WERROR=ON

# print usage message
usage() {
  echo "Usage:"
  echo "sh run_autofuse_test.sh [-s | --st] [-u | --ut] [-j<N>] [-c | --cov]"
  echo "               [--ascend_install_path=<PATH>]"
  echo "               [--ascend_3rd_lib_path=<PATH>]"
  echo ""
  echo "Options:"
  echo "    -s, --st       Build all st"
  echo "    -u, --ut       Build all ut"
  echo "    -m, --module    Option arg, specify the model name to build st or ut, default all"
  echo "    -j<N>            Set the number of threads used for llt, default 8."
  echo "    -c, --cov        Build ut with coverage tag"
  echo "                     Please ensure that the environment has correctly installed lcov, gcov, and genhtml."
  echo "                     and the version matched gcc/g++."
  echo "    -h, --help     Print usage"
  echo "    --ascend_install_path=<PATH>"
  echo "                   Set ascend package install path, default /usr/local/Ascend/ascend-toolkit/latest"
  echo "    --ascend_3rd_lib_path=<PATH>"
  echo "                     Set ascend third_party package install path, default ./output/third_party"
  echo ""
}

# parse and set options
checkopts() {
  ENABLE_UT="off"
  ENABLE_ST="off"
  ENABLE_COV="off"
  THREAD_NUM=8
  MODEL_NAME="all"
  if [ -n "$ASCEND_INSTALL_PATH" ]; then
    ASCEND_INSTALL_PATH="$ASCEND_INSTALL_PATH"
  else
    ASCEND_INSTALL_PATH="/usr/local/Ascend/ascend-toolkit/latest"
  fi
  if [ -n "$CANN_3RD_LIB_PATH" ]; then
    CANN_3RD_LIB_PATH="$CANN_3RD_LIB_PATH"
  else
    CANN_3RD_LIB_PATH="$BASEPATH/output/third_party"
  fi
  if [ -n "$ENABLE_PKG" ]; then
    ENABLE_PKG="$ENABLE_PKG"
  else
    ENABLE_PKG="off"
  fi
  if [ -d "${BASEPATH}/compiler/graph/optimize/autofuse/v35" ]; then
    RUN_V35_TESTS="on"
  fi

  # Process the options
  parsed_args=$(getopt -a -o hsucj:hv -l help,st,ut,cov,verbose,ascend_install_path:,ascend_3rd_lib_path:,module: -- "$@") || {
    usage
    exit 1
  }

  eval set -- "$parsed_args"

  while true; do
    case "$1" in
      -h | --help)
        usage
        exit 0
        ;;
      -j)
        THREAD_NUM="$2"
        shift 2
        ;;
      --ascend_install_path)
        ASCEND_INSTALL_PATH="$(realpath $2)"
        shift 2
        ;;
      --ascend_3rd_lib_path)
        CANN_3RD_LIB_PATH="$(realpath $2)"
        shift 2
        ;;
      -m | --module)
        MODEL_NAME=$2
        shift 2
        ;;
      -s | --st)
        ENABLE_ST="on"
        shift
        ;;
      -u | --ut)
        ENABLE_UT="on"
        shift
        ;;
      -c | --cov)
        ENABLE_COV="on"
        shift
        ;;
      --)
        shift
        break
        ;;
      *)
        echo "Undefined option: $1"
        usage
        exit 1
        ;;
    esac
  done
}

mk_dir() {
  local create_dir="$1"  # the target to make
  mkdir -pv "${create_dir}"
  echo "created ${create_dir}"
}

build_ascgen-dev() {
  echo "create build directory and build ascgen-dev";
  mk_dir "${BUILD_PATH}"
  cd "${BUILD_PATH}"
  g++ -v

  ASCEND_INSTALL_LIB_PATH=${ASCEND_INSTALL_PATH}/$(uname -m)-linux/lib64/

  # Set test-related flags based on UT/ST mode
  if [[ "X$ENABLE_UT" = "Xon" ]]; then
    ENABLE_TEST_FLAG="True"
    ENABLE_GE_UT_FLAG="on"
  else
    ENABLE_TEST_FLAG=""
    ENABLE_GE_UT_FLAG=""
  fi

  if [[ "X$ENABLE_ST" = "Xon" ]]; then
    ENABLE_TEST_FLAG="True"
    ENABLE_GE_ST_FLAG="on"
  else
    ENABLE_GE_ST_FLAG=""
  fi

  CMAKE_ARGS="-D CMAKE_C_COMPILER=gcc \
              -D CMAKE_CXX_COMPILER=g++ \
              -D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
              -D CANN_3RD_LIB_PATH=${CANN_3RD_LIB_PATH} \
              -D ASCEND_INSTALL_PATH=${ASCEND_INSTALL_PATH} \
              -D CMAKE_INSTALL_PREFIX=${OUTPUT_PATH} \
              -D ASCEND_INSTALL_LIB_PATH=${ASCEND_INSTALL_LIB_PATH} \
              -D ENABLE_OPEN_SRC=True \
              -D ENABLE_SYMENGINE=True \
              -D RUN_TEST=1 \
              -D BUILD_METADEF=ON \
              -D TESTS_ST_PATH=${TESTS_ST_PATH} \
              -D ENABLE_PKG=${ENABLE_PKG} \
              -D ENABLE_TEST=${ENABLE_TEST_FLAG} \
              -D ENABLE_GE_UT=${ENABLE_GE_UT_FLAG} \
              -D ENABLE_GE_ST=${ENABLE_GE_ST_FLAG} \
              -D ENABLE_LLT_PKG=ON"

  echo "CMAKE_ARGS is: $CMAKE_ARGS"
  ORIGINAL_LD_LIBRARY_PATH=$LD_LIBRARY_PATH
  unset LD_LIBRARY_PATH
  export BUILD_METADEF=${BUILD_METADEF}
  env

  cmake $CMAKE_ARGS ..

  make -j${THREAD_NUM} autofuse
  if [ $? -ne 0 ]
  then
    env
    echo "execute command: make -j autofuse failed."
    return 1
  fi
  mk_dir "${METADEF_LIB_PATH}"
  echo "$(date '+%F %T') cp metadef libs from ${BUILD_PATH} to ${METADEF_LIB_PATH}"
}

build_test() {
  echo "$(date '+%F %T') create build directory and build ascgen-dev";
  cd "${BUILD_PATH}"

  env

  MAKE_TEST_TARGET="test_main"
  make -j${THREAD_NUM} $MAKE_TEST_TARGET

  if [ $? -ne 0 ]
  then
    env
    echo "execute command: make -j test_main failed."
    return 1
  fi

  echo "$(date '+%F %T') run test_main success!"

  cd ${BUILD_PATH}/tests/autofuse/ut/
  RUN_TEST_CASE=${BUILD_PATH}/tests/autofuse/ut/test_main && ${RUN_TEST_CASE}
  if [ $? -ne 0 ]
  then
    echo "execute command: test_main  failed."
    return 1
  fi
  cd -
  echo "$(date '+%F %T') ascgen-dev test success!"
}

build_ut_common () {
  echo "$(date '+%F %T') create build directory and build test_common";
  cd "${BUILD_PATH}"
  make -j${THREAD_NUM} test_common
  if [ $? -ne 0 ]
  then
    echo "execute command: make test_common failed."
    return 1
  fi

  cp ${BUILD_PATH}/tests/autofuse/ut/common/test_common  ${OUTPUT_PATH}
  RUN_TEST_CASE=${OUTPUT_PATH}/test_common && ${RUN_TEST_CASE}
  if [ $? -ne 0 ]
  then
    echo "execute command: run test_common failed."
    return 1
  fi
  echo "$(date '+%F %T') test_common test successfully!"
}

build_ut_autofusion() {
  echo "$(date '+%F %T') create build directory and build autofusion ut";
  cd "${BUILD_PATH}"
  make -j${THREAD_NUM} autofusion_ut
  if [ $? -ne 0 ]
  then
    echo "execute command: make autofusion_ut failed."
    return 1
  fi

  cp ${BUILD_PATH}/tests/autofuse/ut/autofuse/autofusion_ut  ${OUTPUT_PATH}
  RUN_TEST_CASE=${OUTPUT_PATH}/autofusion_ut && ${RUN_TEST_CASE}

  if [ $? -ne 0 ]
  then
    echo "execute command: run autofusion_ut failed."
    return 1
  fi

  echo "$(date '+%F %T') autofusion_ut test successfully!"
}

build_st_autofuse() {
  echo "$(date '+%F %T') create build directory and build autofuse st";
  cd "${BUILD_PATH}"
  make -j${THREAD_NUM} autofusion_st
  if [ $? -ne 0 ]
  then
    echo "execute command: make autofuse_st failed."
    return 1
  fi

  ctest --output-on-failure -j${THREAD_NUM} -L st -L autofusion_st --test-dir ${BUILD_PATH}/tests/autofuse --no-tests=error \
        -O ${BUILD_PATH}/autofusion_st.log
  if [ $? -ne 0 ]
  then
    echo "execute command: run autofuse_st failed."
    return 1
  fi
  echo "$(date '+%F %T') autofuse_st test successfully!"
}

build_st_common() {
  echo "$(date '+%F %T') create build directory and build common st";
  cd "${BUILD_PATH}"
  make -j${THREAD_NUM} test_common_st
  if [ $? -ne 0 ]
  then
    echo "execute command: make common_st failed."
    return 1
  fi

  ctest --output-on-failure -j${THREAD_NUM} -L st -L test_common_st --test-dir ${BUILD_PATH}/tests/autofuse --no-tests=error \
        -O ${BUILD_PATH}/test_common_st.log
  if [ $? -ne 0 ]
  then
    echo "execute command: run common_st failed."
    return 1
  fi
  echo "$(date '+%F %T') common_st test successfully!"
}

get_coverage() {
    echo "Generating coverage statistics, please wait..."
    cd "${BASEPATH}"
    rm -rf ${BASEPATH}/cov
    mk_dir ${BASEPATH}/cov

    # 获取 lcov 2.x 的错误处理参数
    local lcov_ignore_params=""
    local lcov_rc_param=""
    local lcov_parallel_params=""

    if [ "$(get_lcov_major_version)" -ge 2 ] 2>/dev/null; then
        # lcov 2.x 基础错误处理（不含 child，child 只在并行模式有效）
        # 包含 empty 以处理没有 .gcda 文件的目录
        lcov_ignore_params="--ignore-errors inconsistent,negative,mismatch,empty"
        lcov_rc_param="--rc geninfo_unexecuted_blocks=1"
        genhtml_ignore_errors="--ignore-errors inconsistent,corrupt"

        # 如果使用并行模式且版本 >= 2.3，添加 child 错误处理
        lcov_parallel_params=$(get_lcov_parallel_params 4)
        if [ -n "$lcov_parallel_params" ]; then
            lcov_ignore_params="--ignore-errors child,inconsistent,negative,mismatch,empty"
        fi
    else
        genhtml_ignore_errors=""
    fi

    lcov -c \
      -d ${BUILD_RELATIVE_PATH}/ \
      ${lcov_ignore_params} \
      ${lcov_rc_param} \
      ${lcov_parallel_params} \
      -o cov/tmp.info

    echo ${ASCEND_INSTALL_PATH}
    # 注意：使用双引号以展开变量，添加 unused 错误处理
    lcov -r cov/tmp.info "${ASCEND_INSTALL_PATH}/*" '*/output/*' '*/base/metadef/*' '*/nlohmann/*' \
        "*/${BUILD_RELATIVE_PATH}/opensrc/*" "*/${BUILD_RELATIVE_PATH}/proto/*" '*/third_party/*' '/usr/*' \
        ${lcov_ignore_params} --ignore-errors unused \
        ${lcov_rc_param} \
        -o cov/coverage.info
    genhtml cov/coverage.info --output-directory cov/coverage_report ${genhtml_ignore_errors}
}

build_ut() {
  echo "build_ut start, mode = ${MODEL_NAME}."
  case ${MODEL_NAME} in
    "autofuse")
      build_ut_autofusion || { echo "test ut autofusion failed."; exit 1; }
      ;;
    "common")
      build_ut_common || { echo "test ut common failed."; exit 1; }
      ;;
    "ascendc_api")
      ;;
    "framework")
      build_ut_autofusion || { echo "failed to build and run autofusion ut."; exit 1; }
      build_ut_common || { echo "test ut common failed."; exit 1; }
      build_test || { echo "test build failed."; exit 1; }
      ;;
    "all")
      build_ut_autofusion || { echo "failed to build and run autofusion ut."; exit 1; }
      build_ut_common || { echo "test ut common failed."; exit 1; }
      build_test || { echo "test build failed."; exit 1; }
      ;;
    *)
      echo "输入无效，输入范围autofuse/all."
      ;;
  esac
}

build_st() {
  case ${MODEL_NAME} in
    "autofuse")
      build_st_autofuse || { echo "run autofuse st failed."; exit 1; }
      ;;
    "common")
      build_st_common || { echo "run common st failed."; exit 1; }
      ;;
    "e2e")
      ;;
    "ascendc_api")
      ;;
    "framework")
      build_st_autofuse || { echo "run autofuse st failed."; exit 1; }
      build_st_common || { echo "run common st failed."; exit 1; }
      ;;
    "all")
      build_st_autofuse || { echo "run autofuse st failed."; exit 1; }
      build_st_common || { echo "run common st failed."; exit 1; }
      ;;
    *)
      echo "输入无效，输入范围autofuse/common/all."
      ;;
  esac
}

main() {
  cd "${BASEPATH}"
  checkopts "$@"

  export ASCEND_CUSTOM_PATH=${ASCEND_INSTALL_PATH}
  ASCEND_INSTALL_LIB_PATH=${ASCEND_INSTALL_PATH}/$(uname -m)-linux/lib64/

  build_ascgen-dev || { echo "ascgen-dev build failed."; exit 1; }

  METADEF_SOS=$(find ${BUILD_PATH}/graph_metadef -name *.so)
  for METADEF_SO in ${METADEF_SOS}
  do
    cp -rf ${METADEF_SO} ${METADEF_LIB_PATH}
    echo "cp -rf ${METADEF_SO} ${METADEF_LIB_PATH}"
  done

  if [[ "X$ENABLE_UT" = "Xon" ]]; then
    echo "---------------- ascgen-dev build finished ----------------"
    build_ut || { echo "ut build failed."; exit 1; }
  fi

  if [[ "X$ENABLE_ST" = "Xon" ]]; then
    build_st || { echo "st build failed."; exit 1; }
  fi

  echo "---------------- test finished ----------------"

  if [[ "X$ENABLE_COV" = "Xon" ]]; then
    get_coverage
  fi
}

main "$@"
