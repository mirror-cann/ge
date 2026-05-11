/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <memory>
#include <vector>
#include <gtest/gtest.h>
#include "exe_graph/runtime/op_compile_context.h"
#include "faker/kernel_run_context_faker.h"
#include "lowering/kernel_run_context_builder.h"
#include "graph/ge_context.h"
#include "graph/ge_local_context.h"
#include "graph/op_desc.h"
#include "platform/platform_infos_def.h"

namespace {
const char *kTestOptionKey1 = "test.option.key1";
const char *kTestOptionValue1 = "test_value_1";
const char *kTestOptionKey2 = "test.option.key2";
const char *kTestOptionValue2 = "test_value_2";
const char *kTestOptionKey3 = "test.option.key3";
}  // namespace
using namespace ge;
namespace gert {
class OpCompileContextUT : public testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {
    ge::GetThreadLocalContext().SetGraphOption({});
    ge::GetThreadLocalContext().SetSessionOption({});
    ge::GetThreadLocalContext().SetGlobalOption({});
  }
};

TEST_F(OpCompileContextUT, GetOptionSuccess) {
  ge::GetThreadLocalContext().SetGraphOption({{kTestOptionKey1, kTestOptionValue1}});

  OpCompileContext context;
  AscendString option_key(kTestOptionKey1);
  AscendString option_value;
  auto ret = context.GetOption(option_key, option_value);

  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
  EXPECT_STREQ(option_value.GetString(), kTestOptionValue1);
}

TEST_F(OpCompileContextUT, GetOptionMultipleOptions) {
  ge::GetThreadLocalContext().SetGraphOption({
      {kTestOptionKey1, kTestOptionValue1},
      {kTestOptionKey2, kTestOptionValue2}
  });

  OpCompileContext context;

  AscendString option_key1(kTestOptionKey1);
  AscendString option_value1;
  auto ret1 = context.GetOption(option_key1, option_value1);
  EXPECT_EQ(ret1, ge::GRAPH_SUCCESS);
  EXPECT_STREQ(option_value1.GetString(), kTestOptionValue1);

  AscendString option_key2(kTestOptionKey2);
  AscendString option_value2;
  auto ret2 = context.GetOption(option_key2, option_value2);
  EXPECT_EQ(ret2, ge::GRAPH_SUCCESS);
  EXPECT_STREQ(option_value2.GetString(), kTestOptionValue2);
}

TEST_F(OpCompileContextUT, GetOptionNotFound) {
  ge::GetThreadLocalContext().SetGraphOption({{kTestOptionKey1, kTestOptionValue1}});

  OpCompileContext context;
  AscendString option_key(kTestOptionKey3);
  AscendString option_value;
  auto ret = context.GetOption(option_key, option_value);

  EXPECT_NE(ret, ge::GRAPH_SUCCESS);
}

TEST_F(OpCompileContextUT, GetOptionEmptyValue) {
  ge::GetThreadLocalContext().SetGraphOption({{kTestOptionKey1, ""}});

  OpCompileContext context;
  AscendString option_key(kTestOptionKey1);
  AscendString option_value;
  auto ret = context.GetOption(option_key, option_value);

  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
  EXPECT_STREQ(option_value.GetString(), "");
}

TEST_F(OpCompileContextUT, GetOptionWithSessionOption) {
  ge::GetThreadLocalContext().SetSessionOption({{kTestOptionKey1, kTestOptionValue1}});

  OpCompileContext context;
  AscendString option_key(kTestOptionKey1);
  AscendString option_value;
  auto ret = context.GetOption(option_key, option_value);

  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
  EXPECT_STREQ(option_value.GetString(), kTestOptionValue1);
}

TEST_F(OpCompileContextUT, GetOptionWithGlobalOption) {
  ge::GetThreadLocalContext().SetGlobalOption({{kTestOptionKey1, kTestOptionValue1}});

  OpCompileContext context;
  AscendString option_key(kTestOptionKey1);
  AscendString option_value;
  auto ret = context.GetOption(option_key, option_value);

  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
  EXPECT_STREQ(option_value.GetString(), kTestOptionValue1);
}

TEST_F(OpCompileContextUT, GetOptionGraphOptionPriority) {
  ge::GetThreadLocalContext().SetGraphOption({{kTestOptionKey1, "graph_value"}});
  ge::GetThreadLocalContext().SetSessionOption({{kTestOptionKey1, "session_value"}});
  ge::GetThreadLocalContext().SetGlobalOption({{kTestOptionKey1, "global_value"}});

  OpCompileContext context;
  AscendString option_key(kTestOptionKey1);
  AscendString option_value;
  auto ret = context.GetOption(option_key, option_value);

  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
  EXPECT_STREQ(option_value.GetString(), "graph_value");
}


TEST_F(OpCompileContextUT, GetOptionLongValue) {
  std::string long_value(1000, 'a');
  ge::GetThreadLocalContext().SetGraphOption({{kTestOptionKey1, long_value}});

  OpCompileContext context;
  AscendString option_key(kTestOptionKey1);
  AscendString option_value;
  auto ret = context.GetOption(option_key, option_value);

  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
  EXPECT_STREQ(option_value.GetString(), long_value.c_str());
}

TEST_F(OpCompileContextUT, OpCompileContextMultipleCalls) {
  ge::GetThreadLocalContext().SetGraphOption({
      {kTestOptionKey1, "context1_value"},
      {kTestOptionKey2, "context2_value"}
  });

  OpCompileContext context;

  AscendString option_key1(kTestOptionKey1);
  AscendString option_value1;
  EXPECT_EQ(context.GetOption(option_key1, option_value1), ge::GRAPH_SUCCESS);
  EXPECT_STREQ(option_value1.GetString(), "context1_value");

  AscendString option_key2(kTestOptionKey2);
  AscendString option_value2;
  EXPECT_EQ(context.GetOption(option_key2, option_value2), ge::GRAPH_SUCCESS);
  EXPECT_STREQ(option_value2.GetString(), "context2_value");
}

TEST_F(OpCompileContextUT, GetInputTensorSuccess) {
  ge::OpDescPtr op_desc = std::make_shared<ge::OpDesc>("test0", "test1");
  ge::GeTensorDesc input_desc(ge::GeShape({8, 32}), ge::FORMAT_ND, ge::DT_FLOAT16);
  input_desc.SetOriginFormat(ge::FORMAT_NCHW);
  op_desc->AddInputDesc("x", input_desc);
  op_desc->AddOutputDesc("y", ge::GeTensorDesc());

  gert::Tensor input_tensor(gert::StorageShape({8, 32}, {8, 32}),
                            {ge::FORMAT_NCHW, ge::FORMAT_ND, gert::ExpandDimsType()},
                            ge::DT_FLOAT16);
  std::vector<void *> inputs = {&input_tensor};
  ge::graphStatus ret = ge::GRAPH_FAILED;
  auto holder = KernelRunContextBuilder().Inputs(inputs).Build(op_desc, ret);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);

  auto *context = reinterpret_cast<OpCompileContext *>(holder.GetKernelContext());
  ASSERT_NE(context, nullptr);
  const auto *tensor = context->GetInputTensor(0U);
  ASSERT_NE(tensor, nullptr);
  EXPECT_EQ(tensor, &input_tensor);
  EXPECT_EQ(tensor->GetShape().GetStorageShape(), gert::Shape({8, 32}));
  EXPECT_EQ(tensor->GetShape().GetOriginShape(), gert::Shape({8, 32}));
  EXPECT_EQ(tensor->GetOriginFormat(), ge::FORMAT_NCHW);
  EXPECT_EQ(tensor->GetStorageFormat(), ge::FORMAT_ND);
  EXPECT_EQ(tensor->GetDataType(), ge::DT_FLOAT16);
}

TEST_F(OpCompileContextUT, GetInputTensorOutOfRange) {
  ge::OpDescPtr op_desc = std::make_shared<ge::OpDesc>("test0", "test1");
  op_desc->AddInputDesc("x", ge::GeTensorDesc());
  op_desc->AddOutputDesc("y", ge::GeTensorDesc());

  gert::Tensor input_tensor(gert::StorageShape({8, 32}, {8, 32}),
                            {ge::FORMAT_NCHW, ge::FORMAT_ND, gert::ExpandDimsType()},
                            ge::DT_FLOAT16);
  std::vector<void *> inputs = {&input_tensor};
  ge::graphStatus ret = ge::GRAPH_FAILED;
  auto holder = KernelRunContextBuilder().Inputs(inputs).Build(op_desc, ret);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);

  auto *context = reinterpret_cast<OpCompileContext *>(holder.GetKernelContext());
  ASSERT_NE(context, nullptr);
  EXPECT_EQ(context->GetInputTensor(1U), nullptr);
}

TEST_F(OpCompileContextUT, GetRequiredInputTensorSuccess) {
  gert::Tensor input_tensor_0 = {{{8, 32}, {8, 32}},
                                 {ge::FORMAT_NCHW, ge::FORMAT_ND, {}},
                                 ge::DT_FLOAT16};
  gert::Tensor input_tensor_1 = {{{16, 32}, {16, 32}},
                                 {ge::FORMAT_ND, ge::FORMAT_FRACTAL_NZ, {}},
                                 ge::DT_INT32};

  auto holder = KernelRunContextFaker()
                    .IrInstanceNum({1, 1})
                    .KernelIONum(2, 0)
                    .NodeIoNum(2, 0)
                    .NodeInputTd(0, ge::DT_FLOAT16, ge::FORMAT_NCHW, ge::FORMAT_ND)
                    .NodeInputTd(1, ge::DT_INT32, ge::FORMAT_ND, ge::FORMAT_FRACTAL_NZ)
                    .Inputs({&input_tensor_0, &input_tensor_1})
                    .Build();

  auto *context = holder.GetContext<OpCompileContext>();
  ASSERT_NE(context, nullptr);
  ASSERT_NE(context->GetRequiredInputTensor(0U), nullptr);
  EXPECT_EQ(context->GetRequiredInputTensor(0U), &input_tensor_0);
  EXPECT_EQ(context->GetRequiredInputTensor(0U)->GetShape().GetStorageShape(), gert::Shape({8, 32}));
  EXPECT_EQ(context->GetRequiredInputTensor(0U)->GetOriginFormat(), ge::FORMAT_NCHW);
  EXPECT_EQ(context->GetRequiredInputTensor(0U)->GetStorageFormat(), ge::FORMAT_ND);
  EXPECT_EQ(context->GetRequiredInputTensor(0U)->GetDataType(), ge::DT_FLOAT16);
  ASSERT_NE(context->GetRequiredInputTensor(1U), nullptr);
  EXPECT_EQ(context->GetRequiredInputTensor(1U), &input_tensor_1);
  EXPECT_EQ(context->GetRequiredInputTensor(1U)->GetStorageFormat(), ge::FORMAT_FRACTAL_NZ);
  EXPECT_EQ(context->GetRequiredInputTensor(1U)->GetDataType(), ge::DT_INT32);
}

TEST_F(OpCompileContextUT, GetRequiredInputTensorOutOfRange) {
  gert::Tensor input_tensor = {{{8, 32}, {8, 32}},
                               {ge::FORMAT_NCHW, ge::FORMAT_ND, {}},
                               ge::DT_FLOAT16};

  auto holder = KernelRunContextFaker()
                    .IrInstanceNum({1})
                    .KernelIONum(1, 0)
                    .NodeIoNum(1, 0)
                    .NodeInputTd(0, ge::DT_FLOAT16, ge::FORMAT_NCHW, ge::FORMAT_ND)
                    .Inputs({&input_tensor})
                    .Build();

  auto *context = holder.GetContext<OpCompileContext>();
  ASSERT_NE(context, nullptr);
  EXPECT_EQ(context->GetRequiredInputTensor(1U), nullptr);
}

TEST_F(OpCompileContextUT, GetOptionalInputTensorSuccess) {
  gert::Tensor input_tensor_0 = {{{8, 32}, {8, 32}},
                                 {ge::FORMAT_NCHW, ge::FORMAT_ND, {}},
                                 ge::DT_FLOAT16};
  gert::Tensor input_tensor_1 = {{{16, 32}, {16, 32}},
                                 {ge::FORMAT_ND, ge::FORMAT_ND, {}},
                                 ge::DT_FLOAT};

  auto holder = KernelRunContextFaker()
                    .IrInstanceNum({1, 1})
                    .KernelIONum(2, 0)
                    .NodeIoNum(2, 0)
                    .NodeInputTd(0, ge::DT_FLOAT16, ge::FORMAT_NCHW, ge::FORMAT_ND)
                    .NodeInputTd(1, ge::DT_FLOAT, ge::FORMAT_ND, ge::FORMAT_ND)
                    .Inputs({&input_tensor_0, &input_tensor_1})
                    .Build();

  auto *context = holder.GetContext<OpCompileContext>();
  ASSERT_NE(context, nullptr);
  ASSERT_NE(context->GetOptionalInputTensor(1U), nullptr);
  EXPECT_EQ(context->GetOptionalInputTensor(1U), &input_tensor_1);
  EXPECT_EQ(context->GetOptionalInputTensor(1U)->GetShape().GetStorageShape(), gert::Shape({16, 32}));
  EXPECT_EQ(context->GetOptionalInputTensor(1U)->GetDataType(), ge::DT_FLOAT);
}

TEST_F(OpCompileContextUT, GetOptionalInputTensorNotInstantiated) {
  gert::Tensor input_tensor = {{{8, 32}, {8, 32}},
                               {ge::FORMAT_NCHW, ge::FORMAT_ND, {}},
                               ge::DT_FLOAT16};

  auto holder = KernelRunContextFaker()
                    .IrInstanceNum({1, 0})
                    .KernelIONum(1, 0)
                    .NodeIoNum(1, 0)
                    .NodeInputTd(0, ge::DT_FLOAT16, ge::FORMAT_NCHW, ge::FORMAT_ND)
                    .Inputs({&input_tensor})
                    .Build();

  auto *context = holder.GetContext<OpCompileContext>();
  ASSERT_NE(context, nullptr);
  EXPECT_EQ(context->GetOptionalInputTensor(1U), nullptr);
}

TEST_F(OpCompileContextUT, GetDynamicInputTensorSuccess) {
  gert::Tensor input_tensor_0 = {{{8, 32}, {8, 32}},
                                 {ge::FORMAT_NCHW, ge::FORMAT_ND, {}},
                                 ge::DT_FLOAT16};
  gert::Tensor input_tensor_1 = {{{16, 32}, {16, 32}},
                                 {ge::FORMAT_ND, ge::FORMAT_ND, {}},
                                 ge::DT_FLOAT};
  gert::Tensor input_tensor_2 = {{{32, 32}, {32, 32}},
                                 {ge::FORMAT_ND, ge::FORMAT_FRACTAL_NZ, {}},
                                 ge::DT_INT32};

  auto holder = KernelRunContextFaker()
                    .IrInstanceNum({1, 2})
                    .KernelIONum(3, 0)
                    .NodeIoNum(3, 0)
                    .NodeInputTd(0, ge::DT_FLOAT16, ge::FORMAT_NCHW, ge::FORMAT_ND)
                    .NodeInputTd(1, ge::DT_FLOAT, ge::FORMAT_ND, ge::FORMAT_ND)
                    .NodeInputTd(2, ge::DT_INT32, ge::FORMAT_ND, ge::FORMAT_FRACTAL_NZ)
                    .Inputs({&input_tensor_0, &input_tensor_1, &input_tensor_2})
                    .Build();

  auto *context = holder.GetContext<OpCompileContext>();
  ASSERT_NE(context, nullptr);
  ASSERT_NE(context->GetDynamicInputTensor(1U, 0U), nullptr);
  EXPECT_EQ(context->GetDynamicInputTensor(1U, 0U), &input_tensor_1);
  ASSERT_NE(context->GetDynamicInputTensor(1U, 1U), nullptr);
  EXPECT_EQ(context->GetDynamicInputTensor(1U, 1U), &input_tensor_2);
  EXPECT_EQ(context->GetDynamicInputTensor(1U, 1U)->GetStorageFormat(), ge::FORMAT_FRACTAL_NZ);
  EXPECT_EQ(context->GetDynamicInputTensor(1U, 1U)->GetDataType(), ge::DT_INT32);
}

TEST_F(OpCompileContextUT, GetDynamicInputTensorOutOfRange) {
  gert::Tensor input_tensor_0 = {{{8, 32}, {8, 32}},
                                 {ge::FORMAT_NCHW, ge::FORMAT_ND, {}},
                                 ge::DT_FLOAT16};
  gert::Tensor input_tensor_1 = {{{16, 32}, {16, 32}},
                                 {ge::FORMAT_ND, ge::FORMAT_ND, {}},
                                 ge::DT_FLOAT};

  auto holder = KernelRunContextFaker()
                    .IrInstanceNum({1, 1})
                    .KernelIONum(2, 0)
                    .NodeIoNum(2, 0)
                    .NodeInputTd(0, ge::DT_FLOAT16, ge::FORMAT_NCHW, ge::FORMAT_ND)
                    .NodeInputTd(1, ge::DT_FLOAT, ge::FORMAT_ND, ge::FORMAT_ND)
                    .Inputs({&input_tensor_0, &input_tensor_1})
                    .Build();

  auto *context = holder.GetContext<OpCompileContext>();
  ASSERT_NE(context, nullptr);
  EXPECT_EQ(context->GetDynamicInputTensor(1U, 1U), nullptr);
  EXPECT_EQ(context->GetDynamicInputTensor(2U, 0U), nullptr);
}

TEST_F(OpCompileContextUT, GetPlatformInfosSuccess) {
  OpCompileContext context;
  fe::PlatFormInfos platform_info;
  fe::OptionalInfos optional_infos;

  auto ret = context.GetPlatformInfos(platform_info, optional_infos);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
}

TEST_F(OpCompileContextUT, GetPlatformInfosVerifyCoreNum) {
  OpCompileContext context;
  fe::PlatFormInfos platform_info;
  fe::OptionalInfos optional_infos;

  auto ret = context.GetPlatformInfos(platform_info, optional_infos);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
  EXPECT_EQ(platform_info.GetCoreNum(), 8U);
  EXPECT_EQ(platform_info.GetCoreNumWithLock(), 8U);
  EXPECT_EQ(platform_info.GetCoreNumByType("AiCore"), 8U);
}

TEST_F(OpCompileContextUT, GetPlatformInfosVerifyGetPlatformRes) {
  OpCompileContext context;
  fe::PlatFormInfos platform_info;
  fe::OptionalInfos optional_infos;

  auto ret = context.GetPlatformInfos(platform_info, optional_infos);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);

  std::string val;
  EXPECT_TRUE(platform_info.GetPlatformRes("", "Short_SoC_version", val));
  EXPECT_EQ(val, "Ascend910B");
}

TEST_F(OpCompileContextUT, GetPlatformInfosVerifyGetPlatformResWithLock) {
  OpCompileContext context;
  fe::PlatFormInfos platform_info;
  fe::OptionalInfos optional_infos;

  auto ret = context.GetPlatformInfos(platform_info, optional_infos);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);

  std::string val;
  EXPECT_TRUE(platform_info.GetPlatformResWithLock("DtypeMKN", "Default", val));
  EXPECT_EQ(val, "16,16,16");

  EXPECT_TRUE(platform_info.GetPlatformResWithLock("version", "Short_SoC_Version", val));
  EXPECT_EQ(val, "Ascend910B");

  EXPECT_TRUE(platform_info.GetPlatformResWithLock("version", "NpuArch", val));
  EXPECT_EQ(val, "2201");
}

TEST_F(OpCompileContextUT, GetPlatformInfosVerifySocInfo) {
  OpCompileContext context;
  fe::PlatFormInfos platform_info;
  fe::OptionalInfos optional_infos;

  auto ret = context.GetPlatformInfos(platform_info, optional_infos);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);

  std::map<std::string, std::string> soc_info;
  EXPECT_TRUE(platform_info.GetPlatformResWithLock("SoCInfo", soc_info));
  EXPECT_EQ(soc_info.at("ai_core_cnt"), "24");
  EXPECT_EQ(soc_info.at("vector_core_cnt"), "24");
  EXPECT_EQ(soc_info.at("cube_core_cnt"), "24");
}

TEST_F(OpCompileContextUT, GetPlatformInfosVerifyDtypeMKN) {
  OpCompileContext context;
  fe::PlatFormInfos platform_info;
  fe::OptionalInfos optional_infos;

  auto ret = context.GetPlatformInfos(platform_info, optional_infos);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);

  std::map<std::string, std::string> dtype_mkn;
  EXPECT_TRUE(platform_info.GetPlatformResWithLock("DtypeMKN", dtype_mkn));
  EXPECT_EQ(dtype_mkn.at("DT_UINT8"), "16,32,16");
  EXPECT_EQ(dtype_mkn.at("DT_INT8"), "16,32,16");
  EXPECT_EQ(dtype_mkn.at("DT_INT4"), "16,64,16");
}

TEST_F(OpCompileContextUT, GetPlatformInfosVerifyMethods) {
  OpCompileContext context;
  fe::PlatFormInfos platform_info;
  fe::OptionalInfos optional_infos;

  auto ret = context.GetPlatformInfos(platform_info, optional_infos);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);

  EXPECT_TRUE(platform_info.Init());
  EXPECT_EQ(platform_info.SaveToBuffer(), "");
  EXPECT_TRUE(platform_info.LoadFromBuffer(nullptr, 0));
}

TEST_F(OpCompileContextUT, GetPlatformInfosVerifyOptionalInfos) {
  OpCompileContext context;
  fe::PlatFormInfos platform_info;
  fe::OptionalInfos optional_infos;

  auto ret = context.GetPlatformInfos(platform_info, optional_infos);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);

  optional_infos.SetSocVersion("Ascend910B");
  optional_infos.SetAICoreNum(32);
}
}  // namespace gert
