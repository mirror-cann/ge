/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef BASE_COMMON_OM2_CODEGEN_EMITTER_CODE_EMITTER_H
#define BASE_COMMON_OM2_CODEGEN_EMITTER_CODE_EMITTER_H

#include <string>

#include "ge_common/ge_common_api_types.h"

namespace ge {
class ParamDecl;
class TranslationUnit;
class StablePartDecl;
class IncludeDecl;
class SpaceDecl;
class TypeAliasDecl;
class NamespaceDecl;
class ExternBlockDecl;
class ClassDecl;
class StructDecl;
class AccessSectionDecl;
class FieldDecl;
class MethodDecl;
class FunctionDecl;
class FunctionDef;
class MethodDef;
class IdentifierExpr;
class LiteralExpr;
class AssignExpr;
class BinaryExpr;
class UnaryExpr;
class CallExpr;
class MakeUniqueArrayExpr;
class ToStrExpr;
class MemcpyExpr;
class SizeofExpr;
class RemoveFileExpr;
class IgnoreOutputExpr;
class ContainerMethodExpr;
class AddrOfExpr;
class SubscriptExpr;
class MemberExpr;
class CppArrowMemberExpr;
class CppCastExpr;
class LambdaExpr;
class CCastExpr;
class InitListExpr;
class DesignatedInitListExpr;
class CommentStmt;
class BlankLineStmt;
class VarDeclStmt;
class ExprStmt;
class ReturnStmt;
class BlockStmt;
class IfStmt;
class ForStmt;
class RangeForStmt;
class CaseStmt;
class BreakStmt;
class SwitchStmt;

class CodeEmitter {
 public:
  virtual ~CodeEmitter() = default;

  virtual Status Emit(const ParamDecl &node, std::string &output) = 0;
  virtual Status Emit(const TranslationUnit &node, std::string &output) = 0;
  virtual Status Emit(const StablePartDecl &node, std::string &output) = 0;
  virtual Status Emit(const IncludeDecl &node, std::string &output) = 0;
  virtual Status Emit(const SpaceDecl &node, std::string &output) = 0;
  virtual Status Emit(const TypeAliasDecl &node, std::string &output) = 0;
  virtual Status Emit(const NamespaceDecl &node, std::string &output) = 0;
  virtual Status Emit(const ExternBlockDecl &node, std::string &output) = 0;
  virtual Status Emit(const ClassDecl &node, std::string &output) = 0;
  virtual Status Emit(const StructDecl &node, std::string &output) = 0;
  virtual Status Emit(const AccessSectionDecl &node, std::string &output) = 0;
  virtual Status Emit(const FieldDecl &node, std::string &output) = 0;
  virtual Status Emit(const MethodDecl &node, std::string &output) = 0;
  virtual Status Emit(const FunctionDecl &node, std::string &output) = 0;
  virtual Status Emit(const FunctionDef &node, std::string &output) = 0;
  virtual Status Emit(const MethodDef &node, std::string &output) = 0;
  virtual Status Emit(const IdentifierExpr &node, std::string &output) = 0;
  virtual Status Emit(const LiteralExpr &node, std::string &output) = 0;
  virtual Status Emit(const AssignExpr &node, std::string &output) = 0;
  virtual Status Emit(const BinaryExpr &node, std::string &output) = 0;
  virtual Status Emit(const UnaryExpr &node, std::string &output) = 0;
  virtual Status Emit(const CallExpr &node, std::string &output) = 0;
  virtual Status Emit(const MakeUniqueArrayExpr &node, std::string &output) = 0;
  virtual Status Emit(const ToStrExpr &node, std::string &output) = 0;
  virtual Status Emit(const MemcpyExpr &node, std::string &output) = 0;
  virtual Status Emit(const SizeofExpr &node, std::string &output) = 0;
  virtual Status Emit(const RemoveFileExpr &node, std::string &output) = 0;
  virtual Status Emit(const IgnoreOutputExpr &node, std::string &output) = 0;
  virtual Status Emit(const ContainerMethodExpr &node, std::string &output) = 0;
  virtual Status Emit(const AddrOfExpr &node, std::string &output) = 0;
  virtual Status Emit(const SubscriptExpr &node, std::string &output) = 0;
  virtual Status Emit(const MemberExpr &node, std::string &output) = 0;
  virtual Status Emit(const CppArrowMemberExpr &node, std::string &output) = 0;
  virtual Status Emit(const CppCastExpr &node, std::string &output) = 0;
  virtual Status Emit(const LambdaExpr &node, std::string &output) = 0;
  virtual Status Emit(const CCastExpr &node, std::string &output) = 0;
  virtual Status Emit(const InitListExpr &node, std::string &output) = 0;
  virtual Status Emit(const DesignatedInitListExpr &node, std::string &output) = 0;
  virtual Status Emit(const CommentStmt &node, std::string &output) = 0;
  virtual Status Emit(const BlankLineStmt &node, std::string &output) = 0;
  virtual Status Emit(const VarDeclStmt &node, std::string &output) = 0;
  virtual Status Emit(const ExprStmt &node, std::string &output) = 0;
  virtual Status Emit(const ReturnStmt &node, std::string &output) = 0;
  virtual Status Emit(const BlockStmt &node, std::string &output) = 0;
  virtual Status Emit(const IfStmt &node, std::string &output) = 0;
  virtual Status Emit(const ForStmt &node, std::string &output) = 0;
  virtual Status Emit(const RangeForStmt &node, std::string &output) = 0;
  virtual Status Emit(const CaseStmt &node, std::string &output) = 0;
  virtual Status Emit(const BreakStmt &node, std::string &output) = 0;
  virtual Status Emit(const SwitchStmt &node, std::string &output) = 0;
};

}  // namespace ge
#endif  // BASE_COMMON_OM2_CODEGEN_EMITTER_CODE_EMITTER_H
