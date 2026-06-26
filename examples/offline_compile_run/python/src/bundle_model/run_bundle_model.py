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


class AclBundleModelRunner:
    def __init__(self) -> None:
        self.model_path = "bundle_sample.om"
        self.device_id = DEVICE_ID
        self.bundle_id = None
        self.model_ids = []

    @staticmethod
    def run_model(model_id: int, inputs):
        print(f"[Info] 执行子模型推理，model_id: {model_id}")
        model_desc = acl.mdl.create_desc()
        input_dataset = None
        output_dataset = None
        input_data = []
        output_data = []
        try:
            check_ret("acl.mdl.get_desc", acl.mdl.get_desc(model_desc, model_id))
            input_dataset, input_data = prepare_acl_mdl_dataset(model_desc, "input")
            output_dataset, output_data = prepare_acl_mdl_dataset(model_desc, "output")
            copy_inputs_to_acl_dataset(input_data, inputs)
            check_ret(
                "acl.mdl.execute",
                acl.mdl.execute(model_id, input_dataset, output_dataset),
            )
            return collect_acl_model_outputs(model_desc, output_data)
        finally:
            release_acl_mdl_dataset(input_dataset)
            release_acl_mdl_dataset(output_dataset)
            check_ret("acl.mdl.destroy_desc", acl.mdl.destroy_desc(model_desc))

    def init(self) -> None:
        print("[Info] 初始化运行环境及数据")
        # 初始化 acl
        check_ret("acl.init", acl.init())
        # 指定运行的 Device
        check_ret("acl.rt.set_device", acl.rt.set_device(self.device_id))
        # 从文件加载离线模型数据
        self.bundle_id, ret = acl.mdl.bundle_load_from_file(self.model_path)
        check_ret("acl.mdl.bundle_load_from_file", ret)
        # 获取实际可执行的模型数
        model_num, ret = acl.mdl.bundle_get_model_num(self.bundle_id)
        check_ret("acl.mdl.bundle_get_model_num", ret)
        # 获取实际可执行的模型ID
        for index in range(model_num):
            model_id, ret = acl.mdl.bundle_get_model_id(self.bundle_id, index)
            check_ret("acl.mdl.bundle_get_model_id", ret)
            self.model_ids.append(model_id)
        print(f"[Info] Bundle 模型加载成功，子模型个数: {len(self.model_ids)}")

    def release(self) -> None:
        print("[Info] 释放所有资源")
        if self.bundle_id is not None:
            check_ret("acl.mdl.bundle_unload", acl.mdl.bundle_unload(self.bundle_id))
        acl.rt.reset_device(self.device_id)
        acl.finalize()


def main():
    runner = AclBundleModelRunner()
    try:
        runner.init()
        add_outputs = runner.run_model(runner.model_ids[0], create_sample_inputs())
        print("[Info] Add 子模型推理结果:")
        for i, output in enumerate(add_outputs, start=1):
            print(f"Output[{i}]: \n{output}")

        mul_outputs = runner.run_model(runner.model_ids[1], create_sample_inputs())
        print("[Info] Mul 子模型推理结果:")
        for i, output in enumerate(mul_outputs, start=1):
            print(f"Output[{i}]: \n{output}")
    finally:
        runner.release()


if __name__ == "__main__":
    main()
