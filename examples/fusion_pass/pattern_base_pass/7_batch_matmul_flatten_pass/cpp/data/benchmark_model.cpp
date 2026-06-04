/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "acl/acl.h"
#include "acl/acl_mdl.h"
#include "acl/acl_rt.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <stdexcept>
#include <iomanip>

namespace {
constexpr uint32_t kDeviceId = 0;
constexpr int32_t kWarmupIterations = 50;
constexpr int32_t kBenchmarkIterations = 1000;
constexpr int32_t kSeparatorWidth = 80;
constexpr double kMsPerSec = 1000.0;
constexpr int32_t kArgvBenchItersIndex = 1;
constexpr int32_t kArgvWarmupItersIndex = 2;
constexpr int32_t kMinArgcForBenchIters = kArgvBenchItersIndex + 1;
constexpr int32_t kMinArgcForWarmupIters = kArgvWarmupItersIndex + 1;
const std::string kDefaultOmPath = "model_fused.om";

int32_t ParseIntArg(const char *arg, const std::string &name) {
    try {
        int32_t val = std::stoi(arg);
        if (val <= 0) {
            std::cerr << "[Error] Invalid " << name << ": " << arg << std::endl;
            return -1;
        }
        return val;
    } catch (const std::exception &) {
        std::cerr << "[Error] Invalid " << name << ": " << arg << std::endl;
        return -1;
    }
}

struct ModelBuffers {
    aclmdlDataset *input_dataset = nullptr;
    aclmdlDataset *output_dataset = nullptr;
    std::vector<void*> input_dev_ptrs;
    std::vector<void*> output_dev_ptrs;
    std::vector<size_t> input_sizes;
    std::vector<size_t> output_sizes;
};

struct BenchResult {
    aclError error;
    double total_ms;
    double avg_ms;
};

void ReleaseBuffers(ModelBuffers &buffers) {
    if (buffers.input_dataset != nullptr) {
        size_t num = aclmdlGetDatasetNumBuffers(buffers.input_dataset);
        for (size_t i = 0; i < num; ++i) {
            aclDataBuffer *buf = aclmdlGetDatasetBuffer(buffers.input_dataset, i);
            if (buf != nullptr) {
                void *dev = aclGetDataBufferAddr(buf);
                if (dev != nullptr) {
                    (void)aclrtFree(dev);
                }
                (void)aclDestroyDataBuffer(buf);
            }
        }
        (void)aclmdlDestroyDataset(buffers.input_dataset);
        buffers.input_dataset = nullptr;
    }
    if (buffers.output_dataset != nullptr) {
        size_t num = aclmdlGetDatasetNumBuffers(buffers.output_dataset);
        for (size_t i = 0; i < num; ++i) {
            aclDataBuffer *buf = aclmdlGetDatasetBuffer(buffers.output_dataset, i);
            if (buf != nullptr) {
                void *dev = aclGetDataBufferAddr(buf);
                if (dev != nullptr) {
                    (void)aclrtFree(dev);
                }
                (void)aclDestroyDataBuffer(buf);
            }
        }
        (void)aclmdlDestroyDataset(buffers.output_dataset);
        buffers.output_dataset = nullptr;
    }
    buffers.input_dev_ptrs.clear();
    buffers.output_dev_ptrs.clear();
    buffers.input_sizes.clear();
    buffers.output_sizes.clear();
}

aclError AddBuffersToDataset(aclmdlDesc *model_desc, aclmdlDataset *dataset,
                              bool is_input, std::vector<void*> &dev_ptrs,
                              std::vector<size_t> &sizes) {
    size_t num = is_input ? aclmdlGetNumInputs(model_desc) : aclmdlGetNumOutputs(model_desc);
    for (size_t i = 0; i < num; ++i) {
        size_t size = is_input ? aclmdlGetInputSizeByIndex(model_desc, i)
                                : aclmdlGetOutputSizeByIndex(model_desc, i);
        void *dev = nullptr;
        aclError err = aclrtMalloc(&dev, size, ACL_MEM_MALLOC_NORMAL_ONLY);
        if (err != ACL_SUCCESS) {
            std::cerr << "[Error] aclrtMalloc failed for "
                      << (is_input ? "input" : "output") << " " << i << std::endl;
            return ACL_ERROR_INTERNAL_ERROR;
        }
        aclDataBuffer *buf = aclCreateDataBuffer(dev, size);
        if (buf == nullptr) {
            (void)aclrtFree(dev);
            return ACL_ERROR_INTERNAL_ERROR;
        }
        err = aclmdlAddDatasetBuffer(dataset, buf);
        if (err != ACL_SUCCESS) {
            (void)aclDestroyDataBuffer(buf);
            (void)aclrtFree(dev);
            return err;
        }
        dev_ptrs.push_back(dev);
        sizes.push_back(size);
    }
    return ACL_SUCCESS;
}

aclError CreateBuffers(aclmdlDesc *model_desc, ModelBuffers &buffers) {
    buffers.input_dataset = aclmdlCreateDataset();
    buffers.output_dataset = aclmdlCreateDataset();

    if (buffers.input_dataset == nullptr || buffers.output_dataset == nullptr) {
        std::cerr << "[Error] Failed to create datasets" << std::endl;
        ReleaseBuffers(buffers);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    if (AddBuffersToDataset(model_desc, buffers.input_dataset, true,
                            buffers.input_dev_ptrs, buffers.input_sizes) != ACL_SUCCESS) {
        ReleaseBuffers(buffers);
        return ACL_ERROR_INTERNAL_ERROR;
    }
    if (AddBuffersToDataset(model_desc, buffers.output_dataset, false,
                            buffers.output_dev_ptrs, buffers.output_sizes) != ACL_SUCCESS) {
        ReleaseBuffers(buffers);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    return ACL_SUCCESS;
}

void PrintBanner(const std::string &om_path, int32_t warmup_iters, int32_t bench_iters) {
    std::cout << "\n" << std::string(kSeparatorWidth, '=') << std::endl;
    std::cout << "Testing Fused Model (Reshape+MatMulV2+Reshape)" << std::endl;
    std::cout << "OM Path: " << om_path << std::endl;
    std::cout << "Warmup: " << warmup_iters << " iterations" << std::endl;
    std::cout << "Benchmark: " << bench_iters << " iterations" << std::endl;
    std::cout << std::string(kSeparatorWidth, '=') << std::endl;
}

aclError LoadModelFromFile(const std::string &om_path, uint32_t &model_id, aclmdlDesc *&model_desc) {
    aclError err = aclmdlLoadFromFile(om_path.c_str(), &model_id);
    if (err != ACL_SUCCESS) {
        std::cerr << "[Error] Failed to load model from " << om_path
                  << ", error=" << err << std::endl;
        return err;
    }
    std::cout << "[Info] Model loaded successfully, model_id=" << model_id << std::endl;

    model_desc = aclmdlCreateDesc();
    if (model_desc == nullptr) {
        std::cerr << "[Error] Failed to create model desc" << std::endl;
        (void)aclmdlUnload(model_id);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    err = aclmdlGetDesc(model_desc, model_id);
    if (err != ACL_SUCCESS) {
        std::cerr << "[Error] Failed to get model desc" << std::endl;
        (void)aclmdlDestroyDesc(model_desc);
        model_desc = nullptr;
        (void)aclmdlUnload(model_id);
        return err;
    }

    return ACL_SUCCESS;
}

void PrintModelInfo(aclmdlDesc *model_desc) {
    std::cout << "\n[Info] Model Information:" << std::endl;
    std::cout << "  Input count: " << aclmdlGetNumInputs(model_desc) << std::endl;
    std::cout << "  Output count: " << aclmdlGetNumOutputs(model_desc) << std::endl;

    size_t input_num = aclmdlGetNumInputs(model_desc);
    for (size_t i = 0; i < input_num; ++i) {
        aclmdlIODims dims;
        aclError dims_err = aclmdlGetInputDims(model_desc, i, &dims);
        if (dims_err != ACL_SUCCESS) {
            std::cerr << "[Warning] Failed to get input dims for index " << i << std::endl;
            continue;
        }
        std::cout << "  Input[" << i << "] dims: ";
        for (size_t d = 0; d < dims.dimCount; ++d) {
            std::cout << (d > 0 ? ", " : "") << dims.dims[d];
        }
        std::cout << std::endl;
    }

    size_t output_num = aclmdlGetNumOutputs(model_desc);
    for (size_t i = 0; i < output_num; ++i) {
        aclmdlIODims dims;
        aclError dims_err = aclmdlGetOutputDims(model_desc, i, &dims);
        if (dims_err != ACL_SUCCESS) {
            std::cerr << "[Warning] Failed to get output dims for index " << i << std::endl;
            continue;
        }
        std::cout << "  Output[" << i << "] dims: ";
        for (size_t d = 0; d < dims.dimCount; ++d) {
            std::cout << (d > 0 ? ", " : "") << dims.dims[d];
        }
        std::cout << std::endl;
    }
}

aclError RunWarmup(uint32_t model_id, ModelBuffers &buffers, aclrtStream stream, int32_t warmup_iters) {
    std::cout << "\n[Info] Warmup (" << warmup_iters << " iterations)..." << std::endl;
    for (int32_t i = 0; i < warmup_iters; ++i) {
        aclError exec_err = aclmdlExecuteAsync(model_id, buffers.input_dataset, buffers.output_dataset, stream);
        if (exec_err != ACL_SUCCESS) {
            std::cerr << "[Error] Warmup execute failed at iteration " << i << ", error=" << exec_err << std::endl;
            return exec_err;
        }
    }
    aclError sync_err = aclrtSynchronizeStream(stream);
    if (sync_err != ACL_SUCCESS) {
        std::cerr << "[Error] Warmup synchronize failed, error=" << sync_err << std::endl;
        return sync_err;
    }
    std::cout << "[Info] Warmup completed" << std::endl;
    return ACL_SUCCESS;
}

BenchResult RunBenchmark(uint32_t model_id, ModelBuffers &buffers, aclrtStream stream, int32_t bench_iters) {
    BenchResult result;
    result.error = ACL_SUCCESS;
    result.total_ms = 0.0;
    result.avg_ms = 0.0;

    std::cout << "\n[Info] Benchmark (" << bench_iters << " iterations)..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    for (int32_t i = 0; i < bench_iters; ++i) {
        aclError exec_err = aclmdlExecuteAsync(model_id, buffers.input_dataset, buffers.output_dataset, stream);
        if (exec_err != ACL_SUCCESS) {
            std::cerr << "[Error] Benchmark execute failed at iteration " << i << ", error=" << exec_err << std::endl;
            result.error = exec_err;
            return result;
        }
    }
    aclError sync_err = aclrtSynchronizeStream(stream);
    if (sync_err != ACL_SUCCESS) {
        std::cerr << "[Error] Benchmark synchronize failed, error=" << sync_err << std::endl;
        result.error = sync_err;
        return result;
    }
    auto end = std::chrono::high_resolution_clock::now();

    result.total_ms = std::chrono::duration<double, std::milli>(end - start).count();
    if (bench_iters != 0) {
        result.avg_ms = result.total_ms / bench_iters;
    }
    return result;
}

void PrintResults(const BenchResult &result) {
    std::cout << "\n[Info] Performance Results:" << std::endl;
    std::cout << std::fixed << std::setprecision(3);  // 3: 输出精度为3位小数
    std::cout << "  Total time: " << result.total_ms << " ms" << std::endl;
    std::cout << "  Average time per iteration: " << result.avg_ms << " ms" << std::endl;
    if (result.avg_ms > 0.0) {
        std::cout << "  Throughput: " << (kMsPerSec / result.avg_ms) << " iterations/sec" << std::endl;
    } else {
        std::cout << "  Throughput: N/A (avg_ms is zero)" << std::endl;
    }
}

struct ModelResources {
    uint32_t model_id = 0;
    aclmdlDesc *model_desc = nullptr;
    aclrtStream stream = nullptr;
    ModelBuffers buffers;
};

void ReleaseModelResources(ModelResources &res, bool has_stream) {
    if (has_stream) {
        aclError stream_err = aclrtDestroyStream(res.stream);
        if (stream_err != ACL_SUCCESS) {
            std::cerr << "[Warning] aclrtDestroyStream failed, error=" << stream_err << std::endl;
        }
    }
    ReleaseBuffers(res.buffers);
    aclError desc_err = aclmdlDestroyDesc(res.model_desc);
    if (desc_err != ACL_SUCCESS) {
        std::cerr << "[Warning] aclmdlDestroyDesc failed, error=" << desc_err << std::endl;
    }
    aclError unload_err = aclmdlUnload(res.model_id);
    if (unload_err != ACL_SUCCESS) {
        std::cerr << "[Warning] aclmdlUnload failed, error=" << unload_err << std::endl;
    }
}

aclError LoadAndExecuteModel(const std::string &om_path, int32_t warmup_iters, int32_t bench_iters) {
    PrintBanner(om_path, warmup_iters, bench_iters);

    ModelResources res;
    if (LoadModelFromFile(om_path, res.model_id, res.model_desc) != ACL_SUCCESS) {
        return ACL_ERROR_INTERNAL_ERROR;
    }

    PrintModelInfo(res.model_desc);

    if (CreateBuffers(res.model_desc, res.buffers) != ACL_SUCCESS) {
        std::cerr << "[Error] Failed to create buffers" << std::endl;
        ReleaseModelResources(res, false);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    aclError err = aclrtCreateStream(&res.stream);
    if (err != ACL_SUCCESS) {
        std::cerr << "[Error] Failed to create stream" << std::endl;
        ReleaseModelResources(res, false);
        return err;
    }

    aclError warmup_err = RunWarmup(res.model_id, res.buffers, res.stream, warmup_iters);
    if (warmup_err != ACL_SUCCESS) {
        std::cerr << "[Error] Warmup failed" << std::endl;
        ReleaseModelResources(res, true);
        return warmup_err;
    }

    BenchResult result = RunBenchmark(res.model_id, res.buffers, res.stream, bench_iters);
    if (result.error != ACL_SUCCESS) {
        std::cerr << "[Error] Benchmark failed" << std::endl;
        ReleaseModelResources(res, true);
        return result.error;
    }
    PrintResults(result);

    ReleaseModelResources(res, true);
    std::cout << "[Info] Model test completed" << std::endl;
    return ACL_SUCCESS;
}

}

int main(int argc, char *argv[]) {
    int32_t warmup_iters = kWarmupIterations;
    int32_t bench_iters = kBenchmarkIterations;

    if (argc >= kMinArgcForBenchIters) {
        int32_t val = ParseIntArg(argv[kArgvBenchItersIndex], "benchmark iterations");
        if (val <= 0) {
            return -1;
        }
        bench_iters = val;
    }
    if (argc >= kMinArgcForWarmupIters) {
        int32_t val = ParseIntArg(argv[kArgvWarmupItersIndex], "warmup iterations");
        if (val < 0) {
            return -1;
        }
        warmup_iters = val;
    }

    std::cout << "\n" << std::string(kSeparatorWidth, '=') << std::endl;
    std::cout << "BatchMatMul Flatten Pass Performance Benchmark" << std::endl;
    std::cout << std::string(kSeparatorWidth, '=') << std::endl;

    aclError err = aclInit(nullptr);
    if (err != ACL_SUCCESS) {
        std::cerr << "[Error] aclInit failed, error=" << err << std::endl;
        return -1;
    }

    err = aclrtSetDevice(kDeviceId);
    if (err != ACL_SUCCESS) {
        std::cerr << "[Error] aclrtSetDevice failed, error=" << err << std::endl;
        (void)aclFinalize();
        return -1;
    }

    aclError ret = LoadAndExecuteModel(kDefaultOmPath, warmup_iters, bench_iters);

    aclError reset_err = aclrtResetDevice(kDeviceId);
    if (reset_err != ACL_SUCCESS) {
        std::cerr << "[Warning] aclrtResetDevice failed, error=" << reset_err << std::endl;
    }
    aclError finalize_err = aclFinalize();
    if (finalize_err != ACL_SUCCESS) {
        std::cerr << "[Warning] aclFinalize failed, error=" << finalize_err << std::endl;
    }

    std::cout << "\n[Info] ACL finalized, benchmark completed" << std::endl;
    return (ret == ACL_SUCCESS) ? 0 : -1;
}
