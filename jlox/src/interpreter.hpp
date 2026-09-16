#pragma once

#include "environment.hpp"
#include "expr.hpp"
#include "stmt.hpp"
#include "value.hpp"

#include <memory>
#include <span>

namespace jlox {

class Interpreter final : public ExprVisitor, public StmtVisitor {
public:
  Interpreter();

  void Interpret(std::span<const StmtPtr> statements);
  void InterpretExpression(const Expr &expression);

  // Expressions
  void VisitAssignExpr(const AssignExpr &expr) override;

  void VisitBinaryExpr(const BinaryExpr &expr) override;

  void VisitConditionalExpr(const ConditionalExpr &expr) override;

  void VisitGroupingExpr(const GroupingExpr &expr) override;

  void VisitLiteralExpr(const LiteralExpr &expr) override;

  void VisitUnaryExpr(const UnaryExpr &expr) override;

  void VisitVariableExpr(const VariableExpr &expr) override;

  // Statements
  void VisitBlockStmt(const BlockStmt &stmt) override;

  void VisitExpressionStmt(const ExpressionStmt &stmt) override;

  void VisitPrintStmt(const PrintStmt &stmt) override;

  void VisitVarStmt(const VarStmt &stmt) override;

private:
  Value Evaluate(const Expr &expr);
  void Execute(const Stmt &stmt);

  void ExecuteBlock(const std::vector<StmtPtr> &statements,
                    std::shared_ptr<Environment> environment);

  static bool IsTruthy(const Value &value);
  static bool IsEqual(const Value &left, const Value &right);

  static void CheckNumberOperand(const Token &op, const Value &operand);

  static void CheckNumberOperands(const Token &op, const Value &left,
                                  const Value &right);

  static std::string Stringify(const Value &value);

  Value value_;

  std::shared_ptr<Environment> globals_;
  std::shared_ptr<Environment> environment_;
};

} // namespace jlox