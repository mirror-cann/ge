#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# -------------------------------------------------------------------
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
offline_compile 模块功能测试 - 使用 pytest 框架
测试 offline_compile.py 中的离线图编译相关接口
"""

import copy
import ctypes
import pytest

try:
    from ge.offline_compile import (
        GraphWithOptions,
        ModelBuffer,
        build_finalize,
        build_initialize,
        build_model,
        bundle_build_model,
        bundle_save_model,
        save_model,
    )
    from ge._capi.pyoffline_compile_wrapper import get_offline_compile_lib, is_offline_compile_lib_loaded
    from ge.graph import Graph
    from ge.graph.types import DataType
except ImportError as e:
    pytest.skip(f"无法导入 ge 模块: {e}", allow_module_level=True)


class TestOfflineCompileLib:
    """测试 offline compile C 库加载工具函数"""

    @staticmethod
    def test_lib_is_loaded():
        assert is_offline_compile_lib_loaded() is True

    @staticmethod
    def test_get_lib_not_none():
        lib = get_offline_compile_lib()
        assert lib is not None


class TestGraphWithOptions:
    """测试 GraphWithOptions 数据类"""

    @staticmethod
    def test_valid_construction():
        g = Graph("test_graph")
        gwo = GraphWithOptions(g, {"key": "val"})
        assert gwo.graph is g
        assert gwo.build_options == {"key": "val"}

    @staticmethod
    def test_default_options():
        gwo = GraphWithOptions(Graph("test_graph"))
        assert gwo.build_options == {}

    @staticmethod
    def test_none_options_normalized_to_empty():
        gwo = GraphWithOptions(Graph("test_graph"), None)
        assert gwo.build_options == {}

    @staticmethod
    def test_invalid_graph_type():
        with pytest.raises(TypeError, match="graph must be a Graph"):
            GraphWithOptions("not_a_graph", {})

    @staticmethod
    def test_invalid_options_type():
        with pytest.raises(TypeError, match="build_options must be a dictionary"):
            GraphWithOptions(Graph("test_graph"), "invalid")

    @staticmethod
    def test_non_string_option_key():
        with pytest.raises(TypeError, match="build_options keys and values must be strings"):
            GraphWithOptions(Graph("test_graph"), {1: "val"})

    @staticmethod
    def test_non_string_option_value():
        with pytest.raises(TypeError, match="build_options keys and values must be strings"):
            GraphWithOptions(Graph("test_graph"), {"key": 1})

    @staticmethod
    def test_multiple_options():
        opts = {"input_format": "ND", "precision_mode_v2": "fp16"}
        gwo = GraphWithOptions(Graph("test_graph"), opts)
        assert gwo.build_options == opts


class TestModelBuffer:
    """测试 ModelBuffer 类"""

    @staticmethod
    def test_direct_init_raises():
        """ModelBuffer 不能直接实例化"""
        with pytest.raises(RuntimeError, match="ModelBuffer objects should not be created directly"):
            ModelBuffer()

    @staticmethod
    def test_create_from_none_raises():
        """None handle 不能创建 ModelBuffer"""
        with pytest.raises(ValueError, match="Failed to create ModelBuffer"):
            ModelBuffer._create_from(None)

    @staticmethod
    def test_copy_raises():
        """ModelBuffer 不支持 copy"""
        model = ModelBuffer._create_from(ctypes.c_void_p(0x1))
        model._owns_handle = False
        with pytest.raises(RuntimeError, match="ModelBuffer does not support copy"):
            copy.copy(model)

    @staticmethod
    def test_deepcopy_raises():
        """ModelBuffer 不支持 deepcopy"""
        model = ModelBuffer._create_from(ctypes.c_void_p(0x1))
        model._owns_handle = False
        with pytest.raises(RuntimeError, match="ModelBuffer does not support deepcopy"):
            copy.deepcopy(model)

    @staticmethod
    def test_create_from_valid_handle():
        """从有效 handle 创建 ModelBuffer"""
        model = ModelBuffer._create_from(ctypes.c_void_p(0x1))
        assert model._owns_handle is True
        model._owns_handle = False


class TestBuildInitialize:
    """测试 build_initialize 参数校验"""

    @staticmethod
    def test_invalid_options_type_string():
        with pytest.raises(TypeError, match="global_options must be a dictionary"):
            build_initialize("bad")

    @staticmethod
    def test_invalid_options_type_list():
        with pytest.raises(TypeError, match="global_options must be a dictionary"):
            build_initialize([1, 2])

    @staticmethod
    def test_invalid_option_key_type():
        with pytest.raises(TypeError, match="global_options keys and values must be strings"):
            build_initialize({1: "v"})

    @staticmethod
    def test_invalid_option_value_type():
        with pytest.raises(TypeError, match="global_options keys and values must be strings"):
            build_initialize({"k": 2})


class TestBuildModel:
    """测试 build_model 参数校验"""

    @staticmethod
    def test_invalid_graph_type_string():
        with pytest.raises(TypeError, match="graph must be a Graph"):
            build_model("not_a_graph")

    @staticmethod
    def test_invalid_graph_type_int():
        with pytest.raises(TypeError, match="graph must be a Graph"):
            build_model(123)

    @staticmethod
    def test_invalid_options_type():
        with pytest.raises(TypeError, match="build_options must be a dictionary"):
            build_model(Graph("test_graph"), "bad")

    @staticmethod
    def test_invalid_option_key_type():
        with pytest.raises(TypeError, match="build_options keys and values must be strings"):
            build_model(Graph("test_graph"), {1: "v"})

    @staticmethod
    def test_invalid_option_value_type():
        with pytest.raises(TypeError, match="build_options keys and values must be strings"):
            build_model(Graph("test_graph"), {"k": 1})


class TestSaveModel:
    """测试 save_model 参数校验"""

    @staticmethod
    def test_invalid_output_file_type_int():
        model = ModelBuffer._create_from(ctypes.c_void_p(0x1))
        model._owns_handle = False
        with pytest.raises(TypeError, match="output_file must be a string"):
            save_model(123, model)

    @staticmethod
    def test_invalid_output_file_type_list():
        model = ModelBuffer._create_from(ctypes.c_void_p(0x1))
        model._owns_handle = False
        with pytest.raises(TypeError, match="output_file must be a string"):
            save_model(["out.om"], model)

    @staticmethod
    def test_invalid_model_type_string():
        with pytest.raises(TypeError, match="model must be a ModelBuffer"):
            save_model("test.om", "not_model")

    @staticmethod
    def test_invalid_model_type_none():
        with pytest.raises(TypeError, match="model must be a ModelBuffer"):
            save_model("test.om", None)


class TestBundleBuildModel:
    """测试 bundle_build_model 参数校验"""

    @staticmethod
    def test_invalid_container_type_string():
        with pytest.raises(TypeError, match="graph_with_options must be a list"):
            bundle_build_model("bad")

    @staticmethod
    def test_invalid_container_type_dict():
        with pytest.raises(TypeError, match="graph_with_options must be a list"):
            bundle_build_model({})

    @staticmethod
    def test_empty_list():
        with pytest.raises(ValueError, match="graph_with_options size must be larger than 1"):
            bundle_build_model([])

    @staticmethod
    def test_invalid_item_type_graph():
        with pytest.raises(TypeError, match="Each item in graph_with_options must be a GraphWithOptions"):
            bundle_build_model([Graph("test_graph"), {}])

    @staticmethod
    def test_invalid_item_type_string():
        with pytest.raises(TypeError, match="Each item in graph_with_options must be a GraphWithOptions"):
            bundle_build_model(["bad", {}])

    @staticmethod
    def test_invalid_options_size():
        with pytest.raises(ValueError, match="graph_with_options size must be larger than 1"):
            bundle_build_model([
                GraphWithOptions(Graph("g1"), {"input_format": "ND", }),
            ])


class TestBundleSaveModel:
    """测试 bundle_save_model 参数校验"""

    @staticmethod
    def test_invalid_output_file_type_int():
        model = ModelBuffer._create_from(ctypes.c_void_p(0x1))
        model._owns_handle = False
        with pytest.raises(TypeError, match="output_file must be a string"):
            bundle_save_model(999, model)

    @staticmethod
    def test_invalid_output_file_type_none():
        model = ModelBuffer._create_from(ctypes.c_void_p(0x1))
        model._owns_handle = False
        with pytest.raises(TypeError, match="output_file must be a string"):
            bundle_save_model(None, model)

    @staticmethod
    def test_invalid_model_type_string():
        with pytest.raises(TypeError, match="model must be a ModelBuffer"):
            bundle_save_model("bundle_model.om", "not_model")

    @staticmethod
    def test_invalid_model_type_int():
        with pytest.raises(TypeError, match="model must be a ModelBuffer"):
            bundle_save_model("bundle_model.om", 42)
