#include "interpreter.hpp"

#include <iostream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include "lox.hpp"
#include "runtime_error.hpp"
#include "token_type.hpp"

namespace jlox {

Value Interpreter::Evaluate(const Expr &expr) {
  expr.Accept(*this);
  return value_;
}

void Interpreter::Execute(const Stmt &stmt) { stmt.Accept(*this); }

void Interpreter::ExecuteBlock(const std::vector<StmtPtr> &statements,
                               std::shared_ptr<Environment> environment) {
  const auto previous = environment_;

  try {
    environment_ = std::move(environment);

    for (const auto &statement : statements) {
      Execute(*statement);
    }
  } catch (...) {
    environment_ = previous;
    throw;
  }

  environment_ = previous;
}

void Interpreter::VisitLiteralExpr(const LiteralExpr &expr) {
  value_ = std::visit(
      [](const auto &literal) -> Value {
        using T = std::decay_t<decltype(literal)>;

        if constexpr (std::is_same_v<T, std::monostate>) {
          return std::monostate{};
        } else if constexpr (std::is_same_v<T, std::string_view>) {
          return std::string(literal);
        } else {
          return literal;
        }
      },
      expr.value);
}

void Interpreter::VisitGroupingExpr(const GroupingExpr &expr) {
  value_ = Evaluate(*expr.expression);
}

void Interpreter::VisitUnaryExpr(const UnaryExpr &expr) {
  const Value right = Evaluate(*expr.right);

  switch (expr.op.type) {
  case TokenType::Minus:
    CheckNumberOperand(expr.op, right);
    value_ = -std::get<double>(right);
    return;

  case TokenType::Bang:
    value_ = !IsTruthy(right);
    return;

  default:
    break;
  }

  value_ = std::monostate{};
}

void Interpreter::VisitVariableExpr(const VariableExpr &expr) {
  value_ = environment_->Get(expr.name);
}

void Interpreter::VisitBlockStmt(const BlockStmt &stmt) {
  ExecuteBlock(stmt.statements, std::make_shared<Environment>(environment_));
}

void Interpreter::VisitExpressionStmt(const ExpressionStmt &stmt) {
  Evaluate(*stmt.expression);
}

void Interpreter::VisitPrintStmt(const PrintStmt &stmt) {
  const Value value = Evaluate(*stmt.expression);

  std::cout << Stringify(value) << '\n';
}

void Interpreter::VisitVarStmt(const VarStmt &stmt) {
  Value value = std::monostate{};

  if (stmt.initializer) {
    value = Evaluate(*stmt.initializer);
  }

  environment_->Define(stmt.name.lexeme, std::move(value));
}

Interpreter::Interpreter()
    : globals_(std::make_shared<Environment>()), environment_(globals_) {}

void Interpreter::Interpret(std::span<const StmtPtr> statements) {
  try {
    for (const auto &statement : statements) {
      Execute(*statement);
    }
  } catch (const RuntimeError &error) {
    Lox::RuntimeError(error);
  }
}

void Interpreter::InterpretExpression(const Expr &expression) {
  try {
    const Value value = Evaluate(expression);
    std::cout << Stringify(value) << '\n';
  } catch (const RuntimeError &error) {
    Lox::RuntimeError(error);
  }
}
void Interpreter::VisitAssignExpr(const AssignExpr &expr) {
  Value value = Evaluate(*expr.value);

  environment_->Assign(expr.name, value);

  value_ = std::move(value);
}

void Interpreter::VisitBinaryExpr(const BinaryExpr &expr) {
  const Value left = Evaluate(*expr.left);
  const Value right = Evaluate(*expr.right);

  switch (expr.op.type) {
  case TokenType::Minus:
    CheckNumberOperands(expr.op, left, right);
    value_ = std::get<double>(left) - std::get<double>(right);
    return;

  case TokenType::Slash:
    CheckNumberOperands(expr.op, left, right);
    value_ = std::get<double>(left) / std::get<double>(right);
    return;

  case TokenType::Star:
    CheckNumberOperands(expr.op, left, right);
    value_ = std::get<double>(left) * std::get<double>(right);
    return;

  case TokenType::Plus:
    if (std::holds_alternative<double>(left) &&
        std::holds_alternative<double>(right)) {
      value_ = std::get<double>(left) + std::get<double>(right);
      return;
    }

    if (std::holds_alternative<std::string>(left) &&
        std::holds_alternative<std::string>(right)) {
      value_ = std::get<std::string>(left) + std::get<std::string>(right);
      return;
    }

    throw RuntimeError(expr.op, "Operands must be two numbers or two strings.");

  case TokenType::Greater:
    CheckNumberOperands(expr.op, left, right);
    value_ = std::get<double>(left) > std::get<double>(right);
    return;

  case TokenType::GreaterEqual:
    CheckNumberOperands(expr.op, left, right);
    value_ = std::get<double>(left) >= std::get<double>(right);
    return;

  case TokenType::Less:
    CheckNumberOperands(expr.op, left, right);
    value_ = std::get<double>(left) < std::get<double>(right);
    return;

  case TokenType::LessEqual:
    CheckNumberOperands(expr.op, left, right);
    value_ = std::get<double>(left) <= std::get<double>(right);
    return;

  case TokenType::BangEqual:
    value_ = !IsEqual(left, right);
    return;

  case TokenType::EqualEqual:
    value_ = IsEqual(left, right);
    return;

  case TokenType::Comma:
    // Both operands have already been evaluated above.
    // The comma operator discards the value of the left
    // operand and produces the value of the right operand.
    value_ = right;
    return;

  default:
    break;
  }

  value_ = std::monostate{};
}

void Interpreter::VisitConditionalExpr(const ConditionalExpr &expr) {
  const Value condition = Evaluate(*expr.condition);

  if (IsTruthy(condition)) {
    value_ = Evaluate(*expr.thenBranch);
  } else {
    value_ = Evaluate(*expr.elseBranch);
  }
}

bool Interpreter::IsTruthy(const Value &value) {
  if (std::holds_alternative<std::monostate>(value)) {
    return false;
  }

  if (const auto *boolean = std::get_if<bool>(&value)) {
    return *boolean;
  }

  return true;
}

bool Interpreter::IsEqual(const Value &left, const Value &right) {
  return left == right;
}

void Interpreter::CheckNumberOperand(const Token &op, const Value &operand) {
  if (std::holds_alternative<double>(operand)) {
    return;
  }

  throw RuntimeError(op, "Operand must be a number.");
}

void Interpreter::CheckNumberOperands(const Token &op, const Value &left,
                                      const Value &right) {
  if (std::holds_alternative<double>(left) &&
      std::holds_alternative<double>(right)) {
    return;
  }

  throw RuntimeError(op, "Operands must be numbers.");
}

std::string Interpreter::Stringify(const Value &value) {
  return std::visit(
      [](const auto &current) -> std::string {
        using T = std::decay_t<decltype(current)>;

        if constexpr (std::is_same_v<T, std::monostate>) {
          return "nil";
        } else if constexpr (std::is_same_v<T, bool>) {
          return current ? "true" : "false";
        } else if constexpr (std::is_same_v<T, double>) {
          std::string text = std::to_string(current);

          while (!text.empty() && text.back() == '0') {
            text.pop_back();
          }

          if (!text.empty() && text.back() == '.') {
            text.pop_back();
          }

          return text;
        } else {
          return current;
        }
      },
      value);
}

} // namespace jlox