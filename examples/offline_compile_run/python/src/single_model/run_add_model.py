#!/usr/bin/env python3
# -*- coding: UTF-8 -*-
# ----------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------
import acl
from common import (
    DEVICE_ID,
    check_ret,
    collect_acl_model_outputs,
    copy_inputs_to_acl_dataset,
    create_sample_inputs,
    prepare_acl_mdl_dataset,
    release_acl_mdl_dataset,
)


class AclModelRunner:
    def __init__(self) -> None:
        self.model_path = "add_sample.om"
        self.device_id = DEVICE_ID
        self.model_id = None
        self.model_desc = None
        self.input_dataset = None
        self.output_dataset = None
        self.input_data = []
        self.output_data = []

    def init(self) -> None:
        print("[Info] 初始化运行环境及数据")
        # 初始化 acl
        check_ret("acl.init", acl.init())
        # 指定运行的 Device
        check_ret("acl.rt.set_device", acl.rt.set_device(self.device_id))
        # 从文件加载离线模型数据
        self.model_id, ret = acl.mdl.load_from_file(self.model_path)
        check_ret("acl.mdl.load_from_file", ret)
        # 创建aclmdlDesc类型的数据
        self.model_desc = acl.mdl.create_desc()
        check_ret("acl.mdl.get_desc", acl.mdl.get_desc(self.model_desc, self.model_id))

        self.input_dataset, self.input_data = prepare_acl_mdl_dataset(self.model_desc, "input")
        self.output_dataset, self.output_data = prepare_acl_mdl_dataset(self.model_desc, "output")

    def forward(self, inputs):
        print("[Info] 执行模型推理")
        copy_inputs_to_acl_dataset(self.input_data, inputs)
        check_ret(
            "acl.mdl.execute",
            acl.mdl.execute(self.model_id, self.input_dataset, self.output_dataset),
        )
        return collect_acl_model_outputs(self.model_desc, self.output_data)

    def release(self) -> None:
        print("[Info] 释放所有资源")
        # 释放模型推理的输入、输出资源
        release_acl_mdl_dataset(self.input_dataset)
        release_acl_mdl_dataset(self.output_dataset)

        if self.model_desc is not None:
            check_ret("acl.mdl.destroy_desc", acl.mdl.destroy_desc(self.model_desc))
        if self.model_id is not None:
            check_ret("acl.mdl.unload", acl.mdl.unload(self.model_id))

        acl.rt.reset_device(self.device_id)
        acl.finalize()


def main():
    runner = AclModelRunner()
    try:
        runner.init()
        outputs = runner.forward(create_sample_inputs())
        print("[Info] 模型推理结果:")
        for i, output in enumerate(outputs, start=1):
            print(f"Output[{i}]: \n{output}")
    finally:
        runner.release()


if __name__ == "__main__":
    main()
