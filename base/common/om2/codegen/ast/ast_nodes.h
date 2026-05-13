/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef BASE_COMMON_OM2_CODEGEN_AST_AST_NODES_H
#define BASE_COMMON_OM2_CODEGEN_AST_AST_NODES_H

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "ast_context.h"
#include "common/checker.h"
#include "common/om2/codegen/emitter/code_emitter.h"

namespace ge {
class CodeEmitter;

class AstNode {
 public:
  virtual ~AstNode() = default;
  virtual Status Accept(CodeEmitter &emitter, std::string &output) const = 0;
};

class DeclNode : public AstNode {};
class Stmt : public AstNode {};
class Expr : public AstNode {};

enum class BuiltinType : uint8_t {
  kVoid = 0,
  kBool,
  kChar,
  kInt8,
  kUInt8,
  kInt16,
  kUInt16,
  kInt32,
  kUInt32,
  kInt64,
  kUInt64,
  kFloat,
  kDouble,
};

using TypeName = std::variant<BuiltinType, StringRef>;

class ParamDecl final : public AstNode {
 public:
  static ParamDecl *Create(AstContext &ctx, const std::string &type_spec, const std::string &name);
  ParamDecl(StringRef type_spec, StringRef name) : type_spec_(type_spec), name_(name) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  StringRef GetTypeSpec() const { return type_spec_; }
  StringRef GetName() const { return name_; }

 private:
  StringRef type_spec_;
  StringRef name_;
};

class TranslationUnit final : public DeclNode {
 public:
  static TranslationUnit *Create(AstContext &ctx, const std::vector<DeclNode *> &items);
  explicit TranslationUnit(ArrayRef<DeclNode *> items) : items_(items) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  ArrayRef<DeclNode *> GetItems() const { return items_; }

 private:
  ArrayRef<DeclNode *> items_;
};

enum class StablePartId : uint8_t {
  kChkStatusMacro = 0,
  kChkNotNullMacro,
  kChkTrueMacro,
  kGetAddrMacro,
  kMakeGuardMacro,
  kInterfaceMacros,
  kPointerHelpers,
  kFlattenHostArgs,
  kInterfacePointerHelpers,
  kScopeGuard,
  kReadBinaryFileToBuffer,
  kGenerateJsonFile,
  kLoadAndRunExternalApis,
  kInterfaceDumpApis,
  kLoadAndRunDumpHelpers,
  kCreateLabelListForLabelSwitch,
  kCreateLabelListForLabelGotoEx
};

enum class StablePartPlacement : uint8_t {
  kTranslationUnit = 0,
  kNamespace,
};

enum class StablePartRole : uint8_t {
  kMacroGroup = 0,
  kHelperFunctionGroup,
  kHelperType,
};

class StablePartDecl final : public DeclNode {
 public:
  static StablePartDecl *Create(AstContext &ctx, StablePartId id, StablePartRole role, StablePartPlacement placement);
  StablePartDecl(StablePartId id, StablePartRole role, StablePartPlacement placement)
      : id_(id), role_(role), placement_(placement) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  StablePartId GetId() const { return id_; }
  StablePartRole GetRole() const { return role_; }
  StablePartPlacement GetPlacement() const { return placement_; }

 private:
  StablePartId id_;
  StablePartRole role_;
  StablePartPlacement placement_;
};

class IncludeDecl final : public DeclNode {
 public:
  enum class Kind : uint8_t {
    kQuote = 0,
    kAngle,
  };

  static IncludeDecl *Create(AstContext &ctx, const std::string &path, Kind kind);
  IncludeDecl(StringRef path, Kind kind) : path_(path), kind_(kind) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  StringRef GetPath() const { return path_; }
  Kind GetKind() const { return kind_; }

 private:
 StringRef path_;
 Kind kind_;
};

class SpaceDecl final : public DeclNode {
 public:
  static SpaceDecl *Create(AstContext &ctx);
  SpaceDecl() = default;
  Status Accept(CodeEmitter &emitter, std::string &output) const override;
};

class TypeAliasDecl final : public DeclNode {
 public:
  static TypeAliasDecl *Create(AstContext &ctx, const std::string &type_spec, const std::string &name);
  TypeAliasDecl(StringRef type_spec, StringRef name) : type_spec_(type_spec), name_(name) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  StringRef GetTypeSpec() const { return type_spec_; }
  StringRef GetName() const { return name_; }

 private:
  StringRef type_spec_;
  StringRef name_;
};

class NamespaceDecl final : public DeclNode {
 public:
  static NamespaceDecl *Create(AstContext &ctx, const std::string &name, const std::vector<DeclNode *> &items);
  NamespaceDecl(StringRef name, ArrayRef<DeclNode *> items) : name_(name), items_(items) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  StringRef GetName() const { return name_; }
  ArrayRef<DeclNode *> GetItems() const { return items_; }

 private:
  StringRef name_;
  ArrayRef<DeclNode *> items_;
};

class ExternBlockDecl final : public DeclNode {
 public:
  static ExternBlockDecl *Create(AstContext &ctx, const std::string &language, const std::vector<DeclNode *> &items);
  ExternBlockDecl(StringRef language, ArrayRef<DeclNode *> items) : language_(language), items_(items) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  StringRef GetLanguage() const { return language_; }
  ArrayRef<DeclNode *> GetItems() const { return items_; }

 private:
  StringRef language_;
  ArrayRef<DeclNode *> items_;
};

class ClassDecl final : public DeclNode {
 public:
  static ClassDecl *Create(AstContext &ctx, const std::string &name, const std::vector<DeclNode *> &items);
  ClassDecl(StringRef name, ArrayRef<DeclNode *> items) : name_(name), items_(items) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  StringRef GetName() const { return name_; }
  ArrayRef<DeclNode *> GetItems() const { return items_; }

 private:
  StringRef name_;
  ArrayRef<DeclNode *> items_;
};

class StructDecl final : public DeclNode {
 public:
  static StructDecl *Create(AstContext &ctx, const std::string &name, const std::vector<DeclNode *> &items);
  StructDecl(StringRef name, ArrayRef<DeclNode *> items) : name_(name), items_(items) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  StringRef GetName() const { return name_; }
  ArrayRef<DeclNode *> GetItems() const { return items_; }

 private:
  StringRef name_;
  ArrayRef<DeclNode *> items_;
};

class AccessSectionDecl final : public DeclNode {
 public:
  enum class Kind : uint8_t {
    kPublic = 0,
    kPrivate,
    kProtected,
  };

  static AccessSectionDecl *Create(AstContext &ctx, Kind kind);
  explicit AccessSectionDecl(Kind kind) : kind_(kind) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  Kind GetKind() const { return kind_; }

 private:
  Kind kind_;
};

class FieldDecl final : public DeclNode {
 public:
  static FieldDecl *Create(AstContext &ctx, const std::string &type_spec, const std::string &name, Expr *init = nullptr);
  FieldDecl(StringRef type_spec, StringRef name, Expr *init) : type_spec_(type_spec), name_(name), init_(init) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  StringRef GetTypeSpec() const { return type_spec_; }
  StringRef GetName() const { return name_; }
  Expr *GetInit() const { return init_; }

 private:
  StringRef type_spec_;
  StringRef name_;
  Expr *init_;
};

class MethodDecl final : public DeclNode {
 public:
  static MethodDecl *Create(AstContext &ctx, const std::string &name, const std::vector<ParamDecl *> &params,
                            const std::string &return_type, const std::string &trailing_spec = "");
  MethodDecl(StringRef name, ArrayRef<ParamDecl *> params, StringRef return_type, StringRef trailing_spec)
      : name_(name), params_(params), return_type_(return_type), trailing_spec_(trailing_spec) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  StringRef GetName() const { return name_; }
  ArrayRef<ParamDecl *> GetParams() const { return params_; }
  StringRef GetReturnType() const { return return_type_; }
  StringRef GetTrailingSpec() const { return trailing_spec_; }

 private:
  StringRef name_;
  ArrayRef<ParamDecl *> params_;
  StringRef return_type_;
  StringRef trailing_spec_;
};

class FunctionDecl final : public DeclNode {
 public:
  static FunctionDecl *Create(AstContext &ctx, const std::string &name, const std::vector<ParamDecl *> &params,
                              const std::string &return_type);
  FunctionDecl(StringRef name, ArrayRef<ParamDecl *> params, StringRef return_type)
      : name_(name), params_(params), return_type_(return_type) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  StringRef GetName() const { return name_; }
  ArrayRef<ParamDecl *> GetParams() const { return params_; }
  StringRef GetReturnType() const { return return_type_; }

 private:
  StringRef name_;
  ArrayRef<ParamDecl *> params_;
  StringRef return_type_;
};

class BlockStmt;

class FunctionDef final : public DeclNode {
 public:
  static FunctionDef *Create(AstContext &ctx, const std::string &name, const std::vector<ParamDecl *> &params,
                             const std::string &return_type, BlockStmt *body);
  FunctionDef(StringRef name, ArrayRef<ParamDecl *> params, StringRef return_type, BlockStmt *body)
      : name_(name), params_(params), return_type_(return_type), body_(body) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  StringRef GetName() const { return name_; }
  ArrayRef<ParamDecl *> GetParams() const { return params_; }
  StringRef GetReturnType() const { return return_type_; }
  BlockStmt *GetBody() const { return body_; }

 private:
  StringRef name_;
  ArrayRef<ParamDecl *> params_;
  StringRef return_type_;
  BlockStmt *body_;
};

class MethodDef final : public DeclNode {
 public:
  static MethodDef *Create(AstContext &ctx, const std::string &owner, const std::string &name,
                           const std::vector<ParamDecl *> &params, const std::string &return_type,
                           const std::vector<std::string> &member_init_names, const std::vector<Expr *> &member_init_exprs,
                           BlockStmt *body);
  MethodDef(StringRef owner, StringRef name, ArrayRef<ParamDecl *> params, StringRef return_type,
            ArrayRef<StringRef> member_init_names, ArrayRef<Expr *> member_init_exprs, BlockStmt *body)
      : owner_(owner), name_(name), params_(params), return_type_(return_type), member_init_names_(member_init_names),
        member_init_exprs_(member_init_exprs), body_(body) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  StringRef GetOwner() const { return owner_; }
  StringRef GetName() const { return name_; }
  ArrayRef<ParamDecl *> GetParams() const { return params_; }
  StringRef GetReturnType() const { return return_type_; }
  ArrayRef<StringRef> GetMemberInitNames() const { return member_init_names_; }
  ArrayRef<Expr *> GetMemberInitExprs() const { return member_init_exprs_; }
  BlockStmt *GetBody() const { return body_; }

 private:
  StringRef owner_;
  StringRef name_;
  ArrayRef<ParamDecl *> params_;
  StringRef return_type_;
  ArrayRef<StringRef> member_init_names_;
  ArrayRef<Expr *> member_init_exprs_;
  BlockStmt *body_;
};

class IdentifierExpr final : public Expr {
 public:
  static IdentifierExpr *Create(AstContext &ctx, const std::string &name);
  explicit IdentifierExpr(StringRef name) : name_(name) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  StringRef GetName() const { return name_; }

 private:
  StringRef name_;
};

class LiteralExpr final : public Expr {
 public:
  enum class Kind : uint8_t {
    kInt = 0,
    kBool,
    kString,
    kNullptr,
  };

  enum class IntSuffix : uint8_t {
    kNone = 0,
    kU,
    kL,
    kUL,
  };

  static LiteralExpr *CreateInt(AstContext &ctx, int64_t value, IntSuffix suffix = IntSuffix::kNone);
  static LiteralExpr *CreateBool(AstContext &ctx, bool value);
  static LiteralExpr *CreateString(AstContext &ctx, const std::string &value);
  static LiteralExpr *CreateNullptr(AstContext &ctx);
  LiteralExpr(Kind kind, int64_t int_value, IntSuffix int_suffix, bool bool_value, StringRef string_value)
      : kind_(kind), int_value_(int_value), int_suffix_(int_suffix), bool_value_(bool_value), string_value_(string_value) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  Kind GetKind() const { return kind_; }
  int64_t GetIntValue() const { return int_value_; }
  IntSuffix GetIntSuffix() const { return int_suffix_; }
  bool GetBoolValue() const { return bool_value_; }
  StringRef GetStringValue() const { return string_value_; }

 private:
  Kind kind_;
  int64_t int_value_;
  IntSuffix int_suffix_;
  bool bool_value_;
  StringRef string_value_;
};

class AssignExpr final : public Expr {
 public:
  static AssignExpr *Create(AstContext &ctx, Expr *lhs, Expr *rhs);
  AssignExpr(Expr *lhs, Expr *rhs) : lhs_(lhs), rhs_(rhs) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  Expr *GetLhs() const { return lhs_; }
  Expr *GetRhs() const { return rhs_; }

 private:
  Expr *lhs_;
  Expr *rhs_;
};

class BinaryExpr final : public Expr {
 public:
  enum class Op : uint8_t {
    kEq = 0,
    kNe,
    kLt,
    kLe,
    kGt,
    kGe,
    kLogicalAnd,
    kLogicalOr,
    kAdd,
    kSub,
    kMul,
    kDiv,
    kMod,
    kBitAnd,
    kBitOr,
    kBitXor,
    kShiftLeft,
    kShiftRight,
  };

  static BinaryExpr *Create(AstContext &ctx, Op op, Expr *lhs, Expr *rhs);
  BinaryExpr(Op op, Expr *lhs, Expr *rhs) : op_(op), lhs_(lhs), rhs_(rhs) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  Op GetOp() const { return op_; }
  Expr *GetLhs() const { return lhs_; }
  Expr *GetRhs() const { return rhs_; }

 private:
  Op op_;
  Expr *lhs_;
  Expr *rhs_;
};

class UnaryExpr final : public Expr {
 public:
  enum class Op : uint8_t {
    kLogicalNot = 0,
    kNegate,
    kBitNot,
    kDeref,
    kPreInc,
    kPostInc,
  };

  static UnaryExpr *Create(AstContext &ctx, Op op, Expr *expr);
  UnaryExpr(Op op, Expr *expr) : op_(op), expr_(expr) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  Op GetOp() const { return op_; }
  Expr *GetExpr() const { return expr_; }

 private:
  Op op_;
  Expr *expr_;
};

class CallExpr final : public Expr {
public:
  static CallExpr *Create(AstContext &ctx, Expr *callee, const std::vector<Expr *> &args);
  CallExpr(Expr *callee, ArrayRef<Expr *> args) : callee_(callee), args_(args) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  Expr *GetCallee() const { return callee_; }
  ArrayRef<Expr *> GetArgs() const { return args_; }

 private:
  Expr *callee_;
  ArrayRef<Expr *> args_;
};

class MakeUniqueArrayExpr final : public Expr {
 public:
  static MakeUniqueArrayExpr *Create(AstContext &ctx, const TypeName &elem_type, Expr *count);
  MakeUniqueArrayExpr(TypeName elem_type, Expr *count) : elem_type_(elem_type), count_(count) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  const TypeName &GetElemType() const { return elem_type_; }
  Expr *GetCount() const { return count_; }

 private:
  TypeName elem_type_;
  Expr *count_;
};

class ToStrExpr final : public Expr {
 public:
  static ToStrExpr *Create(AstContext &ctx, Expr *expr);
  explicit ToStrExpr(Expr *expr) : expr_(expr) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  Expr *GetExpr() const { return expr_; }

 private:
  Expr *expr_;
};

class MemcpyExpr final : public Expr {
 public:
  static MemcpyExpr *Create(AstContext &ctx, Expr *dst, Expr *src, Expr *size);
  MemcpyExpr(Expr *dst, Expr *src, Expr *size) : dst_(dst), src_(src), size_(size) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  Expr *GetDst() const { return dst_; }
  Expr *GetSrc() const { return src_; }
  Expr *GetSize() const { return size_; }

 private:
  Expr *dst_;
  Expr *src_;
  Expr *size_;
};

class SizeofExpr final : public Expr {
 public:
  static SizeofExpr *Create(AstContext &ctx, Expr *expr);
  explicit SizeofExpr(Expr *expr) : expr_(expr) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  Expr *GetExpr() const { return expr_; }

 private:
  Expr *expr_;
};

class RemoveFileExpr final : public Expr {
 public:
  static RemoveFileExpr *Create(AstContext &ctx, Expr *path);
  explicit RemoveFileExpr(Expr *path) : path_(path) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  Expr *GetPath() const { return path_; }

 private:
  Expr *path_;
};

class IgnoreOutputExpr final : public Expr {
 public:
  static IgnoreOutputExpr *Create(AstContext &ctx, Expr *expr);
  explicit IgnoreOutputExpr(Expr *expr) : expr_(expr) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  Expr *GetExpr() const { return expr_; }

 private:
  Expr *expr_;
};

class ContainerMethodExpr final : public Expr {
 public:
  enum class Method : uint8_t {
    kClear = 0,
    kResize,
    kSize,
    kData,
    kEmpty,
    kAt,
    kPushBack,
    kCStr,
    kGetPtr,
  };

  static ContainerMethodExpr *Create(AstContext &ctx, Method method, Expr *container, const std::vector<Expr *> &args);
  ContainerMethodExpr(Method method, Expr *container, ArrayRef<Expr *> args)
      : method_(method), container_(container), args_(args) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  Method GetMethod() const { return method_; }
  Expr *GetContainer() const { return container_; }
  ArrayRef<Expr *> GetArgs() const { return args_; }

 private:
  Method method_;
  Expr *container_;
  ArrayRef<Expr *> args_;
};

class AddrOfExpr final : public Expr {
public:
  static AddrOfExpr *Create(AstContext &ctx, Expr *expr);
  explicit AddrOfExpr(Expr *expr) : expr_(expr) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  Expr *GetExpr() const { return expr_; }

 private:
  Expr *expr_;
};

class SubscriptExpr final : public Expr {
 public:
  static SubscriptExpr *Create(AstContext &ctx, Expr *base, Expr *index);
  SubscriptExpr(Expr *base, Expr *index) : base_(base), index_(index) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  Expr *GetBase() const { return base_; }
  Expr *GetIndex() const { return index_; }

 private:
  Expr *base_;
  Expr *index_;
};

class MemberExpr final : public Expr {
 public:
  static MemberExpr *Create(AstContext &ctx, Expr *object, const std::string &field);
  MemberExpr(Expr *object, StringRef field) : object_(object), field_(field) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  Expr *GetObject() const { return object_; }
  StringRef GetField() const { return field_; }

 private:
  Expr *object_;
  StringRef field_;
};

class CppArrowMemberExpr final : public Expr {
 public:
  static CppArrowMemberExpr *Create(AstContext &ctx, Expr *object, const std::string &field);
  CppArrowMemberExpr(Expr *object, StringRef field) : object_(object), field_(field) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  Expr *GetObject() const { return object_; }
  StringRef GetField() const { return field_; }

 private:
  Expr *object_;
  StringRef field_;
};

class CppCastExpr final : public Expr {
 public:
  enum class Kind : uint8_t {
    kReinterpret = 0,
    kConst,
    kStatic,
  };

  static CppCastExpr *Create(AstContext &ctx, Kind kind, const std::string &target_type, Expr *expr);
  CppCastExpr(Kind kind, StringRef target_type, Expr *expr) : kind_(kind), target_type_(target_type), expr_(expr) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  Kind GetKind() const { return kind_; }
  StringRef GetTargetType() const { return target_type_; }
  Expr *GetExpr() const { return expr_; }

 private:
  Kind kind_;
  StringRef target_type_;
  Expr *expr_;
};

class LambdaExpr final : public Expr {
 public:
  static LambdaExpr *Create(AstContext &ctx, const std::vector<std::string> &captures, BlockStmt *body);
  LambdaExpr(ArrayRef<StringRef> captures, BlockStmt *body) : captures_(captures), body_(body) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  ArrayRef<StringRef> GetCaptures() const { return captures_; }
  BlockStmt *GetBody() const { return body_; }

 private:
  ArrayRef<StringRef> captures_;
  BlockStmt *body_;
};

class InitListExpr final : public Expr {
 public:
  static InitListExpr *Create(AstContext &ctx, const std::vector<Expr *> &elements);
  explicit InitListExpr(ArrayRef<Expr *> elements) : elements_(elements) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  ArrayRef<Expr *> GetElements() const { return elements_; }

 private:
  ArrayRef<Expr *> elements_;
};

class CommentStmt final : public Stmt {
 public:
  static CommentStmt *Create(AstContext &ctx, const std::string &text);
  explicit CommentStmt(StringRef text) : text_(text) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  StringRef GetText() const { return text_; }

 private:
  StringRef text_;
};

class BlankLineStmt final : public Stmt {
 public:
  static BlankLineStmt *Create(AstContext &ctx);
  BlankLineStmt() = default;
  Status Accept(CodeEmitter &emitter, std::string &output) const override;
};

class VarDeclStmt final : public Stmt {
 public:
  static VarDeclStmt *Create(AstContext &ctx, const std::string &type_spec, const std::string &name, Expr *init = nullptr);
  VarDeclStmt(StringRef type_spec, StringRef name, Expr *init) : type_spec_(type_spec), name_(name), init_(init) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  StringRef GetTypeSpec() const { return type_spec_; }
  StringRef GetName() const { return name_; }
  Expr *GetInit() const { return init_; }

 private:
  StringRef type_spec_;
  StringRef name_;
  Expr *init_;
};

class ExprStmt final : public Stmt {
 public:
  static ExprStmt *Create(AstContext &ctx, Expr *expr);
  explicit ExprStmt(Expr *expr) : expr_(expr) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  Expr *GetExpr() const { return expr_; }

 private:
  Expr *expr_;
};

class ReturnStmt final : public Stmt {
 public:
  static ReturnStmt *Create(AstContext &ctx, Expr *value = nullptr);
  explicit ReturnStmt(Expr *value) : value_(value) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  Expr *GetValue() const { return value_; }

 private:
  Expr *value_;
};

class BlockStmt final : public Stmt {
 public:
  static BlockStmt *Create(AstContext &ctx, const std::vector<Stmt *> &statements);
  explicit BlockStmt(ArrayRef<Stmt *> statements) : statements_(statements) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  ArrayRef<Stmt *> GetStatements() const { return statements_; }

 private:
  ArrayRef<Stmt *> statements_;
};

class IfStmt final : public Stmt {
 public:
  static IfStmt *Create(AstContext &ctx, Expr *cond, BlockStmt *then_block, BlockStmt *else_block = nullptr);
  IfStmt(Expr *cond, BlockStmt *then_block, BlockStmt *else_block)
      : cond_(cond), then_block_(then_block), else_block_(else_block) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  Expr *GetCond() const { return cond_; }
  BlockStmt *GetThenBlock() const { return then_block_; }
  BlockStmt *GetElseBlock() const { return else_block_; }

 private:
  Expr *cond_;
  BlockStmt *then_block_;
  BlockStmt *else_block_;
};

class ForStmt final : public Stmt {
 public:
  static ForStmt *Create(AstContext &ctx, Stmt *init, Expr *cond, Expr *step, BlockStmt *body);
  ForStmt(Stmt *init, Expr *cond, Expr *step, BlockStmt *body) : init_(init), cond_(cond), step_(step), body_(body) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  Stmt *GetInit() const { return init_; }
  Expr *GetCond() const { return cond_; }
  Expr *GetStep() const { return step_; }
  BlockStmt *GetBody() const { return body_; }

 private:
  Stmt *init_;
  Expr *cond_;
  Expr *step_;
  BlockStmt *body_;
};

class RangeForStmt final : public Stmt {
 public:
  static RangeForStmt *Create(AstContext &ctx, const std::string &type_spec, const std::string &name, Expr *range,
                              BlockStmt *body);
  RangeForStmt(StringRef type_spec, StringRef name, Expr *range, BlockStmt *body)
      : type_spec_(type_spec), name_(name), range_(range), body_(body) {}
  Status Accept(CodeEmitter &emitter, std::string &output) const override;

  StringRef GetTypeSpec() const { return type_spec_; }
  StringRef GetName() const { return name_; }
  Expr *GetRange() const { return range_; }
  BlockStmt *GetBody() const { return body_; }

 private:
  StringRef type_spec_;
  StringRef name_;
  Expr *range_;
  BlockStmt *body_;
};

}  // namespace ge
#endif  // BASE_COMMON_OM2_CODEGEN_AST_AST_NODES_H
