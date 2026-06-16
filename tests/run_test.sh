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
date +"test begin: %Y-%m-%d %H:%M:%S"

BASEPATH=$(cd "$(dirname $0)/.."; pwd)
OUTPUT_PATH="${BASEPATH}/output"
BUILD_RELATIVE_PATH="build_ut"
BUILD_PATH="${BASEPATH}/${BUILD_RELATIVE_PATH}"
echo "PYTHONPATH:${PYTHONPATH}"
echo "LD_LIBRARY_PATH:${LD_LIBRARY_PATH}"
echo "LD_PRELOAD:${LD_PRELOAD}"

unset PYTHONPATH
# delete ascend dir in LD_LIBRARY_PATH for test
export LD_LIBRARY_PATH=$(echo "$LD_LIBRARY_PATH" | sed -e 's/:*[^:]*Ascend[^:]*:*//g' -e 's/^://' -e 's/:$//')
export LD_LIBRARY_PATH=$(echo "$LD_LIBRARY_PATH" | sed -e 's/:*[^:]*cann[^:]*:*//g' -e 's/^://' -e 's/:$//')
unset LD_PRELOAD

echo "PYTHONPATH:${PYTHONPATH}"
echo "LD_LIBRARY_PATH:${LD_LIBRARY_PATH}"
echo "LD_PRELOAD:${LD_PRELOAD}"

# print usage message
usage() {
  echo "Usage:"
  echo "sh run_test.sh [-u | --ut] [-s | --st]"
  echo "               [-c | --cov] [-j<N>] [-h | --help] [-v | --verbose]"
  echo "               [--cann_3rd_lib_path=<PATH>]"
  echo ""
  echo "Options:"
  echo "    -u, --ut       Build all ut"
  echo "        =ge               Build all ge ut"
  echo "            =ge_common    Build ge common ut"
  echo "            =rt           Build ge runtime ut"
  echo "            =python       Build ge python ut"
  echo "            =parser       Build ge parser ut"
  echo "            =dflow        Build ge dflow ut"
  echo "    -s, --st       Build all st"
  echo "        =ge               Build all ge st"
  echo "            =ge_common    Build ge common st"
  echo "            =rt           Build ge runtime st"
  echo "            =hetero       Build ge hetero st"
  echo "            =python       Build ge python st"
  echo "            =parser       Build ge parser st"
  echo "            =dflow        Build ge dflow st"
  echo "    -h, --help     Print usage"
  echo "    -c, --cov      Build ut/st with coverage tag"
  echo "                   Please ensure that the environment has correctly installed lcov, gcov, and genhtml."
  echo "                   and the version matched gcc/g++."
  echo "    -v, --verbose  Show detailed build commands during the build process"
  echo "    -j<N>          Set the number of threads used for building ut/st, default 16"
  echo "    --cann_3rd_lib_path=<PATH>"
  echo "                   Set third_party package install path, default ./output/third_party"
  echo ""
}


# $1: ENABLE_UT/ENABLE_ST
# $2: ENABLE_ST/ENABLE_UT
# $3: ENABLE_GE
# $4: ENABLE_ENGINES
# $5: input value of ut or st
check_on() {
  if [ "X$1" = "Xon" ]; then
    usage;
    exit 1
  elif [ "X$2" = "Xon" ]; then
    if [ "X$3" != "Xon" ] || [ "X$4" != "Xon" ] || [ -n "$5" ]; then
      usage
      exit 1
    fi
  fi
}


# parse and set options
checkopts() {
  VERBOSE=""
  THREAD_NUM=16
  COVERAGE=""

  ENABLE_UT="off"
  ENABLE_ST="off"

  ENABLE_GE="off"
  ENABLE_GE_COMMON="off"
  ENABLE_RT="off"
  ENABLE_HETERO="off"
  ENABLE_PYTHON="off"
  ENABLE_PARSER="off"
  ENABLE_DFLOW="off"
  ENABLE_ENGINES="off"
  ENABLE_ST_WHOLE_PROCESS="off"
  ENABLE_ACL_UT="off"

  ENABLE_GE_BENCHMARK="off"

  if [ -n "$ASCEND_HOME_PATH" ]; then
    ASCEND_INSTALL_PATH="$ASCEND_HOME_PATH"
  else
    echo "Error: No environment variable 'ASCEND_HOME_PATH' was found, please check the cann environment configuration."
    exit 1
  fi

  ASCEND_3RD_LIB_PATH="$BASEPATH/output/third_party"
  BUILD_METADEF="off"

  parsed_args=$(getopt -a -o u::s::cj:hv -l ut::,st::,process_st::,cov,help,verbose,cann_3rd_lib_path: -- "$@") || {
    usage
    exit 1
  }

  eval set -- "$parsed_args"

  while true; do
    case "$1" in
      -u | --ut)
        check_on "$ENABLE_UT" "$ENABLE_ST" "$ENABLE_GE" "$ENABLE_ENGINES" "$2"
        ENABLE_UT="on"
        case "$2" in
          "acl")
            ENABLE_ACL_UT="on"
            shift 2
            ;;
          "")
            ENABLE_GE="on"
            ENABLE_GE_COMMON="on"
            ENABLE_RT="on"
            ENABLE_PYTHON="on"
            ENABLE_PARSER="on"
            ENABLE_DFLOW="on"
            ENABLE_ENGINES="on"
            shift 2
            ;;
          "ge")
            ENABLE_GE_COMMON="on"
            ENABLE_RT="on"
            ENABLE_PYTHON="on"
            ENABLE_GE="on"
            ENABLE_PARSER="on"
            ENABLE_DFLOW="on"
            shift 2
            ;;
          "ge_common")
            ENABLE_GE_COMMON="on"
            ENABLE_GE="on"
            shift 2
            ;;
          "rt")
            ENABLE_RT="on"
            ENABLE_GE="on"
            shift 2
            ;;
          "python")
            ENABLE_PYTHON="on"
            ENABLE_GE="on"
            shift 2
            ;;
          "parser")
            ENABLE_PARSER="on"
            ENABLE_GE="on"
            shift 2
            ;;
          "dflow")
            ENABLE_DFLOW="on"
            ENABLE_GE="on"
            shift 2
            ;;
          *)
            usage
            exit 1
        esac
        ;;
      -s | --st)
        check_on "$ENABLE_ST" "$ENABLE_UT" "$ENABLE_GE" "$ENABLE_ENGINES" "$2"
        ENABLE_ST="on"
        case "$2" in
          "")
            ENABLE_GE="on"
            ENABLE_GE_COMMON="on"
            ENABLE_RT="on"
            ENABLE_HETERO="on"
            ENABLE_PYTHON="on"
            ENABLE_PARSER="on"
            ENABLE_DFLOW="on"
            shift 2
            ;;
          "ge")
            ENABLE_GE_COMMON="on"
            ENABLE_RT="on"
            ENABLE_HETERO="on"
            ENABLE_PYTHON="on"
            ENABLE_PARSER="on"
            ENABLE_DFLOW="on"
            ENABLE_GE="on"
            shift 2
            ;;
          "ge_common")
            ENABLE_GE_COMMON="on"
            ENABLE_GE="on"
            shift 2
            ;;
          "rt")
            ENABLE_RT="on"
            ENABLE_GE="on"
            shift 2
            ;;
          "hetero")
            ENABLE_HETERO="on"
            ENABLE_GE="on"
            shift 2
            ;;
          "python")
            ENABLE_PYTHON="on"
            ENABLE_GE="on"
            shift 2
            ;;
          "parser")
            ENABLE_PARSER="on"
            ENABLE_GE="on"
            shift 2
            ;;
          "dflow")
            ENABLE_DFLOW="on"
            ENABLE_GE="on"
            shift 2
            ;;
          *)
            usage
            exit 1
        esac
        ;;
      --process_st)
        ENABLE_ST="on"
        ENABLE_ST_WHOLE_PROCESS="on"
        BUILD_METADEF="on"
        shift 2
        ;;
      -c | --cov)
        COVERAGE="-c"
        shift
        ;;
      -h | --help)
        usage
        exit 1
        ;;
      -j)
        THREAD_NUM=$2
        shift 2
        ;;
      -v | --verbose)
        VERBOSE="-v"
        shift
        ;;
      --cann_3rd_lib_path)
        ASCEND_3RD_LIB_PATH="$(realpath $2)"
        shift 2
        ;;
      --)
        shift
        if [ $# -ne 0 ]; then
          echo "ERROR: Undefined parameter detected: $*"
          usage
          exit 1
        fi
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

# 安全地打印测试统计信息（不受 ASAN/LD_PRELOAD 影响）
# 参数：test_names数组元素... -- output_files数组元素...
print_test_summary_safe() {
  local test_names=()
  local output_files=()
  local found_separator=false

  # 解析参数：在 "--" 之前的是 test_names，之后的是 output_files
  for arg in "$@"; do
    if [ "${arg}" = "--" ]; then
      found_separator=true
      continue
    fi
    if [ "${found_separator}" = false ]; then
      test_names+=("${arg}")
    else
      output_files+=("${arg}")
    fi
  done

  echo ""
  echo "=========================================="
  echo "               测试结果汇总"
  echo "=========================================="

  local executed_count=0
  local total_all=0
  local passed_all=0

  # 收集所有数据行
  local output_lines=()
  output_lines+=("测试套件|已执行|总用例数|通过")

  for i in "${!output_files[@]}"; do
    local output_file="${output_files[$i]}"
    local test_name="${test_names[$i]}"
    local executed="否"
    local total_tests="0"
    local passed_tests="0"

    # 安全地读取文件内容，避免受 ASAN 影响
    # 只要文件存在就标记为已执行（即使为空，段错误时可能文件未完全写入）
    if [ -f "${output_file}" ]; then
      executed="是"
      executed_count=$((executed_count + 1))
      local ok_count=0
      # 尝试读取文件内容（即使文件可能为空）
      if [ -s "${output_file}" ]; then
        local total_count=0  # 总测试数（passed + failed + skipped）
        while IFS= read -r line || [ -n "${line}" ]; do
          # 解析 GTest 格式：总测试数
          if [[ "${line}" =~ Running[[:space:]]+([0-9]+)[[:space:]]+test[s]? ]]; then
            total_tests="${BASH_REMATCH[1]}"
          fi
          # 解析 GTest 格式：通过的测试数（汇总行）
          if [[ "${line}" =~ \[[[:space:]]+PASSED[[:space:]]+\][[:space:]]+([0-9]+)[[:space:]]+test[s]? ]]; then
            passed_tests="${BASH_REMATCH[1]}"
          fi
          # 统计每个 [ OK ] 行（段错误时可能没有汇总行，需要逐个统计）
          if [[ "${line}" =~ \[[[:space:]]+OK[[:space:]]+\] ]]; then
            ok_count=$((ok_count + 1))
          fi
          # 解析 pytest 格式：汇总行（例如：2 failed, 371 passed, 2 skipped in 3.80s）
          if [[ "${line}" =~ ([0-9]+)[[:space:]]+failed,[[:space:]]+([0-9]+)[[:space:]]+passed(,[[:space:]]+([0-9]+)[[:space:]]+skipped)? ]]; then
            local failed_num="${BASH_REMATCH[1]}"
            passed_tests="${BASH_REMATCH[2]}"
            local skipped_num=0
            if [ -n "${BASH_REMATCH[4]}" ]; then
              skipped_num="${BASH_REMATCH[4]}"
            fi
            total_tests=$((passed_tests + failed_num + skipped_num))
          fi
          # 统计 pytest 格式：每个测试用例的 PASSED/FAILED/SKIPPED
          # 通过逐行统计来确保即使没有汇总行也能得到准确数据
          if [[ "${line}" =~ PASSED[[:space:]]*$ ]]; then
            ok_count=$((ok_count + 1))
            total_count=$((total_count + 1))
          elif [[ "${line}" =~ FAILED[[:space:]]*$ ]]; then
            total_count=$((total_count + 1))
          elif [[ "${line}" =~ SKIPPED[[:space:]]*$ ]]; then
            total_count=$((total_count + 1))
          fi
        done < "${output_file}"
        if [ "${total_tests}" -eq 0 ] && [ "${total_count}" -gt 0 ]; then
          total_tests=${total_count}
        fi
        if [ "${passed_tests}" -eq 0 ] && [ "${ok_count}" -gt 0 ]; then
          passed_tests=${ok_count}
        fi
      fi
    fi

    total_tests=${total_tests:-0}
    passed_tests=${passed_tests:-0}
    total_tests=$((total_tests + 0))
    passed_tests=$((passed_tests + 0))

    # 如果没有找到Running行，使用passed_tests作为total_tests
    if [ "${total_tests}" -eq 0 ] && [ "${passed_tests}" -gt 0 ]; then
      total_tests=${passed_tests}
    fi

    total_all=$((total_all + total_tests))
    passed_all=$((passed_all + passed_tests))
    output_lines+=("${test_name}|${executed}|${total_tests}|${passed_tests}")
  done

  output_lines+=("总计|${executed_count}|${total_all}|${passed_all}")

  # 纯 bash 实现对齐：计算每列的最大宽度, 不引入额外的column等依赖
  local col_widths=(0 0 0 0)  # 4列：测试套件、已执行、总用例数、通过
  for line in "${output_lines[@]}"; do
    IFS='|' read -ra fields <<< "${line}"
    for i in "${!fields[@]}"; do
      local field="${fields[$i]}"
      local len=${#field}  # 字符长度（对中文也适用）
      if [ "${len}" -gt "${col_widths[$i]}" ]; then
        col_widths[$i]=${len}
      fi
    done
  done

  # 计算分隔线长度（所有列宽 + 列之间的空格）
  local separator_len=$((col_widths[0] + col_widths[1] + col_widths[2] + col_widths[3] + 6))

  # 打印表头
  IFS='|' read -ra header_fields <<< "${output_lines[0]}"
  printf "%-*s  %-*s  %-*s  %-*s\n" \
    "${col_widths[0]}" "${header_fields[0]}" \
    "${col_widths[1]}" "${header_fields[1]}" \
    "${col_widths[2]}" "${header_fields[2]}" \
    "${col_widths[3]}" "${header_fields[3]}"

  # 打印表头分隔线
  printf "%*s\n" "${separator_len}" "" | tr ' ' '-'

  # 打印数据行（跳过表头和总计行）
  local total_lines=${#output_lines[@]}
  for ((i=1; i<total_lines-1; i++)); do
    IFS='|' read -ra fields <<< "${output_lines[$i]}"
    printf "%-*s  %-*s  %*s  %*s\n" \
      "${col_widths[0]}" "${fields[0]}" \
      "${col_widths[1]}" "${fields[1]}" \
      "${col_widths[2]}" "${fields[2]}" \
      "${col_widths[3]}" "${fields[3]}"
  done

  # 打印总计行分隔线
  printf "%*s\n" "${separator_len}" "" | tr ' ' '-'

  # 打印总计行（右对齐数字列）
  IFS='|' read -ra total_fields <<< "${output_lines[-1]}"
  printf "%-*s  %-*s  %*s  %*s\n" \
    "${col_widths[0]}" "${total_fields[0]}" \
    "${col_widths[1]}" "${total_fields[1]}" \
    "${col_widths[2]}" "${total_fields[2]}" \
    "${col_widths[3]}" "${total_fields[3]}"

  echo "=========================================="
  echo ""
}

# 读取并打印所有测试汇总
print_all_tests_summary_from_files() {
  local summary_file_base="$1"
  local all_test_names=()
  local all_output_files=()

  if [ -z "${summary_file_base}" ]; then
    return
  fi

  local summary_file="${summary_file_base}.names"
  if [ ! -f "${summary_file}" ]; then
    return
  fi

  # 读取测试名称
  while IFS= read -r name || [ -n "${name}" ]; do
    [ -n "${name}" ] && all_test_names+=("${name}")
  done < "${summary_file}"

  # 读取对应的输出文件路径
  local files_file="${summary_file_base}.files"
  if [ -f "${files_file}" ]; then
    while IFS= read -r file || [ -n "${file}" ]; do
      [ -n "${file}" ] && all_output_files+=("${file}")
    done < "${files_file}"
  fi

  # 如果有收集到测试信息，统一打印
  if [ ${#all_test_names[@]} -gt 0 ]; then
    print_test_summary_safe "${all_test_names[@]}" -- "${all_output_files[@]}"

    # 清理所有临时输出文件
    for file in "${all_output_files[@]}"; do
      rm -f "${file}" 2>/dev/null || true
    done
  fi

  # 清理汇总文件
  rm -f "${summary_file}" "${files_file}" 2>/dev/null || true
}

build_acl() {
  echo "create build directory and build acl ut";
  mk_dir "${BUILD_PATH}"
  cd "${BUILD_PATH}"

  ENABLE_ACL_COV="$(echo ${ENABLE_ACL_COV} | tr 'a-z' 'A-Z')"
  ENABLE_ACL_UT="$(echo ${ENABLE_ACL_UT} | tr 'a-z' 'A-Z')"
  ENABLE_C_COV="$(echo ${ENABLE_C_COV} | tr 'a-z' 'A-Z')"
  ENABLE_C_UT="$(echo ${ENABLE_C_UT} | tr 'a-z' 'A-Z')"
  CMAKE_ARGS="-DBUILD_OPEN_PROJECT=True \
              -DENABLE_OPEN_SRC=True \
              -DASCENDCL_C=${ASCENDCL_C} \
              -DENABLE_GE_COMPILE=${ENABLE_GE_COMPILE} \
              -DENABLE_ENGINES_COMPILE=${ENABLE_ENGINES_COMPILE} \
              -DENABLE_EXECUTOR_C_COMPILE=${ENABLE_EXECUTOR_C_COMPILE} \
              -DBUILD_METADEF=${BUILD_METADEF} \
              -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} \
              -DCMAKE_INSTALL_PREFIX=${OUTPUT_PATH} \
              -DASCEND_INSTALL_PATH=${ASCEND_INSTALL_PATH} \
              -DASCEND_3RD_LIB_PATH=${ASCEND_3RD_LIB_PATH} \
              -DENABLE_ACL_COV=${ENABLE_ACL_COV} \
              -DENABLE_ACL_UT=${ENABLE_ACL_UT} \
              -DENABLE_C_COV=${ENABLE_C_COV} \
              -DENABLE_C_UT=${ENABLE_C_UT} \
              -DPLATFORM=${PLATFORM} \
              -DPRODUCT=${PRODUCT}"

  echo "CMAKE_ARGS=${CMAKE_ARGS}"
  cmake ${CMAKE_ARGS} ..
  if [ $? -ne 0 ]; then
    echo "execute command: cmake ${CMAKE_ARGS} .. failed."
    return 1
  fi

  make ${VERBOSE} acl_utest -j${THREAD_NUM}
  if [ $? -ne 0 ]; then
    echo "execute command: make ${VERBOSE} -j${THREAD_NUM} failed."
    return 1
  fi
  echo "acl ut build success!"
}


run_ut_acl() {
  cp ${BUILD_PATH}/tests/acl_ut/ut/acl/acl_utest ${OUTPUT_PATH}

  local report_dir="${OUTPUT_PATH}/report/ut" && mk_dir "${report_dir}"
  RUN_TEST_CASE="${OUTPUT_PATH}/acl_utest --gtest_output=xml:${report_dir}/acl_utest.xml" && ${RUN_TEST_CASE}
  if [[ "$?" -ne 0 ]]; then
    echo "!!! UT FAILED, PLEASE CHECK YOUR CHANGES !!!"
    echo -e "\033[31m${RUN_TEST_CASE}\033[0m"
    exit 1;
  fi
  echo "Generated coverage statistics, please wait..."
  cd ${BASEPATH}
  rm -rf ${BASEPATH}/cov
  mkdir ${BASEPATH}/cov
  set +e
  lcov -c -d ${BUILD_RELATIVE_PATH}/tests/acl_ut/ut/acl -o cov/tmp.info --ignore-errors mismatch,empty,negative
  set -e

  if [ ! -s "cov/tmp.info" ] || ! grep -q "SF:" "cov/tmp.info"; then
    echo "No valid cpp coverage data found; skip filtering."
    touch cov/coverage.info
  else
    set +e
    lcov -r cov/tmp.info '*/output/*' "*/${BUILD_RELATIVE_PATH}/opensrc/*" "*/${BUILD_RELATIVE_PATH}/proto/*" \
        '*/third_party/*' '*/tests/*' '/usr/local/*' '/usr/include/*' \
        "${ASCEND_INSTALL_PATH}/*" "${ASCEND_3RD_LIB_PATH}/*" -o cov/coverage.info --ignore-errors mismatch,empty,unused
    set -e
  fi
  cd ${BASEPATH}/cov
  genhtml coverage.info
}

main() {
  cd "${BASEPATH}"
  checkopts "$@"

  # 为当前 run_test.sh 进程创建唯一的汇总文件名
  TEST_SUMMARY_FILE="${OUTPUT_PATH}/.test_summary_$$.tmp"
  export TEST_SUMMARY_FILE

  # build cann 3rd lib
  bash ${BASEPATH}/build_third_party.sh ${ASCEND_3RD_LIB_PATH} ${THREAD_NUM} "LLT"

  # build acl ut
  if [ "X$ENABLE_ACL_UT" = "Xon" ]; then
    echo "---------------- acl ut build start ----------------"
    g++ -v
    mk_dir ${OUTPUT_PATH}
    build_acl
    if [[ "$?" -ne 0 ]]; then
      echo "acl ut build failed.";
      exit 1;
    fi
    echo "---------------- acl ut build finished ----------------"

    rm -f ${OUTPUT_PATH}/libgmock*.so
    rm -f ${OUTPUT_PATH}/libgtest*.so
    rm -f ${OUTPUT_PATH}/lib*_stub.so

    chmod -R 750 ${OUTPUT_PATH}

    echo "---------------- acl ut output generated ----------------"
    run_ut_acl
    exit 0
  fi

  if [ "X$ENABLE_UT" != "Xon" ] && [ "X$ENABLE_ST" != "Xon" ]; then
    ENABLE_UT="on"
    ENABLE_ST="on"
    ENABLE_GE="on"
    ENABLE_GE_COMMON="on"
    ENABLE_RT="on"
    ENABLE_HETERO="on"
    ENABLE_PYTHON="on"
    ENABLE_PARSER="on"
    ENABLE_DFLOW="on"
    ENABLE_ENGINES="on"
  fi

  export BUILD_METADEF=${BUILD_METADEF}
  export ASCEND_INSTALL_PATH=${ASCEND_INSTALL_PATH}
  export ASCEND_3RD_LIB_PATH=${ASCEND_3RD_LIB_PATH}

  if [ ! -f ${ASCEND_INSTALL_PATH}/fwkacllib/lib64/switch_by_index.o ]; then
    mkdir -p ${ASCEND_INSTALL_PATH}/fwkacllib/lib64/
    touch ${ASCEND_INSTALL_PATH}/fwkacllib/lib64/switch_by_index.o
  fi

  # 用于记录是否有测试失败（即使失败也要先打印汇总后退出）
  local test_failed=0
  
  # module ge
  if [ "X$ENABLE_GE" = "Xon" ]; then
    # ge ut
    if [ "X$ENABLE_UT" == "Xon" ]; then
      set +e
      if [ "X$ENABLE_GE_COMMON" = "Xon" ] && [ "X$ENABLE_RT" = "Xon" ] && [ "X$ENABLE_PYTHON" = "Xon" ] && [ "X$ENABLE_PARSER" = "Xon" ] && [ "X$ENABLE_DFLOW" = "Xon" ]; then
        bash scripts/build_fwk.sh -t -j $THREAD_NUM $VERBOSE $COVERAGE || test_failed=1
      elif [ "X$ENABLE_GE_COMMON" = "Xon" ]; then
        bash scripts/build_fwk.sh -T -j $THREAD_NUM $VERBOSE $COVERAGE || test_failed=1
        bash scripts/build_fwk.sh -d -j $THREAD_NUM $VERBOSE $COVERAGE || test_failed=1
      elif [ "X$ENABLE_RT" = "Xon" ]; then
        bash scripts/build_fwk.sh -L -j $THREAD_NUM $VERBOSE $COVERAGE || test_failed=1
      elif [ "X$ENABLE_PYTHON" = "Xon" ]; then
        bash scripts/build_fwk.sh -l -j $THREAD_NUM $VERBOSE $COVERAGE || test_failed=1
      elif [ "X$ENABLE_PARSER" = "Xon" ]; then
        bash scripts/build_fwk.sh -m -j $THREAD_NUM $VERBOSE $COVERAGE || test_failed=1
      elif [ "X$ENABLE_DFLOW" = "Xon" ]; then
        bash scripts/build_fwk.sh -o -j $THREAD_NUM $VERBOSE $COVERAGE || test_failed=1
      else
        echo "unknown ut type."
      fi
      set -e
      
      # 如果 GE UT 失败，打印汇总后立即退出
      if [ "${test_failed}" -ne 0 ]; then
        print_all_tests_summary_from_files "${TEST_SUMMARY_FILE}"
        unset TEST_SUMMARY_FILE
        exit 1
      fi
    fi

    # ge st
    if [ "X$ENABLE_ST" = "Xon" ]; then
      set +e
      if [ "X$ENABLE_GE_COMMON" = "Xon" ] && [ "X$ENABLE_RT" = "Xon" ] && [ "X$ENABLE_HETERO" = "Xon" ] && [ "X$ENABLE_PYTHON" == "Xon" ] && [ "X$ENABLE_PARSER" == "Xon" ] && [ "X$ENABLE_DFLOW" == "Xon" ]; then
        bash scripts/build_fwk.sh -s -j $THREAD_NUM $VERBOSE $COVERAGE || test_failed=1
      elif [ "X$ENABLE_GE_COMMON" = "Xon" ]; then
        bash scripts/build_fwk.sh -O -j $THREAD_NUM $VERBOSE $COVERAGE || test_failed=1
      elif [ "X$ENABLE_RT" = "Xon" ]; then
        bash scripts/build_fwk.sh -R -j $THREAD_NUM $VERBOSE $COVERAGE || test_failed=1
      elif [ "X$ENABLE_HETERO" = "Xon" ]; then
        bash scripts/build_fwk.sh -K -j $THREAD_NUM $VERBOSE $COVERAGE || test_failed=1
      elif [ "X$ENABLE_PYTHON" = "Xon" ]; then
        bash scripts/build_fwk.sh -P -j $THREAD_NUM $VERBOSE $COVERAGE || test_failed=1
      elif [ "X$ENABLE_PARSER" = "Xon" ]; then
        bash scripts/build_fwk.sh -n -j $THREAD_NUM $VERBOSE $COVERAGE || test_failed=1
      elif [ "X$ENABLE_DFLOW" = "Xon" ]; then
        bash scripts/build_fwk.sh -D -j $THREAD_NUM $VERBOSE $COVERAGE || test_failed=1
      else
        echo "unknown type st."
      fi
      set -e
      
      # 如果 GE ST 失败，打印汇总后立即退出
      if [ "${test_failed}" -ne 0 ]; then
        print_all_tests_summary_from_files "${TEST_SUMMARY_FILE}"
        unset TEST_SUMMARY_FILE
        exit 1
      fi
    fi
  fi

  # 统一打印所有测试汇总（从 build_fwk.sh 保存的文件中读取）
  print_all_tests_summary_from_files "${TEST_SUMMARY_FILE}"
  unset TEST_SUMMARY_FILE
  date +"test end: %Y-%m-%d %H:%M:%S"
}

main "$@"
