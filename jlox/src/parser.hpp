#pragma once

#include <initializer_list>
#include <span>

#include "expr.hpp"
#include "token.hpp"
#include "token_type.hpp"

namespace jlox {

class Parser final {
public:
  explicit Parser(std::span<const Token> tokens) : tokens_(tokens) {}

  ExprPtr Parse();

private:
  struct ParseError {};

  ExprPtr Expression();
  ExprPtr Comma();
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
  const Token &Peek() const;
  const Token &Previous() const;

  bool IsAtEnd() const;

  const Token &Consume(TokenType type, std::string_view message);

  ParseError Error(const Token &token, std::string_view message);

  std::span<const Token> tokens_;
  std::size_t current_ = 0;
};

} // namespace jlox