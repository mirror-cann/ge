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

"""
pyatc CLI launcher 功能测试 - pytest 框架
"""

import sys

import pytest

try:
    import ge.pyatc.__main__ as pyatc_main
    from ge._capi.pypyatc_wrapper import get_pyatc_lib, is_pyatc_lib_loaded
except ImportError as e:
    pytest.skip(f"无法导入 ge.pyatc 模块: {e}", allow_module_level=True)


class _FakeFunc:
    def __init__(self):
        self.argtypes = None
        self.restype = None
        self.last_args = None
        self.return_value = 0

    def __call__(self, argc, argv):
        self.last_args = (argc, argv)
        return self.return_value


class _FakeLib:
    def __init__(self):
        self.GeApiWrapper_Atc_Main = _FakeFunc()


@pytest.fixture
def fake_pyatc_lib():
    return _FakeLib()


class TestPyAtc:
    @staticmethod
    def test_lib_is_loaded():
        assert is_pyatc_lib_loaded() is True

    @staticmethod
    def test_get_lib_not_none():
        lib = get_pyatc_lib()
        assert lib is not None

    @staticmethod
    def test_forwards_sys_argv_to_native_main(monkeypatch, fake_pyatc_lib):
        monkeypatch.setattr(pyatc_main, "pyatc_lib", fake_pyatc_lib)
        monkeypatch.setattr(
            sys,
            "argv",
            ["/path/to/bin/pyatc", "--model=foo.onnx", "--soc_version=Ascend910"],
        )

        with pytest.raises(SystemExit) as exc:
            pyatc_main.main()

        assert exc.value.code == 0
        argc, argv = fake_pyatc_lib.GeApiWrapper_Atc_Main.last_args
        assert argc == 3
        assert argv[0] == b"/path/to/bin/pyatc"
        assert argv[1] == b"--model=foo.onnx"
        assert argv[2] == b"--soc_version=Ascend910"

    @staticmethod
    def test_forwards_shell_wrapper_argv0_to_native_main(monkeypatch, fake_pyatc_lib):
        monkeypatch.setattr(pyatc_main, "pyatc_lib", fake_pyatc_lib)
        with pytest.raises(SystemExit) as exc:
            pyatc_main.main("/real/cann/bin/pyatc", ["--help"])

        assert exc.value.code == 0
        argc, argv = fake_pyatc_lib.GeApiWrapper_Atc_Main.last_args
        assert argc == 2
        assert argv[0] == b"/real/cann/bin/pyatc"
        assert argv[1] == b"--help"

    @staticmethod
    def test_python_module_entry_uses_executable_module_argv0(monkeypatch, fake_pyatc_lib):
        monkeypatch.setattr(pyatc_main, "pyatc_lib", fake_pyatc_lib)
        monkeypatch.setattr(sys, "executable", "/opt/myenv/bin/python")
        monkeypatch.setattr(sys, "argv", ["/pkg/ge/pyatc/__main__.py", "--help"])

        with pytest.raises(SystemExit) as exc:
            pyatc_main.main()

        assert exc.value.code == 0
        argc, argv = fake_pyatc_lib.GeApiWrapper_Atc_Main.last_args
        assert argc == 2
        assert argv[0] == b"/opt/myenv/bin/python -m ge.pyatc"
        assert argv[1] == b"--help"
