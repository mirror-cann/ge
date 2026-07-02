#!/bin/bash
set -e
# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

sourcedir="${INSTALL_PATH}"
pkg_arch_name="${PKG_ARCH_NAME}"
# WHL_INSTALL_DIR_PATH=$(python3 -c "import sysconfig; print(sysconfig.get_path('purelib'))" 2>/dev/null)
WHL_INSTALL_DIR_PATH="${sourcedir}/python/site-packages"
unset PYTHONPATH
export PIP_BREAK_SYSTEM_PACKAGES=1

WHL_LLM_DATADIST="${sourcedir}/${pkg_arch_name}-linux/lib64/llm_datadist_v1-0.0.1-py3-none-any.whl"
WHL_DATAFLOW="${sourcedir}/${pkg_arch_name}-linux/lib64/dataflow-0.0.1-py3-none-any.whl"
WHL_GE_PY="${sourcedir}/${pkg_arch_name}-linux/lib64/ge_py-0.0.1-py3-none-any.whl"
WHL_BRIDGE_DIR="${sourcedir}/${pkg_arch_name}-linux/lib64"

run_pip() { python3 -m pip "$@" || pip3 "$@"; }

for whl in "${WHL_LLM_DATADIST}" "${WHL_DATAFLOW}"; do
    [ -f "${whl}" ] && echo "[ge-compiler] installing ${whl}" && run_pip install --disable-pip-version-check --upgrade --no-deps --force-reinstall -t "${WHL_INSTALL_DIR_PATH}" "${whl}" || true
done

if [ -f "${WHL_GE_PY}" ]; then
    py_tag=$(python3 -c 'import sys; print("cp%d%d" % sys.version_info[:2])' 2>/dev/null)
    bridge_whl=$(ls "${WHL_BRIDGE_DIR}/ge_py_pass_bridge-*-${py_tag}-${py_tag}-*.whl" 2>/dev/null | head -1)
    if [ -n "${bridge_whl}" ]; then
        echo "[ge-compiler] installing ${WHL_GE_PY} with bridge ${bridge_whl}"
        run_pip install --disable-pip-version-check --upgrade --no-index --no-deps --force-reinstall -t "${WHL_INSTALL_DIR_PATH}" "${WHL_GE_PY}" "${bridge_whl}" || \
        { echo "[ge-compiler] bridge install failed, fallback to ge_py only"; run_pip install --disable-pip-version-check --upgrade --no-deps --force-reinstall -t "${WHL_INSTALL_DIR_PATH}" "${WHL_GE_PY}"; } || true
    else
        echo "[ge-compiler] installing ${WHL_GE_PY}"
        run_pip install --disable-pip-version-check --upgrade --no-deps --force-reinstall -t "${WHL_INSTALL_DIR_PATH}" "${WHL_GE_PY}" || true
    fi
fi

stub_libs="
libacl_op_compiler.so
libfmk_onnx_parser.so
libfmk_parser.so
libge_compiler.so
libge_runner.so
libge_runner_v2.so"

sourcedir="${INSTALL_PATH}"
pkg_arch_name="${PKG_ARCH_NAME}"

create_stub_softlink() {
    local install_path="${sourcedir}"
    if [ ! -d "$install_path" ]; then
        return
    fi
    local devlibdir="${install_path}/${pkg_arch_name}-linux/devlib"
    ([ -d "${devlibdir}" ] && cd "${devlibdir}" && {
        chmod u+w . && \
        for lib in ${stub_libs}; do
            lib="linux/${pkg_arch_name}/$lib"
            [ -f "$lib" ] && ln -sf "$lib" "$(basename $lib)"
        done
        chmod u-w .
    }) || true
}

create_stub_softlink
