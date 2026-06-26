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

"""Pytest coverage for DecomposePass Python helpers and bridge protocol."""

import importlib
import textwrap
from pathlib import Path

import pytest

try:
    bridge = importlib.import_module("ge.passes._bridge")
    bootstrap = importlib.import_module("ge.passes.bootstrap")
    passes = importlib.import_module("ge.passes")
    graph_module = importlib.import_module("ge.graph")
except ImportError as exc:
    pytest.skip(f"无法导入 DecomposePass 相关模块: {exc}", allow_module_level=True)

Graph = graph_module.Graph


def _write_decompose_pass_module(dir_path: Path, module_name: str, pass_name: str) -> Path:
    file_path = dir_path / f"{module_name}.py"
    file_path.write_text(
        textwrap.dedent(f"""
        from ge.graph import Graph
        from ge.passes import DecomposePass, PassStage, register_decompose_pass

        @register_decompose_pass(name="{pass_name}", stage=PassStage.AFTER_INFER_SHAPE, op_types=["Add"])
        class {pass_name}(DecomposePass):
            def meet_requirements(self, node):
                return node.type == "Add"

            def replacement(self, node):
                return Graph("replacement_graph")
    """).strip()
        + "\n",
        encoding="utf-8",
    )
    return file_path


def _write_invalid_replacement_decompose_pass_module(dir_path: Path, module_name: str, pass_name: str) -> Path:
    file_path = dir_path / f"{module_name}.py"
    file_path.write_text(
        textwrap.dedent(f"""
        from ge.passes import DecomposePass, PassStage, register_decompose_pass

        @register_decompose_pass(name="{pass_name}", stage=PassStage.AFTER_INFER_SHAPE, op_types=["Add"])
        class {pass_name}(DecomposePass):
            def replacement(self, node):
                return None
    """).strip()
        + "\n",
        encoding="utf-8",
    )
    return file_path


@pytest.fixture(autouse=True)
def clear_python_decompose_runtime(monkeypatch):
    monkeypatch.delenv(bootstrap.ENV_PY_PASS_PATH, raising=False)
    passes.clear_registered_passes()
    bridge.clear_pass_holders()
    bridge.clear_loaded_pass_modules()
    yield
    bridge.clear_pass_holders()
    bridge.clear_loaded_pass_modules()
    passes.clear_registered_passes()


def test_decompose_pass_rejects_run_override():
    with pytest.raises(TypeError, match="overrides run\\(\\)"):

        class InvalidDecomposePass(passes.DecomposePass):
            def run(self, graph, context):
                return True


def test_register_decompose_pass_requires_non_empty_string_op_types():
    with pytest.raises(TypeError, match="iterable of strings"):

        @passes.register_decompose_pass(
            name="BadDecomposePass",
            stage=passes.PassStage.AFTER_INFER_SHAPE,
            op_types="Add",
        )
        class BadDecomposePass(passes.DecomposePass):
            def replacement(self, node):
                return Graph("replacement_graph")

    with pytest.raises(ValueError, match="at least one op type"):

        @passes.register_decompose_pass(
            name="EmptyDecomposePass",
            stage=passes.PassStage.AFTER_INFER_SHAPE,
            op_types=[],
        )
        class EmptyDecomposePass(passes.DecomposePass):
            def replacement(self, node):
                return Graph("replacement_graph")


def test_register_decompose_pass_exposes_op_types(tmp_path, monkeypatch):
    module_path = _write_decompose_pass_module(tmp_path, "bridge_decompose_pass", "BridgeDecomposePass")
    monkeypatch.setenv(bootstrap.ENV_PY_PASS_PATH, str(module_path))

    descriptors = bridge.load_and_get_pass_descriptors()

    assert len(descriptors) == 1
    assert descriptors[0]["kind"] == "decompose"
    assert descriptors[0]["op_types"] == ["Add"]

    descriptor = passes.get_registered_passes()[0]
    assert descriptor.op_types == ["Add"]
    assert descriptor.cls.op_types == ["Add"]


def test_bridge_decompose_protocol_functions(tmp_path, monkeypatch):
    module_path = _write_decompose_pass_module(tmp_path, "bridge_decompose_pass", "BridgeDecomposePass")
    monkeypatch.setenv(bootstrap.ENV_PY_PASS_PATH, str(module_path))

    descriptors = bridge.load_and_get_pass_descriptors()

    assert len(descriptors) == 1
    descriptor_key = descriptors[0]["descriptor_key"]
    instance_id = "BridgeDecomposePass#1"
    assert bridge.create_pass_holder(instance_id, descriptor_key)

    class FakeNode:
        type = "Add"

    def fake_release_graph(graph):
        assert graph.name == "replacement_graph"
        return 20260423

    monkeypatch.setattr(bridge, "borrow_node", lambda _handle: FakeNode())
    monkeypatch.setattr(bridge, "release_graph", fake_release_graph)

    assert bridge.call_decompose_meet_requirements(instance_id, 1) is True
    assert bridge.call_decompose_replacement(instance_id, 1) == 20260423
    assert bridge.destroy_pass_holder(instance_id) is True


def test_bridge_decompose_replacement_rejects_none(tmp_path, monkeypatch):
    module_path = _write_invalid_replacement_decompose_pass_module(
        tmp_path,
        "invalid_replacement_decompose_pass",
        "InvalidReplacementDecomposePass",
    )
    monkeypatch.setenv(bootstrap.ENV_PY_PASS_PATH, str(module_path))

    descriptors = bridge.load_and_get_pass_descriptors()

    assert len(descriptors) == 1
    descriptor_key = descriptors[0]["descriptor_key"]
    instance_id = "InvalidReplacementDecomposePass#1"
    assert bridge.create_pass_holder(instance_id, descriptor_key)

    class FakeNode:
        type = "Add"

    monkeypatch.setattr(bridge, "borrow_node", lambda _handle: FakeNode())

    with pytest.raises(TypeError, match="must return ge\\.graph\\.Graph"):
        bridge.call_decompose_replacement(instance_id, 1)

    assert bridge.destroy_pass_holder(instance_id) is True
