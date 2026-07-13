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

"""Discovery and loading utilities for Python GE pass plugins."""

from types import ModuleType
from typing import List

from ge._internal.plugin_loader import load_plugins_from_env
from .registry import get_registered_pass_dicts

ENV_PY_PASS_PATH = "ASCEND_GE_PY_PASS_PATH"


def load_pass_plugins() -> List[ModuleType]:
    """Load pass plugins from the env-configured path list."""

    return load_plugins_from_env(
        ENV_PY_PASS_PATH, module_prefix="_ge_py_pass_", plugin_kind="python pass"
    )


def get_registered_passes() -> List[dict]:
    return get_registered_pass_dicts()
