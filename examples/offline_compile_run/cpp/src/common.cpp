/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "common.h"
#include "ge/es_graph_builder.h"
#include <iostream>
#include <memory>

namespace {
ge::Status PrintOneInferOutputBuffer(aclmdlDesc *model_desc, size_t buffer_index, aclDataBuffer *data_buffer) {
  void *data = aclGetDataBufferAddr(data_buffer);
  const size_t len = aclGetDataBufferSizeV2(data_buffer);
  if (data == nullptr || len == 0U) {
    std::cerr << "[Error] 输出 DataBuffer 无效, index=" << buffer_index << "\n";
    return ge::FAILED;
  }
  aclmdlIODims dims{};
  aclError err = aclmdlGetOutputDims(model_desc, buffer_index, &dims);
  if (err != ACL_SUCCESS) {
    std::cerr << "[Error] aclmdlGetOutputDims 失败, aclError=" << err << "\n";
    return ge::FAILED;
  }
  const size_t elem_count = len >= sizeof(float) ? len / sizeof(float) : 0U;
  std::vector<float> host(elem_count);
  err = aclrtMemcpy(host.data(), len, data, len, ACL_MEMCPY_DEVICE_TO_HOST);
  if (err != ACL_SUCCESS) {
    std::cerr << "[Error] aclrtMemcpy(output D2H) 失败, aclError=" << err << "\n";
    return ge::FAILED;
  }
  const float *out_data = host.data();
  std::cout << "Output[" << (buffer_index + 1) << "] shape: ";
  for (size_t d = 0; d < dims.dimCount; ++d) {
    std::cout << (d == 0 ? "" : ", ") << dims.dims[d];
  }
  std::cout << "\n data:";
  for (size_t j = 0; j < elem_count; ++j) {
    std::cout << (j % 3 == 0 ? "\n " : " ") << out_data[j];
  }
  std::cout << std::endl;
  return ge::SUCCESS;
}
}  // namespace

ge::Status CreateIoDataset(aclmdlDesc *model_desc, bool is_input, IoBuffers *out) {
  if (out == nullptr) {
    return ge::FAILED;
  }
  *out = IoBuffers{};
  const size_t io_num = is_input ? aclmdlGetNumInputs(model_desc) : aclmdlGetNumOutputs(model_desc);
  out->dataset = aclmdlCreateDataset();
  if (out->dataset == nullptr) {
    std::cerr << "[Error] aclmdlCreateDataset 失败\n";
    return ge::FAILED;
  }
  for (size_t i = 0; i < io_num; ++i) {
    const size_t buf_size =
        is_input ? aclmdlGetInputSizeByIndex(model_desc, i) : aclmdlGetOutputSizeByIndex(model_desc, i);
    void *dev = nullptr;
    aclError err = aclrtMalloc(&dev, buf_size, ACL_MEM_MALLOC_NORMAL_ONLY);
    if (err != ACL_SUCCESS) {
      std::cerr << "[Error] aclrtMalloc 失败, aclError=" << err << "\n";
      ReleaseDataset(out->dataset);
      *out = IoBuffers{};
      return ge::FAILED;
    }
    aclDataBuffer *data_buffer = aclCreateDataBuffer(dev, buf_size);
    if (data_buffer == nullptr) {
      std::cerr << "[Error] aclCreateDataBuffer 失败\n";
      (void)aclrtFree(dev);
      ReleaseDataset(out->dataset);
      *out = IoBuffers{};
      return ge::FAILED;
    }
    err = aclmdlAddDatasetBuffer(out->dataset, data_buffer);
    if (err != ACL_SUCCESS) {
      std::cerr << "[Error] aclmdlAddDatasetBuffer 失败, aclError=" << err << "\n";
      (void)aclDestroyDataBuffer(data_buffer);
      (void)aclrtFree(dev);
      ReleaseDataset(out->dataset);
      *out = IoBuffers{};
      return ge::FAILED;
    }
    out->device_ptrs.push_back(dev);
    out->sizes.push_back(buf_size);
  }
  return ge::SUCCESS;
}

ge::Status CopyFloatInputs(const std::vector<std::vector<float>> &host_inputs, const IoBuffers &inputs) {
  if (host_inputs.size() > inputs.device_ptrs.size()) {
    std::cerr << "[Error] CopyFloatInputs: host 输入个数超过 device buffer 数\n";
    return ge::FAILED;
  }
  for (size_t i = 0; i < host_inputs.size(); ++i) {
    const auto &data = host_inputs[i];
    const size_t bytes = data.size() * sizeof(float);
    if (bytes > inputs.sizes[i]) {
      std::cerr << "[Error] CopyFloatInputs: 输入 " << i << " 超过 buffer 大小\n";
      return ge::FAILED;
    }
    aclError err = aclrtMemcpy(inputs.device_ptrs[i], inputs.sizes[i], data.data(), bytes, ACL_MEMCPY_HOST_TO_DEVICE);
    if (err != ACL_SUCCESS) {
      std::cerr << "[Error] aclrtMemcpy(input) 失败, aclError=" << err << "\n";
      return ge::FAILED;
    }
  }
  return ge::SUCCESS;
}

ge::Status PrintInferOutputDataset(aclmdlDesc *model_desc, aclmdlDataset *output_dataset) {
  if (output_dataset == nullptr) {
    std::cerr << "[Error] output dataset 为空\n";
    return ge::FAILED;
  }
  const size_t num_buffers = aclmdlGetDatasetNumBuffers(output_dataset);
  for (size_t i = 0; i < num_buffers; ++i) {
    aclDataBuffer *data_buffer = aclmdlGetDatasetBuffer(output_dataset, i);
    if (data_buffer == nullptr) {
      std::cerr << "[Error] aclmdlGetDatasetBuffer 失败, index=" << i << "\n";
      return ge::FAILED;
    }
    const ge::Status st = PrintOneInferOutputBuffer(model_desc, i, data_buffer);
    if (st != ge::SUCCESS) {
      return st;
    }
  }
  return ge::SUCCESS;
}

ge::Status PrintModelOutputs(aclmdlDesc *model_desc, const IoBuffers &outputs) {
  return PrintInferOutputDataset(model_desc, outputs.dataset);
}

std::vector<std::vector<float>> SampleInputs() {
  return {{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F}, {10.0F, 20.0F, 30.0F, 40.0F, 50.0F, 60.0F}};
}

std::unique_ptr<ge::Graph> MakeOfflineAddGraph() {
  auto graph_builder = std::make_unique<ge::es::EsGraphBuilder>("OfflineAddGraph");
  auto input_x = graph_builder->CreateInput(0, "input_x", ge::DT_FLOAT, ge::FORMAT_ND, {2, 3});
  auto input_y = graph_builder->CreateInput(1, "input_y", ge::DT_FLOAT, ge::FORMAT_ND, {2, 3});
  auto result = input_x + input_y;
  (void)graph_builder->SetOutput(result, 0);
  return graph_builder->BuildAndReset();
}

std::unique_ptr<ge::Graph> MakeOfflineMulGraph() {
  auto graph_builder = std::make_unique<ge::es::EsGraphBuilder>("OfflineMulGraph");
  auto input_x = graph_builder->CreateInput(0, "input_x", ge::DT_FLOAT, ge::FORMAT_ND, {2, 3});
  auto input_y = graph_builder->CreateInput(1, "input_y", ge::DT_FLOAT, ge::FORMAT_ND, {2, 3});
  auto result = input_x * input_y;
  (void)graph_builder->SetOutput(result, 0);
  return graph_builder->BuildAndReset();
}

void ReleaseDataset(aclmdlDataset *dataset) {
  if (dataset == nullptr) {
    return;
  }
  const size_t num = aclmdlGetDatasetNumBuffers(dataset);
  for (size_t i = 0; i < num; ++i) {
    aclDataBuffer *buf = aclmdlGetDatasetBuffer(dataset, i);
    if (buf != nullptr) {
      void *dev = aclGetDataBufferAddr(buf);
      if (dev != nullptr) {
        (void)aclrtFree(dev);
      }
      (void)aclDestroyDataBuffer(buf);
    }
  }
  (void)aclmdlDestroyDataset(dataset);
}

void TeardownAclSingleModelInfer(IoBuffers *input_io, IoBuffers *output_io, aclmdlDesc *model_desc, uint32_t model_id) {
  if (input_io != nullptr) {
    ReleaseDataset(input_io->dataset);
  }
  if (output_io != nullptr) {
    ReleaseDataset(output_io->dataset);
  }
  if (model_desc != nullptr) {
    (void)aclmdlDestroyDesc(model_desc);
  }
  if (model_id != 0U) {
    (void)aclmdlUnload(model_id);
  }
  (void)aclrtResetDevice(kDeviceId);
  (void)aclFinalize();
}

void TeardownAclBundleInfer(uint32_t bundle_id) {
  if (bundle_id != 0U) {
    (void)aclmdlBundleUnload(bundle_id);
  }
  (void)aclrtResetDevice(kDeviceId);
  (void)aclFinalize();
}

void TeardownBundleSubmodelSession(aclmdlDesc *model_desc, IoBuffers *input_io, IoBuffers *output_io) {
  if (input_io != nullptr) {
    ReleaseDataset(input_io->dataset);
  }
  if (output_io != nullptr) {
    ReleaseDataset(output_io->dataset);
  }
  if (model_desc != nullptr) {
    (void)aclmdlDestroyDesc(model_desc);
  }
}
