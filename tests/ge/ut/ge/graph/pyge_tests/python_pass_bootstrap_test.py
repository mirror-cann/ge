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

"""Pytest coverage for env-driven Python GE pass bootstrap."""

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
    pytest.skip(f"无法导入 ge pass 模块: {exc}", allow_module_level=True)

Graph = graph_module.Graph


def _write_python_pass_module(dir_path: Path, module_name: str, pass_name: str) -> Path:
    file_path = dir_path / f"{module_name}.py"
    file_path.write_text(
        textwrap.dedent(f"""
        from ge.passes import FusionBasePass, PassStage, register_fusion_pass
        from ge.graph import Graph

        @register_fusion_pass(name="{pass_name}", stage=PassStage.AFTER_INFER_SHAPE)
        class {pass_name}(FusionBasePass):
            def __init__(self):
                self.counter = 0

            def run(self, graph, context):
                assert isinstance(graph, Graph)
                self.counter += 1
                return self.counter
    """).strip()
        + "\n",
        encoding="utf-8",
    )
    return file_path


@pytest.fixture(autouse=True)
def clear_python_pass_runtime(monkeypatch):
    monkeypatch.delenv(bootstrap.ENV_PY_PASS_PATH, raising=False)
    passes.clear_registered_passes()
    bridge.clear_pass_holders()
    yield
    bridge.clear_pass_holders()
    passes.clear_registered_passes()


def test_load_pass_plugins_from_env_file(tmp_path, monkeypatch):
    module_path = _write_python_pass_module(tmp_path, "env_file_pass", "EnvFilePass")
    monkeypatch.setenv(bootstrap.ENV_PY_PASS_PATH, str(module_path))

    modules = bootstrap.load_pass_plugins()

    assert len(modules) == 1
    assert [item.pass_name for item in passes.get_registered_passes()] == ["EnvFilePass"]


def test_load_pass_plugins_from_env_directory(tmp_path, monkeypatch):
    _write_python_pass_module(tmp_path, "env_dir_pass_a", "EnvDirPassA")
    _write_python_pass_module(tmp_path, "env_dir_pass_b", "EnvDirPassB")
    monkeypatch.setenv(bootstrap.ENV_PY_PASS_PATH, str(tmp_path))

    modules = bootstrap.load_pass_plugins()

    assert len(modules) == 2
    assert sorted(item.pass_name for item in passes.get_registered_passes()) == [
        "EnvDirPassA",
        "EnvDirPassB",
    ]


def test_bridge_create_run_destroy_holder_from_env(tmp_path, monkeypatch):
    module_path = _write_python_pass_module(tmp_path, "bridge_pass", "BridgePass")
    monkeypatch.setenv(bootstrap.ENV_PY_PASS_PATH, str(module_path))

    descriptors = bridge.load_and_get_pass_descriptors()

    assert len(descriptors) == 1
    descriptor_key = descriptors[0]["descriptor_key"]
    first_instance_id = "BridgePass#1"
    second_instance_id = "BridgePass#2"

    assert bridge.create_pass_holder(first_instance_id, descriptor_key)
    assert bridge.create_pass_holder(second_instance_id, descriptor_key)

    first_graph = Graph("g1")
    second_graph = Graph("g2")
    assert bridge.run_fusion_base_pass(first_instance_id, graph=first_graph, context=None) == 1
    assert bridge.run_fusion_base_pass(first_instance_id, graph=first_graph, context=None) == 2
    assert bridge.run_fusion_base_pass(second_instance_id, graph=second_graph, context=None) == 1
    assert bridge.destroy_pass_holder(first_instance_id)
    assert bridge.destroy_pass_holder(second_instance_id)
