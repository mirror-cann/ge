#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

import os

from .configs import LLMClusterInfo, LlmConfig, LLMRole
from .configs import LlmConfig as LLMConfig
from .data_type import DataType
from .kv_cache_manager import KvCacheManager
from .llm_datadist import LLMDataDist
from .llm_types import (
    BlocksCacheKey,
    CacheDesc,
    CacheKey,
    CacheKeyByIdAndIndex,
    CacheTask,
    KvCache,
    LayerSynchronizer,
    MemInfo,
    Memtype,
    Placement,
    RegisterMemStatus,
    TransferConfig,
    TransferWithCacheKeyConfig,
)
from .status import LLMException, LLMStatusCode, Status
from .tensor import Tensor, TensorDesc

__all__ = [
    "LLMClusterInfo",
    "LLMStatusCode",
    "LLMException",
    "LLMRole",
    "LlmConfig",
    "LLMConfig",
    "TensorDesc",
    "Tensor",
    "DataType",
    "Status",
    "KvCacheManager",
    "CacheDesc",
    "CacheKey",
    "CacheKeyByIdAndIndex",
    "KvCache",
    "BlocksCacheKey",
    "LLMDataDist",
    "Placement",
    "RegisterMemStatus",
    "LayerSynchronizer",
    "TransferConfig",
    "CacheTask",
    "TransferWithCacheKeyConfig",
    "Memtype",
    "MemInfo",
]

_ENV_VAR_NAME_AUTO_USE_UC_MEMORY = "AUTO_USE_UC_MEMORY"

if _ENV_VAR_NAME_AUTO_USE_UC_MEMORY not in os.environ:
    os.environ[_ENV_VAR_NAME_AUTO_USE_UC_MEMORY] = "0"
