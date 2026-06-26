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

"""
tbe register
"""

from tbe.common.register.class_manager import FusionPassItem, InvokeStage, OpCompute, Operator, Priority
from tbe.common.register.register_api import (
    get_all_fusion_pass,
    get_fusion_buildcfg,
    get_op_compute,
    get_op_register_pattern,
    get_operator,
    get_param_generalization,
    get_tune_param_check_supported,
    get_tune_space,
    register_fusion_pass,
    register_op_compute,
    register_op_param_pass,
    register_operator,
    register_param_generalization,
    register_pass_for_fusion,
    register_tune_param_check_supported,
    register_tune_space,
    reset,
    set_fusion_buildcfg,
)

from . import fusion_pass
