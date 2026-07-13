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

"""Python runtime native artifact discovery helpers."""

from __future__ import annotations

from pathlib import Path
from types import ModuleType
from typing import Iterable, Optional

from ge._internal.artifact_utils import (
    PythonArtifact,
    find_compatible_artifact,
    iter_artifacts as iter_artifacts_from_root,
    load_native_artifact_manifest,
    load_module_from_path,
)

NATIVE_ABI_VERSION = 1
NATIVE_MODULE_NAME = "ge.runtime._ge_runtime_native"


def artifacts_root() -> Path:
    return Path(__file__).resolve().parent / "python_runtime_artifacts"


def iter_artifacts(root: Optional[Path] = None) -> Iterable[PythonArtifact]:
    return iter_artifacts_from_root(
        root or artifacts_root(), load_native_artifact_manifest
    )


def find_prebuilt_artifact() -> Optional[PythonArtifact]:
    return find_compatible_artifact(iter_artifacts(), NATIVE_ABI_VERSION)


def load_native_module(native_path: Path) -> ModuleType:
    return load_module_from_path(NATIVE_MODULE_NAME, native_path)
