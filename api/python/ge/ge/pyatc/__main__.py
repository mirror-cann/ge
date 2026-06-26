#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

"""pyatc CLI entry point."""

import ctypes
import os
import sys

from ge._capi.pypyatc_wrapper import pyatc_lib


def _build_native_argv(argv):
    encoded = [arg.encode("utf-8") for arg in argv]
    c_argv = (ctypes.c_char_p * len(encoded))(*encoded)
    return encoded, c_argv


def _build_native_cmdline_argv(cmdline_argv0=None, args=None):
    if cmdline_argv0 is not None:
        return [cmdline_argv0] + list(args or [])

    native_argv = list(sys.argv)
    if native_argv and os.path.basename(native_argv[0]) == "__main__.py":
        native_argv[0] = sys.executable + " -m ge.pyatc"
    return native_argv


def main(cmdline_argv0=None, args=None) -> None:
    native_argv = _build_native_cmdline_argv(cmdline_argv0, args)
    encoded, c_argv = _build_native_argv(native_argv)
    ret = pyatc_lib.GeApiWrapper_Atc_Main(len(c_argv), c_argv)
    sys.exit(ret)


if __name__ == "__main__":
    main()
