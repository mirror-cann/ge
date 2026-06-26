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

from dataflow import utils
from dataflow.data_type import (
    DT_BOOL,
    DT_DOUBLE,
    DT_FLOAT,
    DT_FLOAT16,
    DT_INT8,
    DT_INT16,
    DT_INT32,
    DT_INT64,
    DT_STRING,
    DT_UINT8,
    DT_UINT16,
    DT_UINT32,
    DT_UINT64,
)
from dataflow.dataflow import (
    CountBatch,
    FlowData,
    FlowFlag,
    FlowGraph,
    FlowGraphProcessPoint,
    FlowInfo,
    FlowNode,
    Framework,
    FuncProcessPoint,
    GraphProcessPoint,
    Tensor,
    TensorDesc,
    TimeBatch,
    alloc_tensor,
    finalize,
    init,
)
from dataflow.plugin.torch.torch_plugin import npu_model
from dataflow.pyflow import method, pyflow
from dataflow.utils.msg_type_register import msg_type_register
from dataflow.utils.utils import (
    DfAbortException,
    DfException,
    get_running_device_id,
    get_running_instance_id,
    get_running_instance_num,
)
