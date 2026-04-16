#!/usr/bin/env python3
# -*- coding: UTF-8 -*-
# ----------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------

from ge.passes import FusionBasePass, register_fusion_pass, PassStage, PassContext


@register_fusion_pass(name="TestPass", stage=PassStage.AFTER_INFER_SHAPE)
class TestPass(FusionBasePass):
    def run(self, graph, context):
        # 打印context，并设置pass_name
        print("--------PassContext as follow--------")
        print(context)
        print(context.get_pass_name())
        context.set_pass_name("halo, i'm testpass")
        print(context.get_pass_name())
        # 打印graph，并设置属性
        print("-----------Graph as follow-----------")
        print(graph)
        graph.set_attr("test_attr", "a test attr")
        print(graph.get_attr("test_attr"))
        return True
