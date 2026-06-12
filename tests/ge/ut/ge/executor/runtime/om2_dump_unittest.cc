/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <vector>

#include "framework/runtime/dump/dump_config.h"
#include "framework/runtime/dump/dump_callback_manager.h"
#include "framework/runtime/dump/model_dump_manager.h"
#include "framework/runtime/dump/overflow_dump_impl.h"
#include "framework/runtime/dump/data_dump_impl.h"
#include "framework/runtime/dump/exception_dump_impl.h"
#include "framework/runtime/dump/profiling_config.h"
#include "framework/runtime/dump/profiling_callback_manager.h"
#include "common/ge_common/ge_log.h"
#include "dump_stub.h"
#include "aprof_pub.h"

using namespace testing;
using namespace ge::dump;

namespace ge {
namespace {

// 测试数据
const char* kValidDataDumpConfig = R"({
    "dump": {
        "dump_path": "/tmp/dump_test",
        "dump_mode": "all",
        "dump_level": "op",
        "dump_step": "1|3-5",
        "dump_data": "tensor",
        "dump_list": [
            {
                "model_name": "test_model",
                "layers": ["layer1", "layer2"]
            }
        ]
    }
})";

const char* kExceptionDumpConfig = R"({
    "dump": {
        "dump_scene": "aic_err_norm_dump",
        "dump_path": "/tmp/exception_dump"
    }
})";

const char* kDebugDumpConfig = R"({
    "dump": {
        "dump_path": "/tmp/debug_dump",
        "dump_debug": "on"
    }
})";

const char* kEmptyDumpConfig = R"({})";

const char* kNoDumpKeyConfig = R"({
    "other_key": "value"
})";

const char* kWatcherSceneConfig = R"({
    "dump": {
        "dump_scene": "watcher",
        "dump_path": "/tmp/watcher_dump"
    }
})";

const char* kLiteExceptionConfig = R"({
    "dump": {
        "dump_scene": "lite_exception",
        "dump_path": "/tmp/lite_exception_dump"
    }
})";

}  // namespace

// DumpConfig 测试类
class DumpConfigTest : public Test {
protected:
    void SetUp() override {
        DumpConfig::Instance().Reset();
    }

    void TearDown() override {
        DumpConfig::Instance().Reset();
    }
};

// 测试 DumpConfig 单例
TEST_F(DumpConfigTest, GetInstanceTest) {
    DumpConfig& instance1 = DumpConfig::Instance();
    DumpConfig& instance2 = DumpConfig::Instance();
    EXPECT_EQ(&instance1, &instance2);
}

// 测试初始状态
TEST_F(DumpConfigTest, InitialStateTest) {
    EXPECT_FALSE(DumpConfig::Instance().IsDataDumpEnabled());
    EXPECT_FALSE(DumpConfig::Instance().IsExceptionDumpEnabled());
    EXPECT_FALSE(DumpConfig::Instance().IsOverflowDumpEnabled());
    EXPECT_TRUE(DumpConfig::Instance().GetDumpPath().empty());
    EXPECT_TRUE(DumpConfig::Instance().GetDumpScene().empty());
}

// 测试解析有效的数据 Dump 配置
TEST_F(DumpConfigTest, ParseValidDataDumpConfigTest) {
    Status ret = DumpConfig::Instance().ParseAndValidate(kValidDataDumpConfig,
                                                          static_cast<int32_t>(strlen(kValidDataDumpConfig)));
    EXPECT_EQ(ret, SUCCESS);
    EXPECT_TRUE(DumpConfig::Instance().IsDataDumpEnabled());
    EXPECT_EQ(DumpConfig::Instance().GetDumpPath(), "/tmp/dump_test");
    EXPECT_EQ(DumpConfig::Instance().GetDumpMode(), "all");
    EXPECT_EQ(DumpConfig::Instance().GetDumpLevel(), "op");
    EXPECT_TRUE(DumpConfig::Instance().NeedDump());
}

// 测试解析异常 Dump 配置
TEST_F(DumpConfigTest, ParseExceptionDumpConfigTest) {
    Status ret = DumpConfig::Instance().ParseAndValidate(kExceptionDumpConfig,
                                                          static_cast<int32_t>(strlen(kExceptionDumpConfig)));
    EXPECT_EQ(ret, SUCCESS);
    EXPECT_TRUE(DumpConfig::Instance().IsExceptionDumpEnabled());
    EXPECT_EQ(DumpConfig::Instance().GetDumpScene(), "aic_err_norm_dump");
    EXPECT_TRUE(DumpConfig::Instance().NeedDump());
}

// 测试解析 Debug Dump 配置（Overflow 检测）
TEST_F(DumpConfigTest, ParseDebugDumpConfigTest) {
    Status ret = DumpConfig::Instance().ParseAndValidate(kDebugDumpConfig,
                                                          static_cast<int32_t>(strlen(kDebugDumpConfig)));
    EXPECT_EQ(ret, SUCCESS);
    EXPECT_TRUE(DumpConfig::Instance().IsOverflowDumpEnabled());
    EXPECT_EQ(DumpConfig::Instance().GetDumpDebug(), "on");
    EXPECT_TRUE(DumpConfig::Instance().NeedDump());
}

// 测试空配置
TEST_F(DumpConfigTest, ParseEmptyConfigTest) {
    Status ret = DumpConfig::Instance().ParseAndValidate(kEmptyDumpConfig,
                                                          static_cast<int32_t>(strlen(kEmptyDumpConfig)));
    EXPECT_EQ(ret, SUCCESS);
    EXPECT_FALSE(DumpConfig::Instance().IsDataDumpEnabled());
    EXPECT_FALSE(DumpConfig::Instance().NeedDump());
}

// 测试没有 dump 键的配置
TEST_F(DumpConfigTest, ParseNoDumpKeyConfigTest) {
    Status ret = DumpConfig::Instance().ParseAndValidate(kNoDumpKeyConfig,
                                                          static_cast<int32_t>(strlen(kNoDumpKeyConfig)));
    EXPECT_EQ(ret, SUCCESS);
    EXPECT_FALSE(DumpConfig::Instance().IsDataDumpEnabled());
    EXPECT_FALSE(DumpConfig::Instance().NeedDump());
}

// 测试 null 输入
TEST_F(DumpConfigTest, ParseNullInputTest) {
    Status ret = DumpConfig::Instance().ParseAndValidate(nullptr, 0);
    EXPECT_EQ(ret, SUCCESS);
    EXPECT_FALSE(DumpConfig::Instance().NeedDump());
}

// 测试 Reset 功能
TEST_F(DumpConfigTest, ResetTest) {
    Status ret = DumpConfig::Instance().ParseAndValidate(kValidDataDumpConfig,
                                                          static_cast<int32_t>(strlen(kValidDataDumpConfig)));
    EXPECT_EQ(ret, SUCCESS);
    EXPECT_TRUE(DumpConfig::Instance().IsDataDumpEnabled());

    DumpConfig::Instance().Reset();

    EXPECT_FALSE(DumpConfig::Instance().IsDataDumpEnabled());
    EXPECT_FALSE(DumpConfig::Instance().IsExceptionDumpEnabled());
    EXPECT_FALSE(DumpConfig::Instance().IsOverflowDumpEnabled());
    EXPECT_TRUE(DumpConfig::Instance().GetDumpPath().empty());
    EXPECT_TRUE(DumpConfig::Instance().GetDumpScene().empty());
    EXPECT_FALSE(DumpConfig::Instance().NeedDump());
}

// 测试 Set/Get 方法
TEST_F(DumpConfigTest, SetGetMethodsTest) {
    DumpConfig::Instance().SetDataDumpEnabled(true);
    EXPECT_TRUE(DumpConfig::Instance().IsDataDumpEnabled());

    DumpConfig::Instance().SetExceptionDumpEnabled(true);
    EXPECT_TRUE(DumpConfig::Instance().IsExceptionDumpEnabled());

    DumpConfig::Instance().SetOverflowDumpEnabled(true);
    EXPECT_TRUE(DumpConfig::Instance().IsOverflowDumpEnabled());

    DumpConfig::Instance().SetDumpPath("/test/path");
    EXPECT_EQ(DumpConfig::Instance().GetDumpPath(), "/test/path");

    DumpConfig::Instance().SetDumpMode("input");
    EXPECT_EQ(DumpConfig::Instance().GetDumpMode(), "input");

    DumpConfig::Instance().SetDumpStep("1-10");
    EXPECT_EQ(DumpConfig::Instance().GetDumpStep(), "1-10");
}

// 测试 Watcher 场景配置
TEST_F(DumpConfigTest, ParseWatcherSceneConfigTest) {
    Status ret = DumpConfig::Instance().ParseAndValidate(kWatcherSceneConfig,
                                                          static_cast<int32_t>(strlen(kWatcherSceneConfig)));
    EXPECT_EQ(ret, SUCCESS);
    EXPECT_TRUE(DumpConfig::Instance().IsExceptionDumpEnabled());
    EXPECT_EQ(DumpConfig::Instance().GetDumpScene(), "watcher");
}

// 测试 Lite Exception 场景配置
TEST_F(DumpConfigTest, ParseLiteExceptionConfigTest) {
    Status ret = DumpConfig::Instance().ParseAndValidate(kLiteExceptionConfig,
                                                          static_cast<int32_t>(strlen(kLiteExceptionConfig)));
    EXPECT_EQ(ret, SUCCESS);
    EXPECT_TRUE(DumpConfig::Instance().IsExceptionDumpEnabled());
    EXPECT_EQ(DumpConfig::Instance().GetDumpScene(), "lite_exception");
}

// 测试 Dump 默认值
TEST_F(DumpConfigTest, DefaultValuesTest) {
    Status ret = DumpConfig::Instance().ParseAndValidate(R"({"dump":{}})", 10);
    EXPECT_EQ(ret, SUCCESS);
    EXPECT_EQ(DumpConfig::Instance().GetDumpMode(), GE_DUMP_MODE_DEFAULT);
    EXPECT_EQ(DumpConfig::Instance().GetDumpStatus(), GE_DUMP_STATUS_DEFAULT);
    EXPECT_EQ(DumpConfig::Instance().GetDumpDebug(), GE_DUMP_DEBUG_DEFAULT);
}

// DumpCallbackManager 测试类
class DumpCallbackManagerTest : public Test {
protected:
    void SetUp() override {
        DumpConfig::Instance().Reset();
    }

    void TearDown() override {
        DumpConfig::Instance().Reset();
    }
};

// 测试单例
TEST_F(DumpCallbackManagerTest, GetInstanceTest) {
    DumpCallbackManager& instance1 = DumpCallbackManager::GetInstance();
    DumpCallbackManager& instance2 = DumpCallbackManager::GetInstance();
    EXPECT_EQ(&instance1, &instance2);
}

// 测试异常 Dump 位判断
TEST_F(DumpCallbackManagerTest, IsEnableExceptionDumpBySwitchTest) {
    // 测试正常异常位
    EXPECT_TRUE(DumpCallbackManager::IsEnableExceptionDumpBySwitch(AIC_ERR_NORM_DUMP_BIT));
    EXPECT_TRUE(DumpCallbackManager::IsEnableExceptionDumpBySwitch(AIC_ERR_BRIEF_DUMP_BIT));

    // 测试组合位
    EXPECT_TRUE(DumpCallbackManager::IsEnableExceptionDumpBySwitch(AIC_ERR_NORM_DUMP_BIT | AIC_ERR_BRIEF_DUMP_BIT));

    // 测试无异常位
    EXPECT_FALSE(DumpCallbackManager::IsEnableExceptionDumpBySwitch(0));
    EXPECT_FALSE(DumpCallbackManager::IsEnableExceptionDumpBySwitch(0x10000));  // 其他位
}

// 测试根据位开关构建异常 JSON
TEST_F(DumpCallbackManagerTest, BuildExceptionDumpJsonBySwitchTest) {
    // NORM 位
    std::string jsonNorm = DumpCallbackManager::BuildExceptionDumpJsonBySwitch(AIC_ERR_NORM_DUMP_BIT);
    EXPECT_FALSE(jsonNorm.empty());
    EXPECT_NE(jsonNorm.find("aic_err_norm_dump"), std::string::npos);

    // BRIEF 位
    std::string jsonBrief = DumpCallbackManager::BuildExceptionDumpJsonBySwitch(AIC_ERR_BRIEF_DUMP_BIT);
    EXPECT_FALSE(jsonBrief.empty());
    EXPECT_NE(jsonBrief.find("aic_err_brief_dump"), std::string::npos);

    // 无效位
    std::string jsonEmpty = DumpCallbackManager::BuildExceptionDumpJsonBySwitch(0x10000);
    EXPECT_TRUE(jsonEmpty.empty());
}

// 测试 EnableDumpCallback - 正常数据 Dump
TEST_F(DumpCallbackManagerTest, EnableDumpCallbackDataDumpTest) {
    int32_t ret = DumpCallbackManager::EnableDumpCallback(0, kValidDataDumpConfig,
                                                           static_cast<int32_t>(strlen(kValidDataDumpConfig)));
    EXPECT_EQ(ret, 0);  // ADUMP_SUCCESS
    EXPECT_TRUE(DumpConfig::Instance().IsDataDumpEnabled());
}

// 测试 EnableDumpCallback - 异常 Dump
TEST_F(DumpCallbackManagerTest, EnableDumpCallbackExceptionTest) {
    int32_t ret = DumpCallbackManager::EnableDumpCallback(0, kExceptionDumpConfig,
                                                           static_cast<int32_t>(strlen(kExceptionDumpConfig)));
    EXPECT_EQ(ret, 0);  // ADUMP_SUCCESS
    EXPECT_TRUE(DumpConfig::Instance().IsExceptionDumpEnabled());
}

// 测试 EnableDumpCallback - Debug Dump (Overflow)
TEST_F(DumpCallbackManagerTest, EnableDumpCallbackDebugTest) {
    int32_t ret = DumpCallbackManager::EnableDumpCallback(0, kDebugDumpConfig,
                                                           static_cast<int32_t>(strlen(kDebugDumpConfig)));
    EXPECT_EQ(ret, 0);  // ADUMP_SUCCESS
    EXPECT_TRUE(DumpConfig::Instance().IsOverflowDumpEnabled());
}

// 测试 EnableDumpCallback - 位开关模式异常 Dump
TEST_F(DumpCallbackManagerTest, EnableDumpCallbackBitSwitchTest) {
    // dumpData 为 null，但异常位被设置
    int32_t ret = DumpCallbackManager::EnableDumpCallback(AIC_ERR_NORM_DUMP_BIT, nullptr, 0);
    EXPECT_EQ(ret, 0);  // ADUMP_SUCCESS
    EXPECT_TRUE(DumpConfig::Instance().IsExceptionDumpEnabled());
    EXPECT_EQ(DumpConfig::Instance().GetDumpScene(), "aic_err_norm_dump");
}

// 测试 DisableDumpCallback
TEST_F(DumpCallbackManagerTest, DisableDumpCallbackTest) {
    // 先启用 Dump
    DumpCallbackManager::EnableDumpCallback(0, kValidDataDumpConfig,
                                            static_cast<int32_t>(strlen(kValidDataDumpConfig)));
    EXPECT_TRUE(DumpConfig::Instance().IsDataDumpEnabled());

    // 再禁用
    int32_t ret = DumpCallbackManager::DisableDumpCallback(0, nullptr, 0);
    EXPECT_EQ(ret, 0);  // ADUMP_SUCCESS
    EXPECT_FALSE(DumpConfig::Instance().IsDataDumpEnabled());
    EXPECT_FALSE(DumpConfig::Instance().IsExceptionDumpEnabled());
    EXPECT_FALSE(DumpConfig::Instance().IsOverflowDumpEnabled());
}

// ModelDumpManager 测试类
class ModelDumpManagerTest : public Test {
protected:
    void SetUp() override {
        DumpConfig::Instance().Reset();
    }

    void TearDown() override {
        DumpConfig::Instance().Reset();
    }
};

// 测试 ModelDumpManager 构造和析构
TEST_F(ModelDumpManagerTest, ConstructorDestructorTest) {
    ModelDumpManager manager(1);
    EXPECT_NO_THROW(manager.Clear());
}

// 测试 GlobalInit
TEST_F(ModelDumpManagerTest, GlobalInitTest) {
    Status ret = ModelDumpManager::GlobalInit();
    EXPECT_EQ(ret, SUCCESS);
}

// 测试 SetModelDumpInfo - 无 Overflow 场景
TEST_F(ModelDumpManagerTest, SetModelDumpInfoWithoutOverflowTest) {
    ModelDumpManager manager(1);
    ModelDumpInfo info{};
    info.model_id = 1;
    info.model_name = "test_model";

    Status ret = manager.SetModelDumpInfo(info);
    EXPECT_EQ(ret, SUCCESS);
}

// 测试 AddOm2TaskInfo - 无 Dump 启用场景
TEST_F(ModelDumpManagerTest, AddOm2TaskInfoNoDumpEnabledTest) {
    ModelDumpManager manager(1);
    Om2TaskInfo info{};
    info.op_name = "test_op";
    info.task_id = 1;
    info.stream_id = 1;

    Status ret = manager.AddOm2TaskInfo(info);
    EXPECT_EQ(ret, SUCCESS);
}

TEST_F(ModelDumpManagerTest, PreprocessOm2TaskInfoNoL0InfoReturnsSuccess) {
    ModelDumpManager manager(1);
    Om2TaskInfo info{};
    info.op_name = "test_op";
    info.stream_id = 1;

    Status ret = manager.PreprocessOm2TaskInfo(info);
    EXPECT_EQ(ret, SUCCESS);
}

TEST_F(ModelDumpManagerTest, ReportDfxTaskPreprocessNullParamReturnsInvalid) {
    ModelDumpManager manager(1);
    Om2TaskInfo info{};

    EXPECT_NE(ReportDfxTaskPreprocess(1U, nullptr, &info, nullptr, 0U), SUCCESS);
    EXPECT_NE(ReportDfxTaskPreprocess(1U, &manager, nullptr, nullptr, 0U), SUCCESS);
}

TEST_F(ModelDumpManagerTest, ReportDfxTaskPreprocessReservedParamReturnsInvalid) {
    ModelDumpManager manager(1);
    Om2TaskInfo info{};
    uint32_t reserved = 0U;

    EXPECT_NE(ReportDfxTaskPreprocess(1U, &manager, &info, &reserved, 0U), SUCCESS);
    EXPECT_NE(ReportDfxTaskPreprocess(1U, &manager, &info, nullptr, 1U), SUCCESS);
}

TEST_F(ModelDumpManagerTest, ReportDfxTaskPostprocessReservedParamReturnsInvalid) {
    ModelDumpManager manager(1);
    Om2TaskInfo info{};
    uint32_t reserved = 0U;

    EXPECT_NE(ReportDfxTaskPostprocess(1U, &manager, &info, &reserved, 0U), SUCCESS);
    EXPECT_NE(ReportDfxTaskPostprocess(1U, &manager, &info, nullptr, 1U), SUCCESS);
}

TEST_F(ModelDumpManagerTest, ReportDfxTaskPostprocessRoutesToAddOm2TaskInfo) {
    ModelDumpManager manager(1);
    Om2TaskInfo info{};
    info.op_name = "test_op";
    info.task_id = 1;
    info.stream_id = 1;

    EXPECT_EQ(ReportDfxTaskPostprocess(1U, &manager, &info, nullptr, 0U), SUCCESS);
}

// 测试 AddOm2TaskInfo - Data Dump 启用场景
TEST_F(ModelDumpManagerTest, AddOm2TaskInfoDataDumpEnabledTest) {
    // 启用 Data Dump
    DumpConfig::Instance().SetDataDumpEnabled(true);

    ModelDumpManager manager(1);
    Om2TaskInfo info{};
    info.op_name = "test_op";
    info.task_id = 1;
    info.stream_id = 1;

    Status ret = manager.AddOm2TaskInfo(info);
    EXPECT_EQ(ret, SUCCESS);
}

// 测试 AddOm2TaskInfo - Exception Dump 启用场景
TEST_F(ModelDumpManagerTest, AddOm2TaskInfoExceptionDumpEnabledTest) {
    // 启用 Exception Dump
    DumpConfig::Instance().SetExceptionDumpEnabled(true);

    ModelDumpManager manager(1);
    Om2TaskInfo info{};
    info.op_name = "test_op";
    info.task_id = 1;
    info.stream_id = 1;

    Status ret = manager.AddOm2TaskInfo(info);
    EXPECT_EQ(ret, SUCCESS);
}

TEST_F(ModelDumpManagerTest, IsDataDumpEnabledTest) {
    ModelDumpManager manager(1);
    uint8_t is_data_dump = 1U;

    Status ret = manager.IsDataDumpEnabled("test_op", &is_data_dump);
    EXPECT_EQ(ret, SUCCESS);
    EXPECT_EQ(is_data_dump, 0U);

    DumpConfig::Instance().SetDataDumpEnabled(true);
    ret = manager.IsDataDumpEnabled("test_op", &is_data_dump);
    EXPECT_EQ(ret, SUCCESS);
    EXPECT_EQ(is_data_dump, 1U);
}

// 测试 DispatchDumpInfo
TEST_F(ModelDumpManagerTest, DispatchDumpInfoTest) {
    ModelDumpManager manager(1);
    Status ret = manager.DispatchDumpInfo();
    EXPECT_EQ(ret, SUCCESS);
}

// 测试 DispatchDumpInfo - Data Dump 启用场景
TEST_F(ModelDumpManagerTest, DispatchDumpInfoDataDumpEnabledTest) {
    DumpConfig::Instance().SetDataDumpEnabled(true);

    ModelDumpManager manager(1);
    ModelDumpInfo modelInfo{};
    modelInfo.model_id = 1;
    manager.SetModelDumpInfo(modelInfo);

    Om2TaskInfo taskInfo{};
    taskInfo.op_name = "test_op";
    taskInfo.task_id = 1;
    taskInfo.stream_id = 1;
    manager.AddOm2TaskInfo(taskInfo);

    Status ret = manager.DispatchDumpInfo();
    EXPECT_EQ(ret, SUCCESS);
}

// ExceptionDumpImpl 测试
TEST(ExceptionDumpImplTest, BasicTest) {
    ExceptionDumpImpl impl;
    Om2TaskInfo info{};
    info.op_name = "test_op";
    info.task_id = 1;
    info.stream_id = 1;

    Status ret = impl.SaveOpInfo(info);
    EXPECT_EQ(ret, SUCCESS);

    OpDescInfo opInfo{};
    EXPECT_TRUE(impl.GetOpDescInfo(OpDescInfoId(1, 1), opInfo));
    EXPECT_EQ(std::string(opInfo.op_name), "test_op");
}

TEST(ExceptionDumpImplTest, SaveOpInfoLiteExceptionDoesNotReportL1Info) {
    DumpConfig::Instance().Reset();
    DumpStub::GetInstance().ClearOpInfos();
    ASSERT_EQ(DumpConfig::Instance().ParseAndValidate(kLiteExceptionConfig,
                                                      static_cast<int32_t>(strlen(kLiteExceptionConfig))), SUCCESS);

    ExceptionDumpImpl impl;
    Om2TaskInfo info{};
    info.op_name = "test_op";
    info.op_type = "Add";
    info.task_id = 1U;
    info.stream_id = 1U;

    EXPECT_EQ(impl.SaveOpInfo(info), SUCCESS);
    EXPECT_TRUE(DumpStub::GetInstance().GetOpInfos().empty());
}

TEST(ExceptionDumpImplTest, SaveOpInfoAicErrNormReportsL1Info) {
    DumpConfig::Instance().Reset();
    DumpStub::GetInstance().ClearOpInfos();
    ASSERT_EQ(DumpConfig::Instance().ParseAndValidate(kExceptionDumpConfig,
                                                      static_cast<int32_t>(strlen(kExceptionDumpConfig))), SUCCESS);

    ExceptionDumpImpl impl;
    Om2TaskInfo info{};
    info.op_name = "test_op";
    info.op_type = "Add";
    info.task_id = 1U;
    info.stream_id = 1U;

    EXPECT_EQ(impl.SaveOpInfo(info), SUCCESS);
    EXPECT_EQ(DumpStub::GetInstance().GetOpInfos().size(), 1U);
}

TEST(ExceptionDumpImplTest, ReportL0ExceptionDumpInfoNoInfoReturnsSuccess) {
    ExceptionDumpImpl impl;
    Om2TaskInfo info{};
    info.op_name = "test_op";

    EXPECT_EQ(impl.ReportL0ExceptionDumpInfo(info), SUCCESS);
}

TEST(ExceptionDumpImplTest, ReportL0ExceptionDumpInfoArgNumWithoutArgsReturnsInvalid) {
    ExceptionDumpImpl impl;
    Om2L0TaskRawInfo l0Info{};
    l0Info.version = 1U;
    l0Info.arg_num = 1U;
    l0Info.args = nullptr;

    Om2TaskInfo info{};
    info.op_name = "test_op";
    info.l0_exception_dump_info = &l0Info;

    EXPECT_EQ(impl.ReportL0ExceptionDumpInfo(info), PARAM_INVALID);
}

TEST(ExceptionDumpImplTest, ReportL0ExceptionDumpInfoLiteDumpDisabledReturnsSuccess) {
    DumpConfig::Instance().Reset();
    DumpStub::GetInstance().Clear();

    ExceptionDumpImpl impl;
    Om2L0ArgSlotInfo slot{};
    slot.kind = OM2_L0_ARG_INPUT;
    slot.value = 32U;
    Om2L0TaskRawInfo l0Info{};
    l0Info.version = 1U;
    l0Info.arg_num = 1U;
    l0Info.args = &slot;

    Om2TaskInfo info{};
    info.op_name = "test_op";
    info.l0_exception_dump_info = &l0Info;

    EXPECT_EQ(impl.ReportL0ExceptionDumpInfo(info), SUCCESS);
    EXPECT_TRUE(DumpStub::GetInstance().GetUnits().empty());
}

TEST(ExceptionDumpImplTest, ReportL0ExceptionDumpInfoConvertsSlotKinds) {
    DumpStub::GetInstance().Clear();

    Om2L0ArgSlotInfo slots[12]{};
    slots[0].kind = OM2_L0_ARG_INPUT;
    slots[0].args_offset = 0U;
    slots[1].kind = OM2_L0_ARG_OUTPUT;
    slots[1].args_offset = 8U;
    slots[2].kind = OM2_L0_ARG_WORKSPACE;
    slots[2].related_index = 0U;
    slots[3].kind = OM2_L0_ARG_SHAPE_INFO;
    slots[3].value = 4U;
    slots[4].kind = OM2_L0_ARG_TILING;
    slots[4].value = 256U;
    slots[5].kind = OM2_L0_ARG_LEVEL1_DESC;
    slots[6].kind = OM2_L0_ARG_PLACEHOLDER;
    slots[7].kind = OM2_L0_ARG_CUSTOM_VALUE;
    slots[7].value = 9U;
    slots[8].kind = OM2_L0_ARG_FFTS_ADDR;
    slots[9].kind = OM2_L0_ARG_EVENT_ADDR;
    slots[10].kind = OM2_L0_ARG_OVERFLOW_ADDR;
    slots[11].kind = OM2_L0_ARG_EMPTY_ADDR;

    Om2L0TaskRawInfo l0Info{};
    l0Info.version = 1U;
    l0Info.need_assert_or_printf = 1U;
    l0Info.arg_num = 12U;
    l0Info.args = slots;

    Om2Tensor inputTensor{};
    inputTensor.size = 32U;
    Om2Tensor outputTensor{};
    outputTensor.size = 64U;
    Om2TaskIoEntry inputEntry{&inputTensor, 0U};
    Om2TaskIoEntry outputEntry{&outputTensor, 8U};
    uint64_t workspaceSize = 128U;

    Om2TaskInfo info{};
    info.op_name = "test_op";
    info.input_num = 1U;
    info.inputs = &inputEntry;
    info.output_num = 1U;
    info.outputs = &outputEntry;
    info.workspace_num = 1U;
    info.workspace_sizes = &workspaceSize;
    info.l0_exception_dump_info = &l0Info;

    ExceptionDumpImpl impl;
    EXPECT_EQ(impl.ReportL0ExceptionDumpInfo(info), SUCCESS);

    ASSERT_EQ(DumpStub::GetInstance().GetUnits().size(), 14U);
    const auto &unit = DumpStub::GetInstance().GetUnits().back();
    EXPECT_EQ(unit[0], 14U);
    EXPECT_EQ(unit[1], 12U);
    EXPECT_EQ(unit[2], 32U);
    EXPECT_EQ(unit[3], 64U);
    EXPECT_EQ(unit[4], (4UL << 56U) | 128U);
    EXPECT_EQ(unit[5], (3UL << 56U) | 4U);
    EXPECT_EQ(unit[6], (4UL << 56U) | 256U);
    for (size_t i = 7U; i < 14U; ++i) {
        EXPECT_EQ(unit[i], 1UL << 56U);
    }
}

TEST(ExceptionDumpImplTest, ReportL0ExceptionDumpInfoUnsupportedKindReturnsInvalid) {
    Om2L0ArgSlotInfo slot{};
    slot.kind = 999U;
    Om2L0TaskRawInfo l0Info{};
    l0Info.version = 1U;
    l0Info.need_assert_or_printf = 1U;
    l0Info.arg_num = 1U;
    l0Info.args = &slot;

    Om2TaskInfo info{};
    info.op_name = "test_op";
    info.l0_exception_dump_info = &l0Info;

    ExceptionDumpImpl impl;
    EXPECT_EQ(impl.ReportL0ExceptionDumpInfo(info), PARAM_INVALID);
}

TEST(ExceptionDumpImplTest, GetOpDescInfoNotFoundTest) {
    ExceptionDumpImpl impl;
    OpDescInfo opInfo{};
    EXPECT_FALSE(impl.GetOpDescInfo(OpDescInfoId(999, 999), opInfo));
}

TEST(ExceptionDumpImplTest, ClearTest) {
    ExceptionDumpImpl impl;
    Om2TaskInfo info{};
    info.op_name = "test_op";
    info.task_id = 1;
    info.stream_id = 1;
    impl.SaveOpInfo(info);

    impl.Clear();

    OpDescInfo opInfo{};
    EXPECT_FALSE(impl.GetOpDescInfo(OpDescInfoId(1, 1), opInfo));
}

// DataDumpImpl 测试
TEST(DataDumpImplTest, ConstructorDestructorTest) {
    DataDumpImpl impl;
    EXPECT_NO_THROW(impl.Clear());
}

TEST(DataDumpImplTest, SaveTaskTest) {
    DataDumpImpl impl;
    Om2TaskInfo info{};
    info.op_name = "test_op";
    info.task_id = 1;
    info.stream_id = 1;

    Status ret = impl.SaveTask(info, ModelTaskType::TASK_TYPE_KERNEL, nullptr, false);
    EXPECT_EQ(ret, SUCCESS);
}

// OverflowDumpImpl 测试
TEST(OverflowDumpImplTest, ConstructorDestructorTest) {
    OverflowDumpImpl impl;
    EXPECT_NO_THROW(impl.Clear());
}

TEST(OverflowDumpImplTest, IsOpDebugEnabledDefaultTest) {
    OverflowDumpImpl impl;
    EXPECT_FALSE(impl.IsOpDebugEnabled());
}

// ProfilingConfig 测试类
class ProfilingConfigTest : public Test {
 protected:
  void SetUp() override {
    ProfilingConfig::Instance().Disable();
  }

  void TearDown() override {
    ProfilingConfig::Instance().Disable();
  }
};

TEST_F(ProfilingConfigTest, ProfilingConfigDefaultDisabled) {
  EXPECT_FALSE(ProfilingConfig::Instance().IsEnabled());
  EXPECT_FALSE(ProfilingConfig::Instance().IsTaskTimeEnabled());
  EXPECT_FALSE(ProfilingConfig::Instance().IsDeviceEnabled());
}

TEST_F(ProfilingConfigTest, ProfilingConfigEnableAndDisable) {
  ProfilingOptions options;
  options.task_time_enabled = true;
  options.device_enabled = true;
  options.module = 0x3U;
  options.cache_flag = 1U;
  options.device_list = {0U, 1U};
  options.config_params.emplace("devNums", "2");
  options.config_params.emplace("devIdList", "0,1");

  EXPECT_EQ(ProfilingConfig::Instance().Enable(options), SUCCESS);
  EXPECT_TRUE(ProfilingConfig::Instance().IsEnabled());
  EXPECT_TRUE(ProfilingConfig::Instance().IsTaskTimeEnabled());
  EXPECT_TRUE(ProfilingConfig::Instance().IsDeviceEnabled());

  const auto saved_options = ProfilingConfig::Instance().GetOptions();
  EXPECT_EQ(saved_options.module, 0x3U);
  EXPECT_EQ(saved_options.cache_flag, 1U);
  ASSERT_EQ(saved_options.device_list.size(), 2U);
  EXPECT_EQ(saved_options.device_list[0U], 0U);
  EXPECT_EQ(saved_options.device_list[1U], 1U);
  EXPECT_EQ(saved_options.config_params.at("devNums"), "2");
  EXPECT_EQ(saved_options.config_params.at("devIdList"), "0,1");

  ProfilingConfig::Instance().Disable();
  EXPECT_FALSE(ProfilingConfig::Instance().IsEnabled());
}

TEST_F(ProfilingConfigTest, StartEnablesConfig) {
  ProfilingConfig::Instance().Disable();

  MsprofCommandHandle cmd = {};
  cmd.type = 1U;
  cmd.profSwitch = PROF_TASK_TIME_MASK | PROF_TASK_TIME_L1_MASK;
  cmd.cacheFlag = 7U;
  cmd.devNums = 2U;
  cmd.devIdList[0U] = 0U;
  cmd.devIdList[1U] = 1U;

  rtError_t ret = ProfilingCallbackManager::ProfilingCtrlCallback(RT_PROF_CTRL_SWITCH, &cmd, sizeof(cmd));
  EXPECT_EQ(ret, RT_ERROR_NONE);

  EXPECT_TRUE(ProfilingConfig::Instance().IsEnabled());
  EXPECT_TRUE(ProfilingConfig::Instance().IsTaskTimeEnabled());
  EXPECT_TRUE(ProfilingConfig::Instance().IsDeviceEnabled());

  const auto options = ProfilingConfig::Instance().GetOptions();
  EXPECT_EQ(options.module, PROF_TASK_TIME_MASK | PROF_TASK_TIME_L1_MASK);
  EXPECT_EQ(options.cache_flag, 7U);
  ASSERT_EQ(options.device_list.size(), 2U);
  EXPECT_EQ(options.device_list[0U], 0U);
  EXPECT_EQ(options.device_list[1U], 1U);
  EXPECT_EQ(options.config_params.at("devNums"), "2");
  EXPECT_EQ(options.config_params.at("devIdList"), "0,1");
}

TEST_F(ProfilingConfigTest, StopDisablesConfig) {
  // First enable profiling
  ProfilingOptions options;
  options.task_time_enabled = true;
  options.device_enabled = true;
  ASSERT_EQ(ProfilingConfig::Instance().Enable(options), SUCCESS);
  EXPECT_TRUE(ProfilingConfig::Instance().IsEnabled());

  // Then send stop command
  MsprofCommandHandle cmd = {};
  cmd.type = 2U;

  rtError_t ret = ProfilingCallbackManager::ProfilingCtrlCallback(RT_PROF_CTRL_SWITCH, &cmd, sizeof(cmd));
  EXPECT_EQ(ret, RT_ERROR_NONE);
  EXPECT_FALSE(ProfilingConfig::Instance().IsEnabled());
}

TEST_F(ProfilingConfigTest, IgnoreUnsupportedCtrlType) {
  ProfilingOptions options;
  options.task_time_enabled = true;
  ASSERT_EQ(ProfilingConfig::Instance().Enable(options), SUCCESS);
  EXPECT_TRUE(ProfilingConfig::Instance().IsEnabled());

  rtError_t ret = ProfilingCallbackManager::ProfilingCtrlCallback(0U, nullptr, 0U);
  EXPECT_EQ(ret, RT_ERROR_NONE);
  EXPECT_TRUE(ProfilingConfig::Instance().IsEnabled());

  MsprofCommandHandle cmd = {};
  cmd.type = 2U;
  ret = ProfilingCallbackManager::ProfilingCtrlCallback(3U, &cmd, sizeof(cmd));
  EXPECT_EQ(ret, RT_ERROR_NONE);
  EXPECT_TRUE(ProfilingConfig::Instance().IsEnabled());
}

TEST_F(ProfilingConfigTest, RejectsInvalidInput) {
  // First enable profiling
  ProfilingOptions options;
  options.task_time_enabled = true;
  ASSERT_EQ(ProfilingConfig::Instance().Enable(options), SUCCESS);
  EXPECT_TRUE(ProfilingConfig::Instance().IsEnabled());

  // Test null data
  rtError_t ret = ProfilingCallbackManager::ProfilingCtrlCallback(RT_PROF_CTRL_SWITCH, nullptr, 0U);
  EXPECT_EQ(ret, static_cast<rtError_t>(-1));
  EXPECT_TRUE(ProfilingConfig::Instance().IsEnabled());

  // Test zero data_len
  MsprofCommandHandle cmd = {};
  ret = ProfilingCallbackManager::ProfilingCtrlCallback(RT_PROF_CTRL_SWITCH, &cmd, 0U);
  EXPECT_EQ(ret, static_cast<rtError_t>(-1));
  EXPECT_TRUE(ProfilingConfig::Instance().IsEnabled());
}

}  // namespace ge
