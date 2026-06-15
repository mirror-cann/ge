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

sourcedir="$PWD"
curpath=$(dirname $(readlink -f "$0"))
common_func_path="${curpath}/common_func.inc"
ge_compiler_func_path="${curpath}/ge-compiler_func.sh"
unset PYTHONPATH

. "${common_func_path}"
. "${ge_compiler_func_path}"

common_parse_dir=""
logfile=""
stage=""
is_quiet="n"
pylocal="n"
hetero_arch="n"

while true; do
    case "$1" in
    --install-path=*)
        pkg_install_path=$(echo "$1" | cut -d"=" -f2-)
        shift
        ;;
    --common-parse-dir=*)
        common_parse_dir=$(echo "$1" | cut -d"=" -f2-)
        shift
        ;;
    --version-dir=*)
        pkg_version_dir=$(echo "$1" | cut -d"=" -f2-)
        shift
        ;;
    --logfile=*)
        logfile=$(echo "$1" | cut -d"=" -f2)
        shift
        ;;
    --stage=*)
        stage=$(echo "$1" | cut -d"=" -f2)
        shift
        ;;
    --quiet=*)
        is_quiet=$(echo "$1" | cut -d"=" -f2)
        shift
        ;;
    --pylocal=*)
        pylocal=$(echo "$1" | cut -d"=" -f2)
        shift
        ;;
    --hetero-arch=*)
        hetero_arch=$(echo "$1" | cut -d"=" -f2)
        shift
        ;;
    -*)
        shift
        ;;
    *)
        break
        ;;
    esac
done

# 写日志
log() {
    local cur_date="$(date +'%Y-%m-%d %H:%M:%S')"
    local log_type="$1"
    shift
    if [ "$log_type" = "INFO" -o "$log_type" = "WARNING" -o "$log_type" = "ERROR" ]; then
        echo -e "[ge-compiler] [$cur_date] [$log_type]: $*"
    else
        echo "[ge-compiler] [$cur_date] [$log_type]: $*" 1> /dev/null
    fi
    echo "[ge-compiler] [$cur_date] [$log_type]: $*" >> "$logfile"
}

ge_compiler_install_package() {
    local _package="$1"
    local _pythonlocalpath="$2"
    log "INFO" "install python module package in ${_package}"
    if ! ge_compiler_has_python_installer; then
        log "ERROR" "install ${_package} failed, python3 -m pip or pip3 is not installed."
        exit 1
    fi
    if [ -f "$_package" ]; then
        if [ "$pylocal" = "y" ]; then
            ge_compiler_run_pip install --disable-pip-version-check --upgrade --no-deps --force-reinstall "${_package}" -t "${_pythonlocalpath}" 1> /dev/null
        else
            if [ $(id -u) -ne 0 ]; then
                ge_compiler_run_pip install --disable-pip-version-check --upgrade --no-deps --force-reinstall "${_package}" --user 1> /dev/null
            else
                ge_compiler_run_pip install --disable-pip-version-check --upgrade --no-deps --force-reinstall "${_package}" 1> /dev/null
            fi
        fi
        local ret=$?
        if [ $ret -ne 0 ]; then
            log "WARNING" "install ${_package} failed, error code: $ret."
            exit 1
        else
            log "INFO" "install ${_package} successfully!"
        fi
    else
        log "ERROR" "ERR_NO:0x0080;ERR_DES:install ${_package} faied, can not find the matched package for this platform."
        exit 1
    fi
}

ge_compiler_has_python_installer() {
    if command -v python3 >/dev/null 2>&1 && python3 -m pip --version >/dev/null 2>&1; then
        return 0
    fi
    command -v pip3 >/dev/null 2>&1
}

ge_compiler_run_pip() {
    if command -v python3 >/dev/null 2>&1 && python3 -m pip --version >/dev/null 2>&1; then
        python3 -m pip "$@"
    else
        pip3 "$@"
    fi
}

ge_compiler_get_python_tag() {
    local _tag
    if command -v python3 >/dev/null 2>&1 && python3 -m pip --version >/dev/null 2>&1; then
        _tag=$(python3 -c 'import sys; print("cp%d%d" % sys.version_info[:2])' 2>/dev/null)
    fi
    if [ -z "${_tag}" ]; then
        _tag=$(pip3 --version 2>/dev/null | sed -n 's/.*(python \([0-9][0-9]*\)\.\([0-9][0-9]*\)).*/cp\1\2/p' | head -n 1)
    fi
    echo "${_tag}"
}

ge_compiler_get_python_info() {
    local _pip_info
    local _python_pip_info
    local _python_info
    if command -v python3 >/dev/null 2>&1; then
        _python_pip_info=$(python3 -m pip --version 2>/dev/null)
    fi
    _pip_info=$(pip3 --version 2>/dev/null)
    if command -v python3 >/dev/null 2>&1; then
        _python_info=$(python3 -c 'import sys; print("%s %d.%d.%d" % (sys.executable, sys.version_info[0], sys.version_info[1], sys.version_info[2]))' 2>/dev/null)
    fi
    if [ -n "${_python_info}" ]; then
        echo "python3 ${_python_info}; python3 -m pip ${_python_pip_info}; pip3 ${_pip_info}"
    else
        echo "pip3 ${_pip_info}"
    fi
}

ge_compiler_list_bridge_python_tags() {
    local _wheel_dir="$1"
    local _tags=""
    local _wheel
    local _base
    local _tag
    for _wheel in "${_wheel_dir}"/ge_py_pass_bridge-*.whl; do
        [ -f "${_wheel}" ] || continue
        _base=$(basename "${_wheel}")
        _tag=$(echo "${_base}" | sed -n 's/^ge_py_pass_bridge-[^-]*-\(cp[0-9][0-9]*\)-\1-.*/\1/p')
        if [ -n "${_tag}" ]; then
            case " ${_tags} " in
                *" ${_tag} "*) ;;
                *) _tags="${_tags} ${_tag}" ;;
            esac
        fi
    done
    if [ -z "${_tags# }" ]; then
        echo "none"
    else
        echo "${_tags# }"
    fi
}

ge_compiler_find_bridge_wheel() {
    local _wheel_dir="$1"
    local _python_tag="$2"
    local _wheel
    [ -n "${_python_tag}" ] || return 1
    for _wheel in "${_wheel_dir}"/ge_py_pass_bridge-*-"${_python_tag}"-"${_python_tag}"-*.whl; do
        if [ -f "${_wheel}" ]; then
            echo "${_wheel}"
            return 0
        fi
    done
    return 1
}

ge_compiler_clean_python_pass_artifacts() {
    local _pythonlocalpath="$1"
    if [ "$pylocal" != "y" ] || [ -z "${_pythonlocalpath}" ]; then
        return 0
    fi
    chmod u+w "${_pythonlocalpath}/ge/passes" 2> /dev/null
    chmod u+w -R "${_pythonlocalpath}/ge/passes/python_pass_artifacts" 2> /dev/null
    rm -fr "${_pythonlocalpath}/ge/passes/python_pass_artifacts" 2> /dev/null
}

ge_compiler_python_pass_artifacts_complete() {
    local _pythonlocalpath="$1"
    local _artifact_root="${_pythonlocalpath}/ge/passes/python_pass_artifacts"
    local _artifact_dir
    [ -d "${_artifact_root}" ] || return 1
    for _artifact_dir in "${_artifact_root}"/*; do
        if [ -d "${_artifact_dir}" ] &&
            [ -f "${_artifact_dir}/manifest.json" ] &&
            [ -f "${_artifact_dir}/libge_python_pass_bridge.so" ] &&
            [ -f "${_artifact_dir}/_ge_pass_native.so" ]; then
            return 0
        fi
    done
    return 1
}

ge_compiler_extract_bridge_wheel() {
    local _bridge_wheel="$1"
    local _target_dir="$2"
    if command -v python3 >/dev/null 2>&1; then
        python3 -m zipfile -e "${_bridge_wheel}" "${_target_dir}" 2> /dev/null && return 0
    fi
    if command -v unzip >/dev/null 2>&1; then
        unzip -oq "${_bridge_wheel}" -d "${_target_dir}" 2> /dev/null
        return $?
    fi
    return 1
}

ge_compiler_fix_python_pass_artifacts() {
    local _pythonlocalpath="$1"
    local _bridge_wheel="$2"
    if [ "$pylocal" != "y" ] || [ -z "${_pythonlocalpath}" ] || [ ! -d "${_pythonlocalpath}" ] ||
        [ -z "${_bridge_wheel}" ] || [ ! -f "${_bridge_wheel}" ]; then
        return 0
    fi
    if ge_compiler_python_pass_artifacts_complete "${_pythonlocalpath}"; then
        return 0
    fi

    log "INFO" "python pass native artifacts are incomplete, extract ${_bridge_wheel} to fix."
    if ! ge_compiler_extract_bridge_wheel "${_bridge_wheel}" "${_pythonlocalpath}"; then
        log "WARNING" "extract ${_bridge_wheel} failed."
        return 1
    fi
    if ! ge_compiler_python_pass_artifacts_complete "${_pythonlocalpath}"; then
        log "WARNING" "fix python pass native artifacts failed."
        return 1
    fi
    log "INFO" "python pass native artifacts fixed successfully."
    return 0
}

ge_compiler_install_ge_package() {
    local _ge_package="$1"
    local _wheel_dir="$2"
    local _pythonlocalpath="$3"
    local _bridge_requirement="ge-py-pass-bridge==0.0.1"
    local _python_tag
    local _available_bridge_tags
    local _bridge_wheel
    log "INFO" "install python module packages in ${_ge_package} and ${_bridge_requirement}"
    if ! ge_compiler_has_python_installer; then
        log "ERROR" "install ${_ge_package} failed, python3 -m pip or pip3 is not installed."
        exit 1
    fi
    if [ ! -f "$_ge_package" ]; then
        log "ERROR" "ERR_NO:0x0080;ERR_DES:install ${_ge_package} faied, can not find the matched package for this platform."
        exit 1
    fi
    ge_compiler_clean_python_pass_artifacts "${_pythonlocalpath}"
    _python_tag=$(ge_compiler_get_python_tag)
    _available_bridge_tags=$(ge_compiler_list_bridge_python_tags "${_wheel_dir}")
    log "INFO" "python package installer: $(ge_compiler_get_python_info); expected native tag: ${_python_tag:-unknown}; available native wheel tags: ${_available_bridge_tags}."
    if ! ls "${_wheel_dir}"/ge_py_pass_bridge-*.whl >/dev/null 2>&1; then
        log "WARNING" "can not find native wheel packages in ${_wheel_dir}, install ${_ge_package} only."
        ge_compiler_install_package "${_ge_package}" "${_pythonlocalpath}"
        return 0
    fi
    if [ -z "${_python_tag}" ]; then
        log "WARNING" "can not detect python tag, install ${_ge_package} only. Available native wheel tags: ${_available_bridge_tags}."
        ge_compiler_install_package "${_ge_package}" "${_pythonlocalpath}"
        return 0
    fi
    _bridge_wheel=$(ge_compiler_find_bridge_wheel "${_wheel_dir}" "${_python_tag}")
    if [ -z "${_bridge_wheel}" ]; then
        log "WARNING" "can not find native wheel package for ${_python_tag} in ${_wheel_dir}. Available native wheel tags: ${_available_bridge_tags}. Install ${_ge_package} only."
        ge_compiler_install_package "${_ge_package}" "${_pythonlocalpath}"
        return 0
    fi

    if [ "$pylocal" = "y" ]; then
        ge_compiler_run_pip install --disable-pip-version-check --upgrade --no-index --no-deps \
            --force-reinstall "${_ge_package}" "${_bridge_wheel}" -t "${_pythonlocalpath}" 1> /dev/null
    else
        if [ $(id -u) -ne 0 ]; then
            ge_compiler_run_pip install --disable-pip-version-check --upgrade --no-index --no-deps \
                --force-reinstall "${_ge_package}" "${_bridge_wheel}" --user 1> /dev/null
        else
            ge_compiler_run_pip install --disable-pip-version-check --upgrade --no-index --no-deps \
                --force-reinstall "${_ge_package}" "${_bridge_wheel}" 1> /dev/null
        fi
    fi
    local ret=$?
    if [ $ret -ne 0 ]; then
        log "WARNING" "install ${_bridge_requirement} failed, error code: $ret. Install ${_ge_package} only."
        ge_compiler_install_package "${_ge_package}" "${_pythonlocalpath}"
        return 0
    else
        log "INFO" "install ${_ge_package} and ${_bridge_requirement} successfully!"
    fi
    ge_compiler_fix_python_pass_artifacts "${_pythonlocalpath}" "${_bridge_wheel}"
}

WHL_INSTALL_DIR_PATH="${common_parse_dir}/python/site-packages"
PYTHON_LLM_DATADIST_WHL="${sourcedir}/${pkg_arch_name}-linux/lib64/llm_datadist_v1-0.0.1-py3-none-any.whl"
PYTHON_DATAFLOW_WHL="${sourcedir}/${pkg_arch_name}-linux/lib64/dataflow-0.0.1-py3-none-any.whl"
PYTHON_GE_WHL="${sourcedir}/${pkg_arch_name}-linux/lib64/ge_py-0.0.1-py3-none-any.whl"
PYTHON_GE_WHL_DIR="${sourcedir}/${pkg_arch_name}-linux/lib64"

custom_install() {
    if [ -z "$common_parse_dir/share/info/ge-compiler" ]; then
        log "ERROR" "ERR_NO:0x0001;ERR_DES:ge-compiler directory is empty"
        exit 1
    fi

    if [ "$hetero_arch" != "y" ]; then
        log "INFO" "install ge-compiler extension module begin..."
        ge_compiler_install_package "${PYTHON_LLM_DATADIST_WHL}" "${WHL_INSTALL_DIR_PATH}"
        ge_compiler_install_package "${PYTHON_DATAFLOW_WHL}" "${WHL_INSTALL_DIR_PATH}"
        ge_compiler_install_ge_package "${PYTHON_GE_WHL}" "${PYTHON_GE_WHL_DIR}" "${WHL_INSTALL_DIR_PATH}"
        log "INFO" "the ge-compiler extension module installed successfully!"

        if [ "${pylocal}" = "y" ]; then
            log "INFO" "please make sure PYTHONPATH include ${WHL_INSTALL_DIR_PATH}."
        else
            log "INFO" "The package te is already installed in python default path. It is recommended to install it using the '--pylocal' parameter, install the package ge-compiler in the ${WHL_INSTALL_DIR_PATH}."
        fi

        if [ "x$stage" = "xinstall" ]; then
            log "INFO" "ge-compiler do migrate user assets."
            if [ $? -ne 0 ]; then
                log "WARNING" "failed to copy custom directories."
                return 1
            fi
        fi
    fi

    return 0
}

custom_install
if [ $? -ne 0 ]; then
    exit 1
fi

# copy_all模式下do_chmod_file_dir跳过了copy类型文件的chmod，
# 需要在此处补充设置文件权限
if [ -n "${common_parse_dir}" ]; then
    chmod 550 "${common_parse_dir}/fwkacllib/lib64/switch_by_index.o" 2>/dev/null
    chmod 550 "${common_parse_dir}/python/site-packages/autofuse/"*.py 2>/dev/null
    chmod 440 "${common_parse_dir}/python/site-packages/autofuse/pyautofuse.so" 2>/dev/null
    chmod 640 "${common_parse_dir}/${pkg_arch_name}-linux/lib64/plugin/opskernel/config/init.conf" 2>/dev/null
    chmod 550 "${common_parse_dir}/${pkg_arch_name}-linux/python/func2graph/func2graph.py" 2>/dev/null
fi

exit 0
