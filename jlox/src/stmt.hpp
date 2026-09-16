#pragma once

#include "token.hpp"
#include "expr.hpp"

#include <memory>
#include <utility>
#include <vector>

namespace jlox {

class Stmt;
class BlockStmt;
class ExpressionStmt;
class PrintStmt;
class VarStmt;

class StmtVisitor {
public:
  virtual ~StmtVisitor() = default;

  virtual void VisitBlockStmt(const BlockStmt &block) = 0;
  virtual void VisitExpressionStmt(const ExpressionStmt &expression) = 0;
  virtual void VisitPrintStmt(const PrintStmt &print) = 0;
  virtual void VisitVarStmt(const VarStmt &var) = 0;
};

class Stmt {
public:
  virtual ~Stmt() = default;
  virtual void Accept(StmtVisitor &visitor) const = 0;
};

using StmtPtr = std::unique_ptr<Stmt>;

class BlockStmt final : public Stmt {
public:
  BlockStmt(std::vector<StmtPtr> statements)
      : statements(std::move(statements)) {}

  void Accept(StmtVisitor &visitor) const override {
    visitor.VisitBlockStmt(*this);
  }

  std::vector<StmtPtr> statements;
};

class ExpressionStmt final : public Stmt {
public:
  ExpressionStmt(ExprPtr expression)
      : expression(std::move(expression)) {}

  void Accept(StmtVisitor &visitor) const override {
    visitor.VisitExpressionStmt(*this);
  }

  ExprPtr expression;
};

class PrintStmt final : public Stmt {
public:
  PrintStmt(ExprPtr expression)
      : expression(std::move(expression)) {}

  void Accept(StmtVisitor &visitor) const override {
    visitor.VisitPrintStmt(*this);
  }

  ExprPtr expression;
};

class VarStmt final : public Stmt {
public:
  VarStmt(Token name, ExprPtr initializer)
      : name(std::move(name)), initializer(std::move(initializer)) {}

  void Accept(StmtVisitor &visitor) const override {
    visitor.VisitVarStmt(*this);
  }

  Token name;
  ExprPtr initializer;
};

} // namespace jlox
