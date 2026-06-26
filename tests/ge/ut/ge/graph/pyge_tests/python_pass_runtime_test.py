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

"""Pytest coverage for Python pass fallback artifact discovery."""

import json
import sys
import types
from pathlib import Path

from ge.passes import _artifact_utils as artifact_utils
from ge.passes import runtime


def _write_codegen_config(codegen_dir: Path, content: str = "{}") -> None:
    codegen_dir.mkdir(parents=True, exist_ok=True)
    (codegen_dir / "build_config.json").write_text(content, encoding="utf-8")


def _write_fallback_sources(codegen_dir: Path, resources: dict) -> None:
    lines = [
        f"_RESOURCES = {resources!r}",
        "",
        "def materialize(output_root):",
        "    from pathlib import Path",
        "    output_root = Path(output_root)",
        "    for rel_path, content in _RESOURCES.items():",
        "        dst = output_root / rel_path",
        "        dst.parent.mkdir(parents=True, exist_ok=True)",
        "        dst.write_bytes(content)",
        "",
    ]
    codegen_dir.mkdir(parents=True, exist_ok=True)
    (codegen_dir / "_sources.py").write_text("\n".join(lines), encoding="utf-8")
    (codegen_dir / "__init__.py").write_text("", encoding="utf-8")


def _write_artifact(artifact_dir: Path, python_tag: str, platform_tag: str, bridge_abi: int) -> None:
    artifact_dir.mkdir(parents=True)
    (artifact_dir / "libge_python_pass_bridge.so").touch()
    (artifact_dir / "_ge_pass_native.so").touch()
    manifest = {
        "python_tag": python_tag,
        "platform": platform_tag,
        "bridge_abi": bridge_abi,
        "artifacts": {
            "bridge": "libge_python_pass_bridge.so",
            "native": "_ge_pass_native.so",
        },
    }
    (artifact_dir / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")


def _current_artifact_dir(artifacts_root: Path) -> Path:
    return artifacts_root / f"{artifact_utils.current_python_tag()}-{artifact_utils.current_platform_tag()}"


def test_find_prebuilt_artifact_from_artifacts_root(tmp_path, monkeypatch):
    artifacts_dir = tmp_path / "python_pass_artifacts"
    monkeypatch.setattr(artifact_utils, "artifacts_root", lambda: artifacts_dir)

    artifact_dir = _current_artifact_dir(artifacts_dir)
    assert artifact_dir.parent == artifacts_dir
    _write_artifact(
        artifact_dir,
        artifact_utils.current_python_tag(),
        artifact_utils.current_platform_tag(),
        artifact_utils.BRIDGE_ABI_VERSION,
    )

    artifact = artifact_utils.find_prebuilt_artifact()

    assert artifact is not None
    assert artifact.bridge_path == artifact_dir / "libge_python_pass_bridge.so"
    assert artifact.native_path == artifact_dir / "_ge_pass_native.so"


def test_find_prebuilt_artifact_rejects_runtime_mismatch(tmp_path, monkeypatch):
    artifacts_dir = tmp_path / "python_pass_artifacts"
    monkeypatch.setattr(artifact_utils, "artifacts_root", lambda: artifacts_dir)

    artifact_dir = _current_artifact_dir(artifacts_dir)
    _write_artifact(
        artifact_dir,
        "cp000",
        artifact_utils.current_platform_tag(),
        artifact_utils.BRIDGE_ABI_VERSION,
    )

    assert artifact_utils.find_prebuilt_artifact() is None


def test_find_prebuilt_artifact_rejects_missing_bridge(tmp_path, monkeypatch):
    artifacts_dir = tmp_path / "python_pass_artifacts"
    monkeypatch.setattr(artifact_utils, "artifacts_root", lambda: artifacts_dir)

    artifact_dir = _current_artifact_dir(artifacts_dir)
    _write_artifact(
        artifact_dir,
        artifact_utils.current_python_tag(),
        artifact_utils.current_platform_tag(),
        artifact_utils.BRIDGE_ABI_VERSION,
    )
    (artifact_dir / "libge_python_pass_bridge.so").unlink()

    assert artifact_utils.find_prebuilt_artifact() is None


def test_resolve_build_inputs_returns_codegen_sources(tmp_path, monkeypatch):
    codegen_dir = tmp_path / "codegen"
    source_dir = codegen_dir / "src" / "bridge"
    include_dir = codegen_dir / "include" / "bridge"
    source_dir.mkdir(parents=True)
    include_dir.mkdir(parents=True)
    source_path = source_dir / "bridge.cc"
    source_path.write_text("int x = 0;\n", encoding="utf-8")
    (include_dir / "bridge.h").write_text("#pragma once\n", encoding="utf-8")
    _write_codegen_config(codegen_dir)
    monkeypatch.setattr(runtime, "_codegen_root", lambda: codegen_dir)

    build_inputs = runtime._resolve_build_inputs()
    assert build_inputs is not None
    assert build_inputs.root == codegen_dir
    assert build_inputs.src_dir == codegen_dir / "src"
    assert build_inputs.include_dir == codegen_dir / "include"


def test_resolve_fallback_build_inputs_materializes_resource_module(tmp_path, monkeypatch):
    codegen_dir = tmp_path / "codegen"
    resources = {
        "src/bridge/bridge.cc": b"int bridge = 0;\n",
        "include/bridge/bridge.h": b"#pragma once\n",
        "src/native/native.cc": b"int native = 0;\n",
        "include/native/native.h": b"#pragma once\n",
    }
    _write_codegen_config(codegen_dir)
    _write_fallback_sources(codegen_dir, resources)
    monkeypatch.setattr(runtime, "_codegen_root", lambda: codegen_dir)

    build_inputs = runtime._resolve_fallback_build_inputs(tmp_path / "work")

    assert build_inputs is not None
    assert build_inputs.root == tmp_path / "work" / "fallback_sources"
    assert build_inputs.src_dir == build_inputs.root / "src"
    assert build_inputs.include_dir == build_inputs.root / "include"
    assert (build_inputs.root / "src/bridge/bridge.cc").read_bytes() == resources["src/bridge/bridge.cc"]
    assert not (codegen_dir / "src").exists()
    assert not (codegen_dir / "include").exists()


def test_run_fallback_codegen_publishes_artifact_and_cleans_work_dir(tmp_path, monkeypatch):
    codegen_dir = tmp_path / "codegen"
    resources = {
        "src/bridge/bridge.cc": b"int bridge = 0;\n",
        "include/bridge/bridge.h": b"#pragma once\n",
        "src/native/native.cc": b"int native = 0;\n",
        "include/native/native.h": b"#pragma once\n",
    }
    _write_codegen_config(codegen_dir)
    _write_fallback_sources(codegen_dir, resources)
    artifacts_dir = tmp_path / "python_pass_artifacts"
    monkeypatch.setattr(runtime, "artifacts_root", lambda: artifacts_dir)
    monkeypatch.setattr(runtime, "_codegen_root", lambda: codegen_dir)

    def fake_compile_artifact_set(build_inputs, work_dir):
        assert build_inputs.root == work_dir / "fallback_sources"
        assert (build_inputs.root / "src/bridge/bridge.cc").read_bytes() == resources["src/bridge/bridge.cc"]
        work_dir.mkdir(parents=True, exist_ok=True)
        bridge_path = work_dir / "libge_python_pass_bridge.so"
        native_path = work_dir / "_ge_pass_native.so"
        bridge_path.write_text("bridge", encoding="utf-8")
        native_path.write_text("native", encoding="utf-8")
        python_info = runtime.PythonBuildInfo(
            tag=artifact_utils.current_python_tag(),
            executable=sys.executable,
            version="3.test",
            include_dir=tmp_path,
            library=tmp_path / "libpython.so",
            pybind_include=tmp_path / "pybind11",
        )
        return runtime._CompiledArtifactSet(
            artifact_paths={
                "libge_python_pass_bridge.so": bridge_path,
                "_ge_pass_native.so": native_path,
            },
            python_info=python_info,
        )

    monkeypatch.setattr(runtime, "_compile_artifact_set", fake_compile_artifact_set)

    artifact = runtime.run_fallback_codegen()

    final_dir = runtime._fallback_artifact_dir()
    assert final_dir.parent == artifacts_dir
    assert artifact.root == final_dir.resolve()
    assert (final_dir / "libge_python_pass_bridge.so").read_text(encoding="utf-8") == "bridge"
    assert (final_dir / "_ge_pass_native.so").read_text(encoding="utf-8") == "native"
    manifest = json.loads((final_dir / "manifest.json").read_text(encoding="utf-8"))
    assert manifest["python_tag"] == artifact_utils.current_python_tag()
    assert manifest["platform"] == artifact_utils.current_platform_tag()
    assert manifest["bridge_abi"] == artifact_utils.BRIDGE_ABI_VERSION
    assert not list(final_dir.glob(".work.*"))


def test_ensure_native_module_uses_fallback_when_no_artifact_is_loadable(tmp_path, monkeypatch):
    sys.modules.pop(artifact_utils.NATIVE_MODULE_NAME, None)
    artifact_dir = tmp_path / "generated"
    _write_artifact(
        artifact_dir,
        artifact_utils.current_python_tag(),
        artifact_utils.current_platform_tag(),
        artifact_utils.BRIDGE_ABI_VERSION,
    )
    artifact = artifact_utils.load_artifact_from_dir(artifact_dir)
    assert artifact is not None
    native_module = types.ModuleType(artifact_utils.NATIVE_MODULE_NAME)

    monkeypatch.setattr(runtime, "find_prebuilt_artifact", lambda: None)
    monkeypatch.setattr(runtime, "run_fallback_codegen", lambda: artifact)
    monkeypatch.setattr(runtime, "load_native_module", lambda native_path: native_module)

    assert runtime.ensure_native_module() is native_module
