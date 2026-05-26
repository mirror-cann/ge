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

"""Python pass artifact discovery helpers."""

from __future__ import annotations

import importlib.util
import json
import platform
import sys
from dataclasses import dataclass
from pathlib import Path
from types import ModuleType
from typing import Iterable, Optional


BRIDGE_ABI_VERSION = 1
NATIVE_MODULE_NAME = "ge.passes._ge_pass_native"


@dataclass(frozen=True)
class PythonPassArtifact:
    root: Path
    manifest_path: Path
    python_tag: str
    platform_tag: str
    bridge_abi: int
    bridge_path: Path
    native_path: Path


def current_python_tag() -> str:
    return f"cp{sys.version_info.major}{sys.version_info.minor}"


def current_platform_tag() -> str:
    return f"{platform.system().lower()}-{platform.machine() or 'unknown'}"


def artifacts_root() -> Path:
    return Path(__file__).resolve().parent / "python_pass_artifacts"


def _manifest_paths(root: Path) -> Iterable[Path]:
    if not root.exists():
        return
    root_manifest = root / "manifest.json"
    if root_manifest.is_file():
        yield root_manifest
    for child in sorted(root.iterdir()):
        manifest_path = child / "manifest.json"
        if manifest_path.is_file():
            yield manifest_path


def _load_manifest(manifest_path: Path) -> Optional[PythonPassArtifact]:
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        artifacts = manifest["artifacts"]
        artifact = PythonPassArtifact(
            root=manifest_path.parent.resolve(),
            manifest_path=manifest_path.resolve(),
            python_tag=manifest["python_tag"],
            platform_tag=manifest["platform"],
            bridge_abi=int(manifest["bridge_abi"]),
            bridge_path=(manifest_path.parent / artifacts["bridge"]).resolve(),
            native_path=(manifest_path.parent / artifacts["native"]).resolve(),
        )
    except (KeyError, TypeError, ValueError, OSError):
        return None
    if not artifact.bridge_path.is_file() or not artifact.native_path.is_file():
        return None
    return artifact


def iter_artifacts(root: Optional[Path] = None) -> Iterable[PythonPassArtifact]:
    for manifest_path in _manifest_paths(root or artifacts_root()):
        artifact = _load_manifest(manifest_path)
        if artifact is not None:
            yield artifact


def find_prebuilt_artifact() -> Optional[PythonPassArtifact]:
    python_tag = current_python_tag()
    platform_tag = current_platform_tag()
    for artifact in iter_artifacts():
        if (artifact.python_tag == python_tag and
                artifact.platform_tag == platform_tag and
                artifact.bridge_abi == BRIDGE_ABI_VERSION):
            return artifact
    return None


def load_native_module(native_path: Path) -> ModuleType:
    loaded_module = sys.modules.get(NATIVE_MODULE_NAME)
    if loaded_module is not None:
        return loaded_module
    spec = importlib.util.spec_from_file_location(NATIVE_MODULE_NAME, native_path)
    if spec is None or spec.loader is None:
        raise ImportError(f"Cannot create import spec for {native_path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[NATIVE_MODULE_NAME] = module
    try:
        spec.loader.exec_module(module)
    except BaseException:
        sys.modules.pop(NATIVE_MODULE_NAME, None)
        raise
    return module
