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

BASEPATH=$(cd "$(dirname $0)/.."; pwd)
OUTPUT_PATH="${BASEPATH}/output"
BUILD_RELATIVE_PATH="build"

# print usage message
usage() {
  echo "Usage:"
  echo "  sh run_test.sh [-h | --help] [-v | --verbose] [-j<N>] [-u | --ut] [-b | --benchmark] [-c | --cov]"
  echo "                 [--ascend_install_path=<PATH>] [--ascend_3rd_lib_path=<PATH>]"
  echo ""
  echo "Options:"
  echo "    -h, --help       Print usage"
  echo "    -v, --verbose    Display build command"
  echo "    -j<N>            Set the number of threads used for building Metadef llt, default 8"
  echo "    -u, --ut         Build and execute ut"
  echo "    -b, --benchmark  Run Benchmark test"
  echo "    -c, --cov        Build ut with coverage tag"
  echo "                     Please ensure that the environment has correctly installed lcov, gcov, and genhtml."
  echo "                     and the version matched gcc/g++."
  echo "        --ascend_install_path=<PATH>"
  echo "                     Set ascend package install path, default /usr/local/Ascend/ascend-toolkit/latest"
  echo "        --ascend_3rd_lib_path=<PATH>"
  echo "                     Set ascend third_party package install path, default ./output/third_party"
  echo ""
}

# parse and set options
checkopts() {
  VERBOSE=""
  THREAD_NUM=8
  ENABLE_METADEF_UT="off"
  ENABLE_METADEF_ST="off"
  ENABLE_METADEF_COV="off"
  ENABLE_BENCHMARK="off"
  GE_ONLY="on"
  ASCEND_INSTALL_PATH="/usr/local/Ascend/ascend-toolkit/latest"
  ASCEND_3RD_LIB_PATH="$BASEPATH/output/third_party"
  CMAKE_BUILD_TYPE="Release"

  # Process the options
  parsed_args=$(getopt -a -o ubcj:hv -l ut,benchmark,cov,help,verbose,ascend_install_path:,ascend_3rd_lib_path: -- "$@") || {
    usage
    exit 1
  }

  eval set -- "$parsed_args"

  while true; do
    case "$1" in
      -u | --ut)
        ENABLE_METADEF_UT="on"
        GE_ONLY="off"
        shift
        ;;
      -b | --benchmark)
        ENABLE_BENCHMARK="on"
        GE_ONLY="off"
        shift
        ;;
      -c | --cov)
        ENABLE_METADEF_COV="on"
        GE_ONLY="off"
        shift
        ;;
      -h | --help)
        usage
        exit 0
        ;;
      -j)
        THREAD_NUM="$2"
        shift 2
        ;;
      -v | --verbose)
        VERBOSE="VERBOSE=1"
        shift
        ;;
      --ascend_install_path)
        ASCEND_INSTALL_PATH="$(realpath $2)"
        shift 2
        ;;
      --ascend_3rd_lib_path)
        ASCEND_3RD_LIB_PATH="$(realpath $2)"
        shift 2
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

# Metadef llt build start
cmake_generate_make() {
  local build_path="$1"
  local cmake_args="$2"
  mk_dir "${build_path}"
  cd "${build_path}"
  echo "${cmake_args}"
  cmake ${cmake_args} ..
  if [ 0 -ne $? ]; then
    echo "execute command: cmake ${cmake_args} .. failed."
    exit 1
  fi
}

# create build path
build_metadef() {
  echo "create build directory and build Metadef llt"
  cd "${BASEPATH}"

  if [[ "X$ENABLE_METADEF_UT" = "Xon" || "X$ENABLE_METADEF_COV" = "Xon" || "X$ENABLE_BENCHMARK" = "Xon" ]]; then
    BUILD_RELATIVE_PATH="build_gcov"
    CMAKE_BUILD_TYPE="GCOV"
  fi

  BUILD_PATH="${BASEPATH}/${BUILD_RELATIVE_PATH}/"
  CMAKE_ARGS="-D GE_ONLY=$GE_ONLY \
              -D ENABLE_OPEN_SRC=True \
              -D ENABLE_METADEF_UT=${ENABLE_METADEF_UT} \
              -D ENABLE_METADEF_ST=${ENABLE_METADEF_ST} \
              -D ENABLE_METADEF_COV=${ENABLE_METADEF_COV} \
              -D ENABLE_BENCHMARK=${ENABLE_BENCHMARK} \
              -D BUILD_WITHOUT_AIR=True \
              -D ASCEND_INSTALL_PATH=${ASCEND_INSTALL_PATH} \
              -D ASCEND_3RD_LIB_PATH=${ASCEND_3RD_LIB_PATH} \
              -D CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} \
              -D CMAKE_INSTALL_PREFIX=${OUTPUT_PATH}"

  echo "CMAKE_ARGS is: $CMAKE_ARGS"
  cmake_generate_make "${BUILD_PATH}" "${CMAKE_ARGS}"

  if [[ "X$ENABLE_METADEF_UT" = "Xon" || "X$ENABLE_METADEF_COV" = "Xon" ]]; then
    make ut_metadef ut_graph ut_register ut_error_manager ut_exe_graph ut_exe_meta_device ut_ascendc_ir ut_expression ut_sc_check ${VERBOSE} -j${THREAD_NUM}
  elif [ "X$ENABLE_BENCHMARK" = "Xon" ]; then
    make exec_graph_benchmark fast_graph_benchmark ut_ascendc_ir ut_expression ${VERBOSE} -j${THREAD_NUM}
  fi

  if [ 0 -ne $? ]; then
    echo "execute command: make ${VERBOSE} -j${THREAD_NUM} && make install failed."
    return 1
  fi
  echo "Metadef build success!"
}

main() {
  cd "${BASEPATH}"
  checkopts "$@"

  if [ "X$ENABLE_METADEF_UT" != "Xon" ] && [ "X$ENABLE_BENCHMARK" != "Xon" ]; then
    ENABLE_METADEF_UT=on
  fi

  g++ -v
  mk_dir ${OUTPUT_PATH}
  echo "---------------- Metadef llt build start ----------------"
  build_metadef || { echo "Metadef llt build failed."; exit 1; }
  echo "---------------- Metadef llt build finished ----------------"

  if [ "X$ENABLE_BENCHMARK" = "Xon" ]; then
    RUN_TEST_CASE=${BUILD_PATH}/tests/benchmark/exe_graph/exec_graph_benchmark && ${RUN_TEST_CASE}
    RUN_TEST_CASE=${BUILD_PATH}/tests/benchmark/fast_graph/fast_graph_benchmark && ${RUN_TEST_CASE}
  fi

  if [[ "X$ENABLE_METADEF_UT" = "Xon" || "X$ENABLE_METADEF_COV" = "Xon" ]]; then
    cp ${BUILD_PATH}/tests/ge/ut/base/ut_metadef ${OUTPUT_PATH}
    cp ${BUILD_PATH}/tests/ge/ut/graph/ut_graph ${OUTPUT_PATH}
    cp ${BUILD_PATH}/tests/ge/ut/register/ut_register ${OUTPUT_PATH}
    cp ${BUILD_PATH}/tests/ge/ut/error_manager/ut_error_manager ${OUTPUT_PATH}
    cp ${BUILD_PATH}/tests/ge/ut/exe_graph/ut_exe_graph ${OUTPUT_PATH}
    cp ${BUILD_PATH}/tests/ge/ut/ascendc_ir/ut_ascendc_ir ${OUTPUT_PATH}
    cp ${BUILD_PATH}/tests/ge/ut/expression/ut_expression ${OUTPUT_PATH}
    cp ${BUILD_PATH}/tests/ge/ut/sc_check/ut_sc_check ${OUTPUT_PATH}
    export ASAN_OPTIONS=detect_container_overflow=0
    RUN_TEST_CASE=${OUTPUT_PATH}/ut_metadef && ${RUN_TEST_CASE} &&
    RUN_TEST_CASE=${OUTPUT_PATH}/ut_graph && ${RUN_TEST_CASE} &&
    RUN_TEST_CASE=${OUTPUT_PATH}/ut_register && ${RUN_TEST_CASE} &&
    RUN_TEST_CASE=${OUTPUT_PATH}/ut_error_manager && ${RUN_TEST_CASE} &&
    RUN_TEST_CASE=${OUTPUT_PATH}/ut_exe_graph && ${RUN_TEST_CASE} &&
    RUN_TEST_CASE=${OUTPUT_PATH}/ut_ascendc_ir && ${RUN_TEST_CASE} &&
    RUN_TEST_CASE=${OUTPUT_PATH}/ut_expression && ${RUN_TEST_CASE} &&
    RUN_TEST_CASE=${OUTPUT_PATH}/ut_sc_check && ${RUN_TEST_CASE}
    if [[ "$?" -ne 0 ]]; then
      echo "!!! UT FAILED, PLEASE CHECK YOUR CHANGES !!!"
      echo -e "\033[31m${RUN_TEST_CASE}\033[0m"
      exit 1
    fi
    unset ASAN_OPTIONS

    if [[ "X$ENABLE_METADEF_COV" = "Xon" ]]; then
      echo "Generating coverage statistics, please wait..."
      cd "${BASEPATH}"
      rm -rf ${BASEPATH}/cov
      mk_dir ${BASEPATH}/cov
      lcov -c \
        -d ${BUILD_RELATIVE_PATH}/base/CMakeFiles/metadef.dir \
        -d ${BUILD_RELATIVE_PATH}/base/CMakeFiles/opp_registry.dir \
        -d ${BUILD_RELATIVE_PATH}/graph/CMakeFiles/graph.dir \
        -d ${BUILD_RELATIVE_PATH}/graph/CMakeFiles/graph_base.dir \
        -d ${BUILD_RELATIVE_PATH}/graph/ascendc_ir/CMakeFiles/aihac_ir.dir \
        -d ${BUILD_RELATIVE_PATH}/graph/expression/CMakeFiles/aihac_symbolizer.dir \
        -d ${BUILD_RELATIVE_PATH}/graph/ascendc_ir/generator/CMakeFiles/ascir_generate.dir \
        -d ${BUILD_RELATIVE_PATH}/graph/ascendc_ir/generator/CMakeFiles/aihac_ir_register.dir \
        -d ${BUILD_RELATIVE_PATH}/graph/ascendc_ir/generator/CMakeFiles/ascir_ops_header_generator.dir \
        -d ${BUILD_RELATIVE_PATH}/register/CMakeFiles/register.dir \
        -d ${BUILD_RELATIVE_PATH}/register/CMakeFiles/rt2_registry_objects.dir \
        -d ${BUILD_RELATIVE_PATH}/error_manager/CMakeFiles/error_manager.dir \
        -d ${BUILD_RELATIVE_PATH}/base/CMakeFiles/exe_graph.dir \
        -d ${BUILD_RELATIVE_PATH}/exe_graph/CMakeFiles/lowering.dir \
        -d ${BUILD_RELATIVE_PATH}/tests/ge/ut/exe_graph/CMakeFiles/ut_exe_graph.dir \
        -o cov/tmp.info
      lcov -r cov/tmp.info '*/output/*' "*/${BUILD_RELATIVE_PATH}/opensrc/*" "*/${BUILD_RELATIVE_PATH}/proto/*" \
                           '*/third_party/*' '*/tests/*' '/usr/*' \
                           "${ASCEND_INSTALL_PATH}/*" "${ASCEND_3RD_LIB_PATH}/*" -o cov/coverage.info --ignore-errors mismatch,empty,unused
      cd "${BASEPATH}/cov"
      genhtml coverage.info
    fi
  fi

  echo "---------------- Metadef llt finished ----------------"
}

main "$@"
