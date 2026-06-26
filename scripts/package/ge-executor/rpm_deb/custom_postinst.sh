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

stub_libs="
libacl_cblas.so
libacl_mdl.so
libacl_op_executor.so
libge_common.so
libgert.so
libgraph.so
libhybrid_executor.so
liblowering.so
libregister.so"

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
    })
}

create_stub_softlink
