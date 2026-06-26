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

"""GE Python error reporting tests."""

import ctypes
import importlib
import re
import sys
import types
from pathlib import Path

import pytest

GE_PYTHON_DIR = Path(__file__).resolve().parents[6] / "api/python/ge"
if str(GE_PYTHON_DIR) not in sys.path:
    sys.path.insert(0, str(GE_PYTHON_DIR))


class FakeCFunc:
    def __init__(self, impl=None):
        self.impl = impl or (lambda *args: 0)
        self.calls = []
        self.argtypes = None
        self.restype = None

    def __call__(self, *args):
        self.calls.append(args)
        return self.impl(*args)


class FakeRuntimeWrapper:
    def __init__(self):
        self.GeApiWrapper_GEInitialize = FakeCFunc()
        self.GeApiWrapper_GEFinalize = FakeCFunc()
        self.GeApiWrapper_GEGetErrorMsg = FakeCFunc(lambda: b"E10062: invalid input")

        self.GeApiWrapper_Session_CreateSession = FakeCFunc(lambda: ctypes.c_void_p(0x1234))
        self.GeApiWrapper_Session_CreateSessionWithOptions = FakeCFunc(lambda *_args: ctypes.c_void_p(0x1234))
        self.GeApiWrapper_Session_AddGraph = FakeCFunc()
        self.GeApiWrapper_Session_AddGraphWithOptions = FakeCFunc()
        self.GeApiWrapper_Session_RemoveGraph = FakeCFunc()
        self.GeApiWrapper_Session_RunGraph = FakeCFunc()
        self.GeApiWrapper_Session_RunGraphWithStreamAsync = FakeCFunc()
        self.GeApiWrapper_Session_FreeTensorArray = FakeCFunc()
        self.GeApiWrapper_Session_DestroySession = FakeCFunc()
        self.GeApiWrapper_Session_RegisterDefaultAllocator = FakeCFunc()
        self.GeApiWrapper_Session_RegisterExternalAllocator = FakeCFunc()
        self.GeApiWrapper_Session_UnregisterExternalAllocator = FakeCFunc()
        self.GeApiWrapper_HasExternalAllocator = FakeCFunc(lambda _stream: False)
        self.GeApiWrapper_HasDefaultAllocator = FakeCFunc(lambda _stream: False)
        self.GeApiWrapper_IsGEInitialized = FakeCFunc(lambda: True)

        self.GeApiWrapper_OfflineCompile_BuildInitialize = FakeCFunc()
        self.GeApiWrapper_OfflineCompile_BuildFinalize = FakeCFunc()
        self.GeApiWrapper_OfflineCompile_BuildModel = FakeCFunc()
        self.GeApiWrapper_OfflineCompile_SaveModel = FakeCFunc()
        self.GeApiWrapper_OfflineCompile_BundleBuildModel = FakeCFunc()
        self.GeApiWrapper_OfflineCompile_BundleSaveModel = FakeCFunc()
        self.GeApiWrapper_ModelBuffer_Destroy = FakeCFunc()
        self.GeApiWrapper_ModelBuffer_GetLength = FakeCFunc(lambda _model: 0)


def _drop_loaded_ge_modules(monkeypatch):
    module_names = (
        "ge.error",
        "ge.ge_global",
        "ge.ge_global.geapi",
        "ge.session",
        "ge.session.session",
        "ge.offline_compile",
        "ge.offline_compile.offline_compile",
        "ge._capi.pygeapi_wrapper",
        "ge._capi.pysession_wrapper",
        "ge._capi.pyoffline_compile_wrapper",
        "ge._capi._allocator_callback_adapter",
    )
    for module_name in module_names:
        monkeypatch.delitem(sys.modules, module_name, raising=False)

    parent_attrs = {
        "ge": ("error", "ge_global", "session", "offline_compile"),
        "ge._capi": (
            "pygeapi_wrapper",
            "pysession_wrapper",
            "pyoffline_compile_wrapper",
            "_allocator_callback_adapter",
        ),
    }
    for parent_name, attr_names in parent_attrs.items():
        parent = sys.modules.get(parent_name)
        if parent is None:
            continue
        for attr_name in attr_names:
            monkeypatch.delattr(parent, attr_name, raising=False)


def _install_fake_graph_modules(monkeypatch):
    graph_pkg = types.ModuleType("ge.graph")
    graph_mod = types.ModuleType("ge.graph.graph")
    tensor_mod = types.ModuleType("ge.graph.tensor")
    allocator_pkg = types.ModuleType("ge.allocator")

    class Graph:
        def __init__(self):
            self._handle = ctypes.c_void_p(0x5678)

    class Tensor:
        def __init__(self, handle=0x9ABC):
            self._handle = ctypes.c_void_p(handle)

        @staticmethod
        def _create_from(handle):
            return Tensor(handle)

    class Allocator:
        pass

    class MemBlock:
        pass

    graph_mod.Graph = Graph
    tensor_mod.Tensor = Tensor
    allocator_pkg.Allocator = Allocator
    allocator_pkg.MemBlock = MemBlock
    graph_pkg.graph = graph_mod
    graph_pkg.tensor = tensor_mod

    monkeypatch.setitem(sys.modules, "ge.graph", graph_pkg)
    monkeypatch.setitem(sys.modules, "ge.graph.graph", graph_mod)
    monkeypatch.setitem(sys.modules, "ge.graph.tensor", tensor_mod)
    monkeypatch.setitem(sys.modules, "ge.allocator", allocator_pkg)

    ge_pkg = importlib.import_module("ge")
    monkeypatch.setattr(ge_pkg, "graph", graph_pkg, raising=False)
    monkeypatch.setattr(ge_pkg, "allocator", allocator_pkg, raising=False)


@pytest.fixture
def fake_runtime_wrapper(monkeypatch):
    fake_lib = FakeRuntimeWrapper()
    _drop_loaded_ge_modules(monkeypatch)
    _install_fake_graph_modules(monkeypatch)
    loader_mod = importlib.import_module("ge._capi._lib_loader")
    monkeypatch.setattr(loader_mod, "load_lib_from_path", lambda *_args: fake_lib)
    return fake_lib


def test_raise_ge_error_contains_context_and_errmgr_message(fake_runtime_wrapper):
    del fake_runtime_wrapper
    error_mod = importlib.import_module("ge.error")

    with pytest.raises(error_mod.GeError) as err:
        error_mod.raise_ge_error("RunGraph", 145000, graph_id=7)

    assert "RunGraph failed" in str(err.value)
    assert "graph_id=7" in str(err.value)
    assert "E10062: invalid input" in str(err.value)


def test_ge_initialize_raises_ge_error(fake_runtime_wrapper):
    fake_runtime_wrapper.GeApiWrapper_GEInitialize.impl = lambda *_args: 145000
    error_mod = importlib.import_module("ge.error")
    geapi_mod = importlib.import_module("ge.ge_global.geapi")

    with pytest.raises(error_mod.GeError, match="GEInitialize.*E10062"):
        geapi_mod.GeApi.ge_initialize({"ge.execDeviceId": "0"})


def test_ge_finalize_raises_ge_error(fake_runtime_wrapper):
    fake_runtime_wrapper.GeApiWrapper_GEFinalize.impl = lambda: 145000
    error_mod = importlib.import_module("ge.error")
    geapi_mod = importlib.import_module("ge.ge_global.geapi")

    with pytest.raises(error_mod.GeError, match="GEFinalize.*E10062"):
        geapi_mod.GeApi.ge_finalize()


def test_add_graph_failure_uses_status_return_value(fake_runtime_wrapper):
    fake_runtime_wrapper.GeApiWrapper_Session_AddGraph.impl = lambda *_args: 145000
    error_mod = importlib.import_module("ge.error")
    session_mod = importlib.import_module("ge.session.session")

    session = session_mod.Session()
    with pytest.raises(error_mod.GeError, match="AddGraph.*graph_id=9.*E10062"):
        session.add_graph(9, session_mod.Graph())


def test_run_graph_failure_uses_status_return_value(fake_runtime_wrapper):
    fake_runtime_wrapper.GeApiWrapper_Session_RunGraph.impl = lambda *_args: 145000
    error_mod = importlib.import_module("ge.error")
    session_mod = importlib.import_module("ge.session.session")

    session = session_mod.Session()
    with pytest.raises(error_mod.GeError, match="RunGraph.*graph_id=3.*E10062"):
        session.run_graph(3, [])

    assert len(fake_runtime_wrapper.GeApiWrapper_Session_RunGraph.calls[0]) == 6


def test_run_graph_with_stream_async_failure_uses_status_return_value(
    fake_runtime_wrapper,
):
    fake_runtime_wrapper.GeApiWrapper_Session_RunGraphWithStreamAsync.impl = lambda *_args: 145000
    error_mod = importlib.import_module("ge.error")
    session_mod = importlib.import_module("ge.session.session")

    session = session_mod.Session()
    with pytest.raises(
        error_mod.GeError,
        match="RunGraphWithStreamAsync.*graph_id=3.*stream=0x1111.*E10062",
    ):
        session.run_graph_with_stream_async(3, 0x1111, [])

    assert len(fake_runtime_wrapper.GeApiWrapper_Session_RunGraphWithStreamAsync.calls[0]) == 7


def test_run_graph_success_accepts_empty_output_list(fake_runtime_wrapper):
    def run_graph_success(_session, _graph_id, _inputs, _input_num, _outputs, tensor_num):
        tensor_num._obj.value = 0
        return 0

    fake_runtime_wrapper.GeApiWrapper_Session_RunGraph.impl = run_graph_success
    session_mod = importlib.import_module("ge.session.session")

    session = session_mod.Session()
    assert session.run_graph(3, []) == []
    assert fake_runtime_wrapper.GeApiWrapper_Session_FreeTensorArray.calls == []


def test_run_graph_success_frees_output_tensor_array(fake_runtime_wrapper):
    output_array = (ctypes.c_void_p * 2)(0x111, 0x222)

    def run_graph_success(_session, _graph_id, _inputs, _input_num, outputs, tensor_num):
        output_ptr = ctypes.cast(outputs, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p)))
        output_ptr[0] = ctypes.cast(output_array, ctypes.POINTER(ctypes.c_void_p))
        tensor_num._obj.value = 2
        return 0

    fake_runtime_wrapper.GeApiWrapper_Session_RunGraph.impl = run_graph_success
    session_mod = importlib.import_module("ge.session.session")

    session = session_mod.Session()
    outputs = session.run_graph(3, [])

    assert [tensor._handle.value for tensor in outputs] == [0x111, 0x222]
    assert len(fake_runtime_wrapper.GeApiWrapper_Session_FreeTensorArray.calls) == 1
    freed_array = fake_runtime_wrapper.GeApiWrapper_Session_FreeTensorArray.calls[0][0]
    assert [freed_array[i] for i in range(2)] == [0x111, 0x222]


def test_offline_compile_failures_raise_ge_error(fake_runtime_wrapper):
    error_mod = importlib.import_module("ge.error")
    offline_mod = importlib.import_module("ge.offline_compile.offline_compile")
    model = offline_mod.ModelBuffer.__new__(offline_mod.ModelBuffer)
    model._handle = offline_mod.ModelBufferDataPtr()
    model._owns_handle = False

    cases = (
        (
            fake_runtime_wrapper.GeApiWrapper_OfflineCompile_BuildInitialize,
            lambda: offline_mod.build_initialize({}),
            "BuildInitialize",
        ),
        (
            fake_runtime_wrapper.GeApiWrapper_OfflineCompile_BuildModel,
            lambda: offline_mod.build_model(offline_mod.Graph()),
            "BuildModel",
        ),
        (
            fake_runtime_wrapper.GeApiWrapper_OfflineCompile_SaveModel,
            lambda: offline_mod.save_model("out", model),
            "SaveModel",
        ),
        (
            fake_runtime_wrapper.GeApiWrapper_OfflineCompile_BundleBuildModel,
            lambda: offline_mod.bundle_build_model(
                [
                    offline_mod.GraphWithOptions(offline_mod.Graph()),
                    offline_mod.GraphWithOptions(offline_mod.Graph()),
                ]
            ),
            "BundleBuildModel",
        ),
        (
            fake_runtime_wrapper.GeApiWrapper_OfflineCompile_BundleSaveModel,
            lambda: offline_mod.bundle_save_model("bundle", model),
            "BundleSaveModel",
        ),
    )

    for c_func, call, api_name in cases:
        c_func.impl = lambda *_args: 145000
        with pytest.raises(error_mod.GeError, match=rf"{re.escape(api_name)}.*E10062"):
            call()
        c_func.impl = lambda *_args: 0
