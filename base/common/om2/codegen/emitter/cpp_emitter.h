/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef BASE_COMMON_OM2_CODEGEN_EMITTER_CPP_EMITTER_H
#define BASE_COMMON_OM2_CODEGEN_EMITTER_CPP_EMITTER_H

#include <string>
#include <utility>

#include "common/om2/codegen/ast/ast_context.h"
#include "code_emitter.h"

namespace ge {
class DeclNode;

class CppEmitter final : public CodeEmitter {
 public:
  CppEmitter() : indent_unit_("  ") {}
  explicit CppEmitter(std::string indent_unit) : indent_unit_(std::move(indent_unit)) {}

  Status Emit(const ParamDecl &node, std::string &output) override;
  Status Emit(const TranslationUnit &node, std::string &output) override;
  Status Emit(const StablePartDecl &node, std::string &output) override;
  Status Emit(const IncludeDecl &node, std::string &output) override;
  Status Emit(const SpaceDecl &node, std::string &output) override;
  Status Emit(const TypeAliasDecl &node, std::string &output) override;
  Status Emit(const NamespaceDecl &node, std::string &output) override;
  Status Emit(const ExternBlockDecl &node, std::string &output) override;
  Status Emit(const ClassDecl &node, std::string &output) override;
  Status Emit(const StructDecl &node, std::string &output) override;
  Status Emit(const AccessSectionDecl &node, std::string &output) override;
  Status Emit(const FieldDecl &node, std::string &output) override;
  Status Emit(const MethodDecl &node, std::string &output) override;
  Status Emit(const FunctionDecl &node, std::string &output) override;
  Status Emit(const FunctionDef &node, std::string &output) override;
  Status Emit(const MethodDef &node, std::string &output) override;
  Status Emit(const IdentifierExpr &node, std::string &output) override;
  Status Emit(const LiteralExpr &node, std::string &output) override;
  Status Emit(const AssignExpr &node, std::string &output) override;
  Status Emit(const BinaryExpr &node, std::string &output) override;
  Status Emit(const UnaryExpr &node, std::string &output) override;
  Status Emit(const CallExpr &node, std::string &output) override;
  Status Emit(const MakeUniqueArrayExpr &node, std::string &output) override;
  Status Emit(const ToStrExpr &node, std::string &output) override;
  Status Emit(const MemcpyExpr &node, std::string &output) override;
  Status Emit(const SizeofExpr &node, std::string &output) override;
  Status Emit(const RemoveFileExpr &node, std::string &output) override;
  Status Emit(const IgnoreOutputExpr &node, std::string &output) override;
  Status Emit(const ContainerMethodExpr &node, std::string &output) override;
  Status Emit(const AddrOfExpr &node, std::string &output) override;
  Status Emit(const SubscriptExpr &node, std::string &output) override;
  Status Emit(const MemberExpr &node, std::string &output) override;
  Status Emit(const CppArrowMemberExpr &node, std::string &output) override;
  Status Emit(const CppCastExpr &node, std::string &output) override;
  Status Emit(const LambdaExpr &node, std::string &output) override;
  Status Emit(const InitListExpr &node, std::string &output) override;
  Status Emit(const CommentStmt &node, std::string &output) override;
  Status Emit(const BlankLineStmt &node, std::string &output) override;
  Status Emit(const VarDeclStmt &node, std::string &output) override;
  Status Emit(const ExprStmt &node, std::string &output) override;
  Status Emit(const ReturnStmt &node, std::string &output) override;
  Status Emit(const BlockStmt &node, std::string &output) override;
  Status Emit(const IfStmt &node, std::string &output) override;
  Status Emit(const ForStmt &node, std::string &output) override;
  Status Emit(const RangeForStmt &node, std::string &output) override;

 private:
  void AppendIndent(std::string &output) const;
  void AppendIndentAt(size_t level, std::string &output) const;
  Status EmitParamList(const ArrayRef<ParamDecl *> &params, std::string &output);
  Status EmitDeclBlock(const ArrayRef<DeclNode *> &items, bool separate_items, std::string &output);
  Status EmitRecordDecl(const char *keyword, StringRef name, const ArrayRef<DeclNode *> &items, std::string &output);
  Status EmitFunctionSignature(StringRef return_type, StringRef name, const ArrayRef<ParamDecl *> &params,
                               std::string &output);

  std::string indent_unit_;
  size_t indent_level_ = 0;
};

}  // namespace ge

#endif  // BASE_COMMON_OM2_CODEGEN_EMITTER_CPP_EMITTER_H
