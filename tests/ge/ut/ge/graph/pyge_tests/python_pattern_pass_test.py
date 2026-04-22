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

"""Pytest coverage for PatternFusionPass Python helpers and bridge protocol."""

import importlib
import textwrap
from pathlib import Path

import pytest

try:
    bridge = importlib.import_module("ge.passes._bridge")
    bootstrap = importlib.import_module("ge.passes.bootstrap")
    passes = importlib.import_module("ge.passes")
    graph_module = importlib.import_module("ge.graph")
    graph_builder_module = importlib.import_module("ge.es.graph_builder")
except ImportError as exc:
    pytest.skip(f"无法导入 PatternFusionPass 相关模块: {exc}", allow_module_level=True)

Graph = graph_module.Graph
GraphBuilder = graph_builder_module.GraphBuilder
Pattern = passes.Pattern
PatternMatcherConfigBuilder = passes.PatternMatcherConfigBuilder


def _write_pattern_pass_module(dir_path: Path, module_name: str, pass_name: str) -> Path:
    file_path = dir_path / f"{module_name}.py"
    file_path.write_text(textwrap.dedent(f"""
        from ge.graph import Graph
        from ge.passes import PassStage, PatternFusionPass, register_fusion_pass

        @register_fusion_pass(name="{pass_name}", stage=PassStage.AFTER_INFER_SHAPE)
        class {pass_name}(PatternFusionPass):
            def patterns(self):
                return [Graph("pattern_graph")]

            def meet_requirements(self, match_result):
                return match_result.get_pattern_graph_name() == "borrowed_pattern"

            def replacement(self, match_result):
                return Graph("replacement_graph")
    """).strip() + "\n", encoding="utf-8")
    return file_path


def _write_pattern_matcher_config_pass_module(dir_path: Path, module_name: str, pass_name: str) -> Path:
    file_path = dir_path / f"{module_name}.py"
    file_path.write_text(textwrap.dedent(f"""
        from ge.graph import Graph
        from ge.passes import (
            PassStage,
            PatternFusionPass,
            PatternMatcherConfigBuilder,
            register_fusion_pass,
        )

        @register_fusion_pass(name="{pass_name}", stage=PassStage.AFTER_INFER_SHAPE)
        class {pass_name}(PatternFusionPass):
            def __init__(self):
                super().__init__(
                    PatternMatcherConfigBuilder()
                    .enable_const_value_match()
                    .enable_ir_attr_match()
                    .build()
                )

            def patterns(self):
                return [Graph("pattern_graph")]

            def replacement(self, match_result):
                return Graph("replacement_graph")
    """).strip() + "\n", encoding="utf-8")
    return file_path


@pytest.fixture(autouse=True)
def clear_python_pattern_runtime(monkeypatch):
    monkeypatch.delenv(bootstrap.ENV_PY_PASS_PATH, raising=False)
    passes.clear_registered_passes()
    bridge.clear_pass_holders()
    bridge.clear_loaded_pass_modules()
    yield
    bridge.clear_pass_holders()
    bridge.clear_loaded_pass_modules()
    passes.clear_registered_passes()


def test_pattern_matcher_config_builder_flags():
    config = (
        PatternMatcherConfigBuilder()
        .enable_const_value_match()
        .enable_ir_attr_match()
        .build()
    )

    assert config.is_enable_const_value_match() is True
    assert config.is_enable_ir_attr_match() is True


def test_bridge_get_pattern_matcher_config_returns_native_handle(tmp_path, monkeypatch):
    module_path = _write_pattern_matcher_config_pass_module(
        tmp_path, "bridge_pattern_config_pass", "BridgePatternConfigPass"
    )
    monkeypatch.setenv(bootstrap.ENV_PY_PASS_PATH, str(module_path))

    descriptors = bridge.load_and_get_pass_descriptors()
    assert len(descriptors) == 1

    instance_id = "BridgePatternConfigPass#1"
    descriptor_key = descriptors[0]["descriptor_key"]
    assert bridge.create_pass_holder(instance_id, descriptor_key)

    matcher_config_handle = bridge.get_pattern_matcher_config(instance_id)
    assert isinstance(matcher_config_handle, int)
    assert matcher_config_handle != 0

    assert bridge.destroy_pass_holder(instance_id) is True


def test_native_pattern_capture_tensor_and_release():
    builder = GraphBuilder("pattern_graph")
    input_tensor = builder.create_input(0)
    builder.set_graph_output(input_tensor, 0)
    graph = builder.build_and_reset()
    data_node = next(node for node in graph.get_all_nodes() if node.type == "Data")

    pattern = Pattern(graph)
    pattern.capture_tensor(data_node, 0)

    captured_tensors = pattern.get_captured_tensors()
    assert len(captured_tensors) == 1
    assert captured_tensors[0].node.type == "Data"
    assert captured_tensors[0].index == 0

    released_pattern_handle = pattern.release()
    assert isinstance(released_pattern_handle, int)
    assert released_pattern_handle != 0


def test_bridge_pattern_protocol_functions(tmp_path, monkeypatch):
    module_path = _write_pattern_pass_module(tmp_path, "bridge_pattern_pass", "BridgePatternPass")
    monkeypatch.setenv(bootstrap.ENV_PY_PASS_PATH, str(module_path))

    descriptors = bridge.load_and_get_pass_descriptors()

    assert len(descriptors) == 1
    descriptor_key = descriptors[0]["descriptor_key"]
    instance_id = "BridgePatternPass#1"
    assert bridge.create_pass_holder(instance_id, descriptor_key)

    pattern_handles = bridge.get_pass_patterns(instance_id)
    assert len(pattern_handles) == 1
    assert isinstance(pattern_handles[0], int)
    assert pattern_handles[0] != 0

    created_match_results = []

    class FakeMatchResult:
        def __init__(self):
            self.invalidated = False

        def get_pattern_graph_name(self):
            return "borrowed_pattern"

        def _invalidate(self):
            self.invalidated = True

    def fake_borrow_match_result(_handle):
        fake = FakeMatchResult()
        created_match_results.append(fake)
        return fake

    def fake_release_graph(graph):
        assert graph.name == "replacement_graph"
        return 20260417

    monkeypatch.setattr(bridge, "borrow_match_result", fake_borrow_match_result)
    monkeypatch.setattr(bridge, "release_graph", fake_release_graph)

    assert bridge.call_meet_requirements(instance_id, 1) is True
    assert created_match_results[-1].invalidated is True

    assert bridge.call_replacement(instance_id, 1) == 20260417
    assert created_match_results[-1].invalidated is True

    assert bridge.destroy_pass_holder(instance_id) is True
