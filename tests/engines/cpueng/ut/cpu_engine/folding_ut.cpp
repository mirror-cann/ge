/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fstream>
#include <mutex>
#include "aicpu_const_folding/folding.h"
#include "stub.h"

namespace {
std::mutex &GetEnvMutex()
{
    static std::mutex env_mutex;
    return env_mutex;
}

bool ReadEnvSafe(const char *name, std::string &value)
{
    std::lock_guard<std::mutex> guard(GetEnvMutex());
    const char *raw = std::getenv(name);
    if (raw == nullptr) {
        return false;
    }
    value.assign(raw);
    return true;
}

void SetEnvSafe(const char *name, const char *value, int overwrite)
{
    std::lock_guard<std::mutex> guard(GetEnvMutex());
    (void)setenv(name, value, overwrite);
}

void UnsetEnvSafe(const char *name)
{
    std::lock_guard<std::mutex> guard(GetEnvMutex());
    (void)unsetenv(name);
}

void SetAllTestAttrs(ge::Operator &op)
{
    ge::AscendString attr1 = "attr1";
    op.SetAttr("testattr1", attr1);
    float32_t attr2 = 0;
    op.SetAttr("testattr2", attr2);
    int32_t attr3 = 0;
    op.SetAttr("attr3", attr3);
    bool attr4 = true;
    op.SetAttr("attr4", attr4);
    ge::DataType attr5 = ge::DT_FLOAT;
    op.SetAttr("attr5", attr5);
    ge::Tensor attr6;
    op.SetAttr("attr6", attr6);

    std::vector<ge::AscendString> attrList1;
    attrList1.push_back("attrList1");
    op.SetAttr("attrList1", attrList1);
    std::vector<float32_t> attrList2;
    attrList2.push_back(0);
    op.SetAttr("attrList2", attrList2);
    std::vector<int32_t> attrList3;
    attrList3.push_back(0);
    op.SetAttr("attrList3", attrList3);
    std::vector<bool> attrList4;
    attrList4.push_back(true);
    op.SetAttr("attrList4", attrList4);
    std::vector<ge::DataType> attrList5;
    attrList5.push_back(ge::DT_FLOAT);
    op.SetAttr("attrList5", attrList5);
    std::vector<ge::Tensor> attrList6;
    ge::Tensor testtensor;
    attrList6.push_back(testtensor);
    op.SetAttr("attrList6", attrList6);
    std::vector<std::vector<int64_t>> attrList7;
    std::vector<int64_t> testvec;
    testvec.push_back(0);
    attrList7.push_back(testvec);
    op.SetAttr("attrList7", attrList7);
}
}  // namespace

class AicpuConstFoldingTest : public ::testing::Test {
protected:
    void SetUp() override {
        has_saved_home_path_ = ReadEnvSafe("ASCEND_HOME_PATH", saved_home_path_);
    }
    void TearDown() override {
        if (has_saved_home_path_) {
            SetEnvSafe("ASCEND_HOME_PATH", saved_home_path_.c_str(), 1);
        } else {
            UnsetEnvSafe("ASCEND_HOME_PATH");
        }
    }
    std::string saved_home_path_;
    bool has_saved_home_path_ = false;
};

TEST_F(AicpuConstFoldingTest, InitCpuConstantFoldingNew_Success)
{
    setenv("ASCEND_HOME_PATH", "/tmp", 1);
    int32_t ret = InitCpuConstantFoldingNew([]() -> ge::HostCpuOp * {
        return new (std::nothrow) ge::HostCpuTestOp();
    });
    ASSERT_EQ(ret, 0);
}

TEST_F(AicpuConstFoldingTest, InitCpuConstantFoldingNew_NoEnvFail)
{
    unsetenv("ASCEND_HOME_PATH");
    int32_t ret = InitCpuConstantFoldingNew([]() -> ge::HostCpuOp * {
        return new (std::nothrow) ge::HostCpuTestOp();
    });
    ASSERT_EQ(ret, -1);
}

TEST_F(AicpuConstFoldingTest, InitCpuConstantFoldingNew_LoadSoTraversal)
{
    std::string tmp_dir = "/tmp/test_folding_ut_" + std::to_string(getpid());
    std::string host_cpu_dir = tmp_dir + "/opp/built-in/op_impl/host_cpu/";
    std::string cmd_mkdir = "mkdir -p " + host_cpu_dir;
    ASSERT_EQ(system(cmd_mkdir.c_str()), 0);

    // Create files matching libopconstant_folding_*.so pattern
    std::ofstream(host_cpu_dir + "libopconstant_folding_math.so").close();
    std::ofstream(host_cpu_dir + "libopconstant_folding_nn.so").close();
    std::ofstream(host_cpu_dir + "libopconstant_folding_cv.so").close();
    // Create files that should NOT match the pattern
    std::ofstream(host_cpu_dir + "libother.so").close();
    std::ofstream(host_cpu_dir + "libmath_constant_folding_ops.so").close();
    std::ofstream(host_cpu_dir + "notlib_constant_folding_ops.so").close();
    // libconstant_folding_ops.so should be excluded (loaded by host_cpu_engine separately)
    std::ofstream(host_cpu_dir + "libconstant_folding_ops.so").close();

    setenv("ASCEND_HOME_PATH", tmp_dir.c_str(), 1);
    int32_t ret = InitCpuConstantFoldingNew([]() -> ge::HostCpuOp * {
        return new (std::nothrow) ge::HostCpuTestOp();
    });
    // Init succeeds even if dlopen fails on empty .so files (just warns)
    ASSERT_EQ(ret, 0);

    std::string cmd_rm = "rm -rf " + tmp_dir;
    system(cmd_rm.c_str());
}

TEST_F(AicpuConstFoldingTest, InitCpuConstantFoldingNew_PathWithTrailingSlash)
{
    setenv("ASCEND_HOME_PATH", "/tmp/", 1);
    int32_t ret = InitCpuConstantFoldingNew([]() -> ge::HostCpuOp * {
        return new (std::nothrow) ge::HostCpuTestOp();
    });
    ASSERT_EQ(ret, 0);
}

TEST_F(AicpuConstFoldingTest, CpuConstantFoldingComputeNew_Success)
{
    setenv("ASCEND_HOME_PATH", "/tmp", 1);
    InitCpuConstantFoldingNew([]() -> ge::HostCpuOp * {
        return new (std::nothrow) ge::HostCpuTestOp();
    });

    ge::Operator op("testop");
    auto op_desc = ge::OpDescUtils::GetOpDescFromOperator(op);
    ge::GeTensorDesc testTensorDesc;
    ge::GeShape testgeshape;
    testgeshape.SetDimNum(2);
    testTensorDesc.SetShape(testgeshape);
    op_desc->AddInputDesc("testop", testTensorDesc);
    op_desc->AddOutputDesc("testop", testTensorDesc);

    SetAllTestAttrs(op);

    std::map<std::string, const ge::Tensor> inputs;
    std::map<std::string, ge::Tensor> outputs;
    ge::Tensor testTensor;
    inputs.emplace("testop", testTensor);
    outputs.emplace("testop", testTensor);
    int32_t ret = CpuConstantFoldingComputeNew(op, inputs, outputs);
    ASSERT_EQ(ret, 0);
}

TEST_F(AicpuConstFoldingTest, CpuConstantFoldingComputeNew_MissingInput)
{
    setenv("ASCEND_HOME_PATH", "/tmp", 1);
    InitCpuConstantFoldingNew([]() -> ge::HostCpuOp * {
        return new (std::nothrow) ge::HostCpuTestOp();
    });

    ge::Operator op("testop");
    auto op_desc = ge::OpDescUtils::GetOpDescFromOperator(op);
    ge::GeTensorDesc testTensorDesc;
    op_desc->AddInputDesc("input1", testTensorDesc);
    op_desc->AddOutputDesc("output1", testTensorDesc);
    // Deliberately not providing required input
    std::map<std::string, const ge::Tensor> inputs;
    std::map<std::string, ge::Tensor> outputs;
    ge::Tensor testTensor;
    outputs.emplace("output1", testTensor);
    int32_t ret = CpuConstantFoldingComputeNew(op, inputs, outputs);
    ASSERT_EQ(ret, -1);
}

TEST_F(AicpuConstFoldingTest, CpuConstantFoldingComputeNew_MissingOutput)
{
    setenv("ASCEND_HOME_PATH", "/tmp", 1);
    InitCpuConstantFoldingNew([]() -> ge::HostCpuOp * {
        return new (std::nothrow) ge::HostCpuTestOp();
    });

    ge::Operator op("testop");
    auto op_desc = ge::OpDescUtils::GetOpDescFromOperator(op);
    ge::GeTensorDesc testTensorDesc;
    op_desc->AddInputDesc("input1", testTensorDesc);
    op_desc->AddOutputDesc("output1", testTensorDesc);
    std::map<std::string, const ge::Tensor> inputs;
    std::map<std::string, ge::Tensor> outputs;
    ge::Tensor testTensor;
    inputs.emplace("input1", testTensor);
    // Deliberately not providing required output
    int32_t ret = CpuConstantFoldingComputeNew(op, inputs, outputs);
    ASSERT_EQ(ret, -1);
}

// 验证: 多个 libopconstant_folding_*.so 共存场景下, Init 不会因为
// 每个 so 都有独立的 CpuKernelRegister 单例而发生 this/成员函数错配的
// coredump, Compute 回落到 V1 路径仍能正常工作.
TEST_F(AicpuConstFoldingTest, InitCpuConstantFoldingNew_MultiOpsSoNoCrash)
{
    std::string tmp_dir = "/tmp/test_folding_ut_multi_" + std::to_string(getpid());
    std::string host_cpu_dir = tmp_dir + "/opp/built-in/op_impl/host_cpu/";
    std::string cmd_mkdir = "mkdir -p " + host_cpu_dir;
    ASSERT_EQ(system(cmd_mkdir.c_str()), 0);

    std::ofstream(host_cpu_dir + "libopconstant_folding_math.so").close();
    std::ofstream(host_cpu_dir + "libopconstant_folding_nn.so").close();
    std::ofstream(host_cpu_dir + "libopconstant_folding_cv.so").close();

    setenv("ASCEND_HOME_PATH", tmp_dir.c_str(), 1);
    int32_t init_ret = InitCpuConstantFoldingNew([]() -> ge::HostCpuOp * {
        return new (std::nothrow) ge::HostCpuTestOp();
    });
    ASSERT_EQ(init_ret, 0);

    ge::Operator op("testop");
    auto op_desc = ge::OpDescUtils::GetOpDescFromOperator(op);
    ge::GeTensorDesc tensor_desc;
    op_desc->AddInputDesc("testop", tensor_desc);
    op_desc->AddOutputDesc("testop", tensor_desc);
    std::map<std::string, const ge::Tensor> inputs;
    std::map<std::string, ge::Tensor> outputs;
    ge::Tensor t;
    inputs.emplace("testop", t);
    outputs.emplace("testop", t);
    int32_t rc = CpuConstantFoldingComputeNew(op, inputs, outputs);
    ASSERT_EQ(rc, 0);

    std::string cmd_rm = "rm -rf " + tmp_dir;
    (void)system(cmd_rm.c_str());
}

// 验证: 当 V2 不命中时, Compute 会回退到本地 V1 GetCpuKernel 路径.
// 覆盖 opbase 侧 "GetCpuKernelV2 不再内建回退" 后 ge 侧的外部兜底链路.
TEST_F(AicpuConstFoldingTest, CpuConstantFoldingComputeNew_V2Miss_FallbackV1)
{
    setenv("ASCEND_HOME_PATH", "/tmp", 1);
    int32_t init_ret = InitCpuConstantFoldingNew([]() -> ge::HostCpuOp * {
        return new (std::nothrow) ge::HostCpuTestOp();
    });
    ASSERT_EQ(init_ret, 0);

    // stub 里 V1 的 GetCpuKernel 总返回非空, V2 反向索引里不会有 "unknown_op",
    // Compute 应当走 V1 路径返回 0.
    ge::Operator op("unknown_op");
    auto op_desc = ge::OpDescUtils::GetOpDescFromOperator(op);
    ge::GeTensorDesc tensor_desc;
    op_desc->AddInputDesc("unknown_op", tensor_desc);
    op_desc->AddOutputDesc("unknown_op", tensor_desc);
    std::map<std::string, const ge::Tensor> inputs;
    std::map<std::string, ge::Tensor> outputs;
    ge::Tensor t;
    inputs.emplace("unknown_op", t);
    outputs.emplace("unknown_op", t);
    int32_t rc = CpuConstantFoldingComputeNew(op, inputs, outputs);
    ASSERT_EQ(rc, 0);
}