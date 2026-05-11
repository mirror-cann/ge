/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <iostream>
#include <vector>
#include "acl/acl.h"

#define INFO_LOG(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define ERROR_LOG(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)

int main(int argc, char *argv[]) {
    // 1. 初始化
    if (argc != 2) {
        ERROR_LOG("Usage: %s <model_path>", argv[0]);
        return 1;
    }
    const char* modelPath = argv[1];
    aclInit(nullptr);
    int32_t deviceId = 0;
    aclrtSetDevice(deviceId);

    // 2. 加载模型
    uint32_t modelId;
    aclmdlLoadFromFile(modelPath, &modelId);
    aclmdlDesc *modelDesc = aclmdlCreateDesc();
    aclmdlGetDesc(modelDesc, modelId);

    // 3. 准备输入数据 (假设 Add 算子有两个输入，每个 1024 字节)
    aclmdlDataset *inputDataset = aclmdlCreateDataset();
    size_t numInputs = aclmdlGetNumInputs(modelDesc);

    for (size_t i = 0; i < numInputs; ++i) {
        size_t bufferSize = aclmdlGetInputSizeByIndex(modelDesc, i);
        void* devPtr = nullptr;
        aclrtMalloc(&devPtr, bufferSize, ACL_MEM_MALLOC_NORMAL_ONLY);

        // 构造一点测试数据 (全 1.0f)
        std::vector<float> hostData(bufferSize / sizeof(float), 1.0f);
        aclrtMemcpy(devPtr, bufferSize, hostData.data(), bufferSize, ACL_MEMCPY_HOST_TO_DEVICE);

        aclDataBuffer* inputData = aclCreateDataBuffer(devPtr, bufferSize);
        aclmdlAddDatasetBuffer(inputDataset, inputData);
    }

    // 4. 准备输出数据
    aclmdlDataset *outputDataset = aclmdlCreateDataset();
    size_t bufferSizeOut = aclmdlGetOutputSizeByIndex(modelDesc, 0);
    void* devPtrOut = nullptr;
    aclrtMalloc(&devPtrOut, bufferSizeOut, ACL_MEM_MALLOC_NORMAL_ONLY);
    aclDataBuffer* outputData = aclCreateDataBuffer(devPtrOut, bufferSizeOut);
    aclmdlAddDatasetBuffer(outputDataset, outputData);

    // 5. 模型执行 (同步)
    aclError ret = aclmdlExecute(modelId, inputDataset, outputDataset);
    if (ret == ACL_SUCCESS) {
        INFO_LOG("Model executed successfully!");

        // 将结果拷贝回 Host 查看
        std::vector<float> hostOut(bufferSizeOut / sizeof(float));
        aclrtMemcpy(hostOut.data(), bufferSizeOut, devPtrOut, bufferSizeOut, ACL_MEMCPY_DEVICE_TO_HOST);
        INFO_LOG("First element of output: %f", hostOut[0]);
    } else {
        ERROR_LOG("Model execution failed, error code: %d", ret);
    }

    // 6. 释放资源 (此处为简化省略了循环遍历释放 Dataset 中的 Buffer)
    aclmdlDestroyDesc(modelDesc);
    aclmdlUnload(modelId);
    aclrtResetDevice(deviceId);
    aclFinalize();

    return 0;
}
