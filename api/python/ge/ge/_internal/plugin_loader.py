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

"""Shared Python plugin loading helpers."""

from __future__ import annotations

import importlib
import importlib.util
import os
import sys
from pathlib import Path
from types import ModuleType
from typing import Iterable, List


def normalize_path_list(path_value: str) -> List[str]:
    if not path_value:
        return []
    return [item.strip() for item in path_value.split(os.pathsep) if item.strip()]


def load_module_from_file(
    file_path: Path, *, module_prefix: str, plugin_kind: str
) -> ModuleType:
    module_name = f"{module_prefix}{file_path.stem}_{abs(hash(str(file_path)))}"
    if module_name in sys.modules:
        return sys.modules[module_name]
    spec = importlib.util.spec_from_file_location(module_name, file_path)
    if spec is None or spec.loader is None:
        raise ImportError(f"cannot load {plugin_kind} file: {file_path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return module


def scan_modules_from_path(
    path_item: str, *, module_prefix: str, plugin_kind: str
) -> List[ModuleType]:
    path = Path(path_item)
    if not path.exists():
        raise FileNotFoundError(f"{plugin_kind} path does not exist: {path}")
    if path.is_file():
        if path.suffix != ".py":
            raise ValueError(f"{plugin_kind} file must end with .py: {path}")
        return [
            load_module_from_file(
                path, module_prefix=module_prefix, plugin_kind=plugin_kind
            )
        ]

    modules: List[ModuleType] = []
    if str(path) not in sys.path:
        sys.path.insert(0, str(path))
    for child in sorted(path.iterdir(), key=lambda item: item.name):
        if child.name.startswith("_"):
            continue
        if child.is_file() and child.suffix == ".py":
            modules.append(
                load_module_from_file(
                    child, module_prefix=module_prefix, plugin_kind=plugin_kind
                )
            )
            continue
        if child.is_dir() and (child / "__init__.py").exists():
            modules.append(importlib.import_module(child.name))
    return modules


def load_plugins_from_env(
    env_name: str, *, module_prefix: str, plugin_kind: str
) -> List[ModuleType]:
    loaded_modules: List[ModuleType] = []
    seen_module_names = set()

    def append_modules(modules: Iterable[ModuleType]) -> None:
        for module in modules:
            module_name = getattr(module, "__name__", "")
            if module_name in seen_module_names:
                continue
            seen_module_names.add(module_name)
            loaded_modules.append(module)

    for path_item in normalize_path_list(os.getenv(env_name, "")):
        append_modules(
            scan_modules_from_path(
                path_item, module_prefix=module_prefix, plugin_kind=plugin_kind
            )
        )
    return loaded_modules
