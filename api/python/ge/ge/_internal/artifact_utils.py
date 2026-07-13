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

"""Shared artifact discovery helpers for GE Python modules."""

from __future__ import annotations

import importlib.util
import json
import platform
import sys
from dataclasses import dataclass
from pathlib import Path
from types import ModuleType
from typing import Callable, Iterable, Optional


@dataclass(frozen=True)
class PythonArtifact:
    root: Path
    manifest_path: Path
    python_tag: str
    platform_tag: str
    abi: int
    native_path: Path
    bridge_path: Optional[Path] = None


def current_python_tag() -> str:
    return f"cp{sys.version_info.major}{sys.version_info.minor}"


def current_platform_tag() -> str:
    return f"{platform.system().lower()}-{platform.machine() or 'unknown'}"


def iter_manifest_paths(root: Path) -> Iterable[Path]:
    if not root.exists():
        return
    root_manifest = root / "manifest.json"
    if root_manifest.is_file():
        yield root_manifest
    for child in sorted(root.iterdir()):
        manifest_path = child / "manifest.json"
        if manifest_path.is_file():
            yield manifest_path


def load_manifest_json(manifest_path: Path) -> dict:
    return json.loads(manifest_path.read_text(encoding="utf-8"))


def _load_artifact_manifest(
    manifest_path: Path, abi_key: str, require_bridge: bool
) -> Optional[PythonArtifact]:
    try:
        manifest = load_manifest_json(manifest_path)
        artifacts = manifest["artifacts"]
        native_path = (manifest_path.parent / artifacts["native"]).resolve()
        bridge_path = (
            (manifest_path.parent / artifacts["bridge"]).resolve()
            if require_bridge
            else None
        )
        artifact = PythonArtifact(
            root=manifest_path.parent.resolve(),
            manifest_path=manifest_path.resolve(),
            python_tag=manifest["python_tag"],
            platform_tag=manifest["platform"],
            abi=int(manifest[abi_key]),
            native_path=native_path,
            bridge_path=bridge_path,
        )
    except (KeyError, TypeError, ValueError, OSError):
        return None
    if not artifact.native_path.is_file():
        return None
    if require_bridge and (
        artifact.bridge_path is None or not artifact.bridge_path.is_file()
    ):
        return None
    return artifact


def load_native_artifact_manifest(manifest_path: Path) -> Optional[PythonArtifact]:
    return _load_artifact_manifest(manifest_path, "native_abi", False)


def load_bridge_artifact_manifest(manifest_path: Path) -> Optional[PythonArtifact]:
    return _load_artifact_manifest(manifest_path, "bridge_abi", True)


def iter_artifacts(
    root: Path, load_manifest: Callable[[Path], Optional[PythonArtifact]]
) -> Iterable[PythonArtifact]:
    for manifest_path in iter_manifest_paths(root):
        artifact = load_manifest(manifest_path)
        if artifact is not None:
            yield artifact


def find_compatible_artifact(
    artifacts: Iterable[PythonArtifact], abi_version: int
) -> Optional[PythonArtifact]:
    python_tag = current_python_tag()
    platform_tag = current_platform_tag()
    for artifact in artifacts:
        if (
            artifact.python_tag == python_tag
            and artifact.platform_tag == platform_tag
            and artifact.abi == abi_version
        ):
            return artifact
    return None


def load_module_from_path(module_name: str, module_path: Path) -> ModuleType:
    loaded_module = sys.modules.get(module_name)
    if loaded_module is not None:
        return loaded_module
    spec = importlib.util.spec_from_file_location(module_name, module_path)
    if spec is None or spec.loader is None:
        raise ImportError(f"Cannot create import spec for {module_path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    try:
        spec.loader.exec_module(module)
    except BaseException:
        sys.modules.pop(module_name, None)
        raise
    return module
