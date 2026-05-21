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

"""GeUtils Python API tests."""

import ctypes
import pytest

try:
    import ge._capi.pyge_utils_wrapper as wrapper
    import ge.utils.ge_utils as ge_utils_module
    from ge.es.graph_builder import GraphBuilder
    from ge.utils import GeUtils
    from ge.graph import Graph, Node
except (ImportError, OSError) as exc:
    pytest.skip(f"无法导入 ge 模块: {exc}", allow_module_level=True)


class TestGeUtils:
    @staticmethod
    def test_ge_utils_wrapper_lib_helpers_available():
        assert wrapper.is_ge_utils_lib_loaded() is True
        assert wrapper.get_ge_utils_lib() is not None

    @staticmethod
    def test_infer_shape_returns_none_on_success():
        graph = Graph("ge_utils_empty_graph")

        assert GeUtils.infer_shape(graph, []) is None

    @staticmethod
    def test_infer_shape_raises_on_native_failure():
        graph = Graph("ge_utils_empty_graph")

        with pytest.raises(RuntimeError, match="Failed to infer shape"):
            GeUtils.infer_shape(graph, [[1]])

    @staticmethod
    @pytest.mark.parametrize("input_shapes", [
        "bad",
        [1, 2],
        [[1, "bad"]],
    ])
    def test_infer_shape_rejects_invalid_input_shapes(input_shapes):
        with pytest.raises(TypeError):
            GeUtils.infer_shape(Graph("ge_utils_invalid_input_shapes"), input_shapes)

    @staticmethod
    def test_check_node_support_on_aicore_returns_support_result(monkeypatch):
        builder = GraphBuilder("ge_utils_check_node_support")
        const_tensor = builder.create_const_float(1.0)
        builder.set_graph_output(const_tensor, 0)
        graph = builder.build_and_reset()
        node = next(node for node in graph.get_all_nodes() if node.type == "Const")
        reason_buffer = ctypes.create_string_buffer("mock unsupported reason".encode("utf-8"))
        freed_reasons = []

        def mock_check_node_support_on_aicore(node_handle, is_supported, unsupported_reason):
            assert node_handle == node._handle
            ctypes.cast(is_supported, ctypes.POINTER(ctypes.c_bool))[0] = False
            reason_ptr = ctypes.cast(reason_buffer, ctypes.POINTER(ctypes.c_char))
            ctypes.cast(unsupported_reason, ctypes.POINTER(ctypes.POINTER(ctypes.c_char)))[0] = reason_ptr
            return 0

        def mock_free_string(reason_ptr):
            freed_reasons.append(ctypes.string_at(reason_ptr).decode("utf-8"))

        monkeypatch.setattr(
            ge_utils_module.ge_utils_lib,
            "GeApiWrapper_GeUtils_CheckNodeSupportOnAicore",
            mock_check_node_support_on_aicore,
        )
        monkeypatch.setattr(ge_utils_module.ge_utils_lib, "GeApiWrapper_GeUtils_FreeString", mock_free_string)

        is_supported, reason = GeUtils.check_node_support_on_aicore(node)

        assert is_supported is False
        assert reason == "mock unsupported reason"
        assert freed_reasons == ["mock unsupported reason"]
