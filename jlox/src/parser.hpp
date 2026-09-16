#pragma once

#include "expr.hpp"
#include "stmt.hpp"
#include "token.hpp"

#include <initializer_list>
#include <span>
#include <vector>

namespace jlox {

class Parser final {
public:
  explicit Parser(std::span<const Token> tokens) : tokens_(tokens) {}

  [[nodiscard]] std::vector<StmtPtr> Parse();
  [[nodiscard]] ExprPtr ParseExpression();

private:
  class ParseError final {};

  // Declarations / statements
  StmtPtr Declaration();
  StmtPtr VarDeclaration();
  StmtPtr Statement();
  StmtPtr PrintStatement();
  StmtPtr ExpressionStatement();

  std::vector<StmtPtr> Block();

  // Expressions
  ExprPtr Expression();
  ExprPtr Comma();
  ExprPtr Assignment();
  ExprPtr Conditional();
  ExprPtr Equality();
  ExprPtr Comparison();
  ExprPtr Term();
  ExprPtr Factor();
  ExprPtr Unary();
  ExprPtr Primary();

  bool Match(std::initializer_list<TokenType> types);
  bool Check(TokenType type) const;
  const Token &Advance();
  bool IsAtEnd() const;

  const Token &Peek() const;
  const Token &Previous() const;

  const Token &Consume(TokenType type, std::string_view message);

  ParseError Error(const Token &token, std::string_view message);
  void Synchronize();

  std::span<const Token> tokens_;
  std::size_t current_ = 0;
};

} // namespace jlox