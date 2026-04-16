#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# -------------------------------------------------------------------
# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

"""
Session 功能测试 - 使用 pytest 框架
测试 session.py 中的 Session 类的各种功能
"""

import pytest
# 添加 ge 到 Python 路径
try:
    from ge.session import Session
    from ge.ge_global import GeApi
    from ge.graph import Graph
    from ge.graph import Tensor
    from ge.graph.types import DataType, Format
    from ge.es.graph_builder import GraphBuilder
    from ge.allocator import Allocator, MemBlock
except ImportError as e:
    pytest.skip(f"无法导入 ge 模块: {e}", allow_module_level=True)


class TestSession:
    """Session 功能测试类"""
    @staticmethod
    def test_copy_not_supported():
        session = Session()
        """测试复制不支持"""
        with pytest.raises(RuntimeError, match="Session does not support copy"):
            session.__copy__()

    @staticmethod
    def test_deepcopy_not_supported():
        session = Session()
        """测试深复制不支持"""
        with pytest.raises(RuntimeError, match="Session does not support deepcopy"):
            session.__deepcopy__({})

    @staticmethod
    def test_session_with_option():
        option = {"ge.exec.enableDump":"0"}
        session = Session(option)
        assert session is not None

    @staticmethod
    def test_session_with_option_invalid():
        option = {"ge.exec.deviceId"}
        with pytest.raises(TypeError, match="option must be a dictionary"):
            Session(option)

    @staticmethod
    def test_ge_initialize_invalid():
        """测试初始化无效入参"""
        options = {1, "ge.graphRunMode"}
        ge_api = GeApi()
        with pytest.raises(TypeError, match="Ge init config must be a dictionary"):
            ge_api.ge_initialize(options)

    @staticmethod
    def test_add_graph_invalid_graph_id():
        session = Session()
        graph = Graph("test_graph")
        graph_id = "test"
        with pytest.raises(TypeError, match="Graph_id must be an integer"):
            session.add_graph(graph_id, graph)

    @staticmethod
    def test_add_graph_invalid_option():
        session = Session()
        graph = Graph("test_graph")
        graph_id = 0
        option = {"ge.exec.deviceId"}
        with pytest.raises(TypeError, match="options must be a dictionary"):
            session.add_graph(graph_id, graph, option)

    @staticmethod
    def test_add_graph_invalid_graph():
        session = Session()
        graph_id = 0
        with pytest.raises(TypeError, match="Add_graph must be a Graph"):
            session.add_graph(graph_id, session)

    @staticmethod
    def test_add_graph_without_ge_init():
        ge_api = GeApi()
        ge_api.ge_finalize()
        graph = Graph("test_graph")
        graph_id = 2
        session = Session()
        with pytest.raises(RuntimeError, match="Failed to add graph, graph_id is 2"):
            session.add_graph(graph_id, graph)

    @staticmethod
    def test_add_graph_with_option_invalid():
        ge_api = GeApi()
        ge_api.ge_finalize()
        graph = Graph("test_graph")
        graph_id = 2
        option = {"ge.exec.deviceId":"2", "ge.graphRunMode":"0"}
        session = Session()
        with pytest.raises(RuntimeError, match="Failed to add graph, graph_id is 2"):
            session.add_graph(graph_id, graph, option)

    @staticmethod
    def test_run_graph_invaild_graph_id():
        session = Session()
        graph_id = "test"
        tensor = Tensor([1, 2, 3, 4, 5], None, DataType.DT_INT8, Format.FORMAT_ND, [1,2,3])
        inputs = [tensor]
        with pytest.raises(TypeError, match="Graph_id must be an integer"):
            session.run_graph(graph_id, inputs)

    @staticmethod
    def test_run_graph_invaild_inputs():
        session = Session()
        graph_id = 0
        tensor = 0
        inputs = [tensor]
        with pytest.raises(TypeError, match="All elements in inputs must be the type of Tensor"):
            session.run_graph(graph_id, inputs)

    @staticmethod
    def test_run_graph_without_ge_init():
        tensor = Tensor([1.0, 2.0, 3.0, 4.0], shape=[2, 2])
        inputs = [tensor]
        graph_id = 2
        session = Session()
        with pytest.raises(RuntimeError, match="Failed to run graph, graph_id is 2"):
            session.run_graph(graph_id,inputs)

    @staticmethod
    def test_remove_graph_invalid_graph_id():
        session = Session()
        with pytest.raises(TypeError, match="Graph_id must be an integer"):
            session.remove_graph("test")

    @staticmethod
    def test_run_graph_with_stream_async_invalid_graph_id():
        session = Session()
        tensor = Tensor([1], None, DataType.DT_INT32, Format.FORMAT_ND, [])
        with pytest.raises(TypeError, match="Graph_id must be an integer"):
            session.run_graph_with_stream_async("test", 1, [tensor])

    @staticmethod
    def test_run_graph_with_stream_async_invalid_inputs():
        session = Session()
        tensor = Tensor([1], None, DataType.DT_INT32, Format.FORMAT_ND, [])
        with pytest.raises(TypeError, match="All elements in inputs must be the type of Tensor"):
            session.run_graph_with_stream_async(0, 1, [0])

    @staticmethod
    def test_run_graph_with_stream_async_stream_none():
        session = Session()
        tensor = Tensor([1], None, DataType.DT_INT32, Format.FORMAT_ND, [])
        with pytest.raises(TypeError, match="Stream must be an integer"):
            session.run_graph_with_stream_async(0, None, [tensor])

    @staticmethod
    def test_run_graph_with_stream_async_inputs_not_list():
        session = Session()
        tensor = Tensor([1], None, DataType.DT_INT32, Format.FORMAT_ND, [])
        with pytest.raises(TypeError, match="inputs must be a list of Tensor"):
            session.run_graph_with_stream_async(0, 1, tensor)

    @staticmethod
    def test_register_external_allocator_invalid_stream():
        """测试 register_external_allocator stream 非整数类型"""
        class _StubAllocator(Allocator):
            def malloc(self, size):
                return MemBlock(0, size)

            def free(self, block):
                pass

        session = Session()
        with pytest.raises(TypeError, match="stream must be an integer"):
            session.register_external_allocator("not_int", _StubAllocator())

    @staticmethod
    def test_register_external_allocator_invalid_allocator():
        """测试 register_external_allocator allocator 类型不合法"""
        session = Session()
        with pytest.raises(TypeError, match="allocator must be an Allocator instance"):
            session.register_external_allocator(1, "not_an_allocator")

    @staticmethod
    def test_unregister_external_allocator_invalid_stream():
        """测试 unregister_external_allocator stream 非整数类型"""
        session = Session()
        with pytest.raises(TypeError, match="stream must be an integer"):
            session.unregister_external_allocator("not_int")

    @staticmethod
    def test_ge_init_invaild():
        ge_api = GeApi()
        init_config = {}
        with pytest.raises(TypeError, match="Ge init config must not be empty"):
            ge_api.ge_initialize(init_config)

    @staticmethod
    def test_ge_fin_invaild():
        ge_api = GeApi()
        ret = ge_api.ge_finalize()
        assert ret == 0

    @staticmethod
    def test_ge_init():
        ge_api = GeApi()
        config = {"ge.exec.deviceId":"2", "ge.graphRunMode":"0"}
        ret = ge_api.ge_initialize(config)
        assert ret == 0
