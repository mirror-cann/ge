/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "program_generator.h"
#include "common/om2/codegen/ast/ast_nodes.h"
#include "common/om2/codegen/emitter/cpp_emitter.h"
#include "common/om2/codegen/file_code_generator/args_manager_file_code_generator.h"
#include "common/om2/codegen/file_code_generator/interface_file_code_generator.h"
#include "common/om2/codegen/file_code_generator/kernel_reg_file_code_generator.h"
#include "common/om2/codegen/file_code_generator/load_and_run_file_code_generator.h"
#include "common/om2/codegen/file_code_generator/resources_file_code_generator.h"
#include "common/om2/codegen/om2_codegen_utils.h"
#include "framework/common/debug/ge_log.h"

namespace ge {
namespace {
Status EmitFile(const GeneratedFileIndex file_index, const AstNode *unit, Om2CodePrinter &code_printer) {
  GE_ASSERT_TRUE(unit != nullptr, "[OM2] Program %zu AST is null.", static_cast<size_t>(file_index));
  CppEmitter emitter;
  std::string code_content;
  GE_ASSERT_SUCCESS(unit->Accept(emitter, code_content), "[OM2] Program %zu code generation failed.",
                    static_cast<size_t>(file_index));
  // atc 通过 /proc/self/fd/N 传递源码给编译器，导致 __FILE__ 展开为 fd 路径。
  // 插入 #line 指令强制 __FILE__ 为正确的文件名。
  const std::string &file_name = code_printer.GetFileName(file_index);
  constexpr char kCppExt[] = ".cpp";
  if (file_name.size() >= sizeof(kCppExt) - 1 &&
      file_name.compare(file_name.size() - (sizeof(kCppExt) - 1), sizeof(kCppExt) - 1, kCppExt) == 0) {
    code_content = "#line 1 \"" + file_name + "\"\n" + code_content;
  }
  code_printer.AddContent(file_index, code_content);
  return SUCCESS;
}
}  // namespace

Status ProgramGenerator::GenerateProgram(Om2CodePrinter &code_printer) {
  GE_ASSERT_SUCCESS(GenerateKernelRegSource(code_printer));
  GE_ASSERT_SUCCESS(GenerateInterfaceHeader(code_printer));
  GE_ASSERT_SUCCESS(GenerateResourcesSource(code_printer));
  GE_ASSERT_SUCCESS(GenerateLoadAndRunSource(code_printer));
  GE_ASSERT_SUCCESS(GenerateArgsManagerSource(code_printer));
  GE_ASSERT_SUCCESS(GenerateMakeFile(code_printer));
  return SUCCESS;
}

std::vector<DeclNode *> ProgramGenerator::BuildInterfaceHeaderIncludes() const {
  return {
      ast_.Include("iostream", IncludeDecl::Kind::kAngle),
      ast_.Include("cstddef", IncludeDecl::Kind::kAngle),
      ast_.Include("ctime", IncludeDecl::Kind::kAngle),
      ast_.Include("chrono", IncludeDecl::Kind::kAngle),
      ast_.Include("fstream", IncludeDecl::Kind::kAngle),
      ast_.Include("iomanip", IncludeDecl::Kind::kAngle),
      ast_.Include("sstream", IncludeDecl::Kind::kAngle),
      ast_.Include("vector", IncludeDecl::Kind::kAngle),
      ast_.Include("map", IncludeDecl::Kind::kAngle),
      ast_.Include("unordered_map", IncludeDecl::Kind::kAngle),
      ast_.Include("functional", IncludeDecl::Kind::kAngle),
      ast_.Include("cstdint", IncludeDecl::Kind::kAngle),
      ast_.Include("type_traits", IncludeDecl::Kind::kAngle),
      ast_.Include("array", IncludeDecl::Kind::kAngle),
      ast_.Include("securec.h"),
      ast_.Include("acl/acl.h"),
      ast_.Include("acl/acl_base.h"),
      ast_.Include("exe_graph/runtime/runtime_tensor.h"),
      ast_.Include("rt_external.h"),
      ast_.Include("dlog_pub.h", IncludeDecl::Kind::kQuote),
      ast_.Include("sys/syscall.h", IncludeDecl::Kind::kAngle),
      ast_.Include("unistd.h", IncludeDecl::Kind::kAngle),
      ast_.Include("cinttypes", IncludeDecl::Kind::kAngle),
  };
}

Status ProgramGenerator::GenerateInterfaceHeader(Om2CodePrinter &code_printer) {
  InterfaceFileCodeGenerator interface_handler(ast_);
  auto external_api_decls = interface_handler.BuildExternalApiDecls();
  auto rt_forward_decls = interface_handler.BuildRtForwardDecls();
  auto file_items = BuildInterfaceHeaderIncludes();
  file_items.push_back(ast_.Space());
  file_items.push_back(ast_.StablePart(StablePartId::kOm2LogMacros, StablePartPlacement::kTranslationUnit));
  file_items.push_back(ast_.StablePart(StablePartId::kInterfaceMacros));
  file_items.push_back(ast_.StablePart(StablePartId::kInterfacePointerHelpers));
  file_items.push_back(ast_.StablePart(StablePartId::kInterfaceDumpApis));
  file_items.insert(file_items.end(), rt_forward_decls.begin(), rt_forward_decls.end());
  file_items.push_back(ast_.Namespace(
      "om2", {
                 ast_.Field("constexpr int32_t", "INPUT_NUM", static_cast<int>(codegen_model_.model_io.input_count)),
                 ast_.Field("constexpr int32_t", "OUTPUT_NUM", static_cast<int>(codegen_model_.model_io.output_count)),
                 interface_handler.BuildOm2ModelHandleAlias(),
                 interface_handler.BuildBinDataInfoStruct(),
                 interface_handler.BuildAicpuParamHeadStruct(),
                 interface_handler.BuildAicpuSessionInfoStruct(),
                 interface_handler.BuildArgsInfoStruct(),
                 interface_handler.BuildTfAiCpuExInfoStruct(),
                 ast_.StablePart(StablePartId::kScopeGuard, StablePartPlacement::kNamespace),
                 interface_handler.BuildOm2ArgsTableClass(),
                 ast_.StablePart(StablePartId::kOpDefStructs, StablePartPlacement::kNamespace),
                 interface_handler.BuildOm2ModelClass(codegen_model_),
             }));
  file_items.push_back(ast_.ExternBlock("C", external_api_decls));
  auto *translation_unit = ast_.File(file_items);
  GE_ASSERT_SUCCESS(EmitFile(GeneratedFileIndex::kInterfaceHeaderFile, translation_unit, code_printer));
  return SUCCESS;
}

Status ProgramGenerator::GenerateResourcesSource(Om2CodePrinter &code_printer) {
  ResourcesFileCodeGenerator resources_handler(ast_);
  std::vector<DeclNode *> resources_items = {
      resources_handler.BuildOm2ModelConstructor(codegen_model_),
      resources_handler.BuildOm2ModelDestructor(),
      resources_handler.BuildInitResourcesMethod(codegen_model_, task_code_builder_list_),
      resources_handler.BuildReleaseResourcesMethod(codegen_model_),
  };
  if (codegen_model_.runtime.has_label_switch) {
    resources_items.push_back(ast_.StablePart(StablePartId::kCreateLabelListForLabelSwitch));
  }
  if (codegen_model_.runtime.has_label_goto) {
    resources_items.push_back(ast_.StablePart(StablePartId::kCreateLabelListForLabelGotoEx));
  }
  auto *translation_unit = ast_.File({
      ast_.Include(codegen_model_.model_name + "_interface.h"),
      ast_.Space(),
      ast_.Namespace("om2", resources_items),
  });
  GE_ASSERT_SUCCESS(EmitFile(GeneratedFileIndex::kResourcesFile, translation_unit, code_printer));
  GELOGD("[OM2] Interface header file code is generated.");
  return SUCCESS;
}

Status ProgramGenerator::GenerateArgsManagerSource(Om2CodePrinter &code_printer) {
  ArgsManagerFileCodeGenerator args_manager_handler(ast_);
  auto *translation_unit = ast_.File({
      ast_.Include(codegen_model_.model_name + "_interface.h"),
      ast_.Space(),
      ast_.Namespace("om2",
                     {
                         args_manager_handler.BuildInitMethod(codegen_model_),
                         args_manager_handler.BuildDestructor(),
                         args_manager_handler.BuildGetArgsInfoMethod(),
                         args_manager_handler.BuildGetDevArgAddrMethod(),
                         args_manager_handler.BuildGetHostArgAddrMethod(),
                         args_manager_handler.BuildUpdateHostArgsMethod(),
                         args_manager_handler.BuildCopyArgsToDeviceMethod(),
                     }),
  });
  GE_ASSERT_SUCCESS(EmitFile(GeneratedFileIndex::kArgsManagerFile, translation_unit, code_printer));
  GELOGD("[OM2] Args Manager source file code is generated.");
  return SUCCESS;
}

Status ProgramGenerator::GenerateKernelRegSource(Om2CodePrinter &code_printer) {
  KernelRegFileCodeGenerator kernel_reg_handler(ast_);
  std::vector<DeclNode *> anonymous_items = {
      ast_.Field("constexpr uint32_t", "kMaxJsonFileLen", ast_.UInt(512)),
      kernel_reg_handler.BuildBinaryBufferStruct(),
      kernel_reg_handler.BuildAicoreRegisterInfoStruct(),
      kernel_reg_handler.BuildAicpuRegisterInfoStruct(),
      kernel_reg_handler.BuildCustAicpuRegisterInfoStruct(),
      ast_.StablePart(StablePartId::kReadBinaryFileToBuffer, StablePartPlacement::kNamespace),
      ast_.StablePart(StablePartId::kGenerateJsonFile, StablePartPlacement::kNamespace),
      kernel_reg_handler.BuildAssembleAicpuLoadOptions(),
      kernel_reg_handler.BuildRegisterAicoreKernel(),
      kernel_reg_handler.BuildRegisterAicpuKernel(),
      kernel_reg_handler.BuildRegisterCustAicpuKernel(),
  };
  auto *translation_unit = ast_.File({
      ast_.Include(codegen_model_.model_name + "_interface.h"),
      ast_.Namespace("om2",
                     {
                         ast_.Namespace("", anonymous_items),
                         kernel_reg_handler.BuildRegisterKernels(codegen_model_),
                     }),
  });
  GE_ASSERT_SUCCESS(EmitFile(GeneratedFileIndex::kKernelRegistryFile, translation_unit, code_printer));
  GELOGD("[OM2] Kernel Reg source file code is generated.");
  return SUCCESS;
}

Status ProgramGenerator::GenerateLoadAndRunSource(Om2CodePrinter &code_printer) {
  LoadAndRunFileCodeGenerator load_and_run_handler(ast_);
  auto anonymous_items = load_and_run_handler.BuildAnonymousNamespaceItems(codegen_model_, task_code_builder_list_);
  (void)anonymous_items.insert(anonymous_items.begin(),
                               ast_.StablePart(StablePartId::kLoadAndRunDumpHelpers, StablePartPlacement::kNamespace));
  anonymous_items.push_back(load_and_run_handler.BuildOpDefTable(codegen_model_, task_code_builder_list_));
  auto *translation_unit = ast_.File({
      ast_.Include(codegen_model_.model_name + "_interface.h"),
      ast_.Space(),
      ast_.Namespace("om2",
                     {
                         ast_.Namespace("", anonymous_items),
                         load_and_run_handler.BuildGetRtModelHandleMethod(),
                         load_and_run_handler.BuildLoadMethod(codegen_model_, task_code_builder_list_),
                         load_and_run_handler.BuildRunAsyncMethod(codegen_model_),
                         load_and_run_handler.BuildRunMethod(codegen_model_),
                     }),
      ast_.StablePart(StablePartId::kLoadAndRunExternalApis),
  });
  GE_ASSERT_SUCCESS(EmitFile(GeneratedFileIndex::kLoadingAndRunningFile, translation_unit, code_printer));
  GELOGD("[OM2] Load and run source file code is generated.");
  return SUCCESS;
}

Status ProgramGenerator::GenerateMakeFile(Om2CodePrinter &code_printer) {
  const std::string model_name = codegen_model_.model_name;
  const std::string lib_name = model_name + "_om2";
  std::string cmakelists_content = R"(CANN_ROOT ?= $(ASCEND_HOME_PATH)
USE_STUB_LIB ?= 1
MAKEFLAGS += -j$(shell nproc)

ifeq ($(origin CXX),default)
CXX := c++
endif
CXX ?= c++

TARGET := lib)" + lib_name + R"(.so
SRC_FILES := )" + model_name + R"(_resources.cpp )" +
                                   model_name + R"(_kernel_reg.cpp )" + model_name + R"(_load_and_run.cpp )" +
                                   model_name + R"(_args_manager.cpp

ifndef CPPFLAGS
CPPFLAGS := \
  -I$(CANN_ROOT)/include \
  -I$(CANN_ROOT)/pkg_inc \
  -I$(CANN_ROOT)/pkg_inc/base \
  -I$(CANN_ROOT)/pkg_inc/runtime \
  -I$(CANN_ROOT)/pkg_inc/profiling \
  -I$(CURDIR)/include
endif

ifndef CXXFLAGS
CXXFLAGS := -std=c++17 -O2 -fPIC
endif

ifeq ($(USE_STUB_LIB),1)
LIB_PATH ?= $(CANN_ROOT)/devlib
else
LIB_PATH ?= $(CANN_ROOT)/lib64
endif

ifndef LDFLAGS
LDFLAGS := -shared -L$(LIB_PATH) -Wl,--no-as-needed
endif
ifndef LDLIBS
LDLIBS := -lacl_rt -Wl,--as-needed
endif

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC_FILES)
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

  clean:
	@rm -f $(TARGET)
)";
  code_printer.AddContent(GeneratedFileIndex::kCMakeListsFile, cmakelists_content + "\n");
  GELOGD("[OM2] Makefile code is generated.");
  return SUCCESS;
}
}  // namespace ge
