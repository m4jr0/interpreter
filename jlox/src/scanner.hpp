#pragma once

#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "token.hpp"
#include "token_type.hpp"

namespace jlox {

class Scanner final {
public:
  explicit Scanner(std::string_view source) : source_(source) {}

  std::span<const Token> ScanTokens();

private:
  bool IsAtEnd() const;

  void ScanToken();
  char Advance();
  bool Match(char expected);

  bool IsDigit(char c) const;
  bool IsAlpha(char c) const;
  bool IsAlphaNumeric(char c) const;

  char Peek() const;
  char PeekNext() const;

  void String();
  void Number();
  void Identifier();

  void BlockComment();

  void AddToken(TokenType type);
  void AddToken(TokenType type, Literal literal);

  inline static const std::unordered_map<std::string_view, TokenType> keywords_{
      {"and", TokenType::And},       {"class", TokenType::Class},
      {"else", TokenType::Else},     {"false", TokenType::False},
      {"for", TokenType::For},       {"fun", TokenType::Fun},
      {"if", TokenType::If},         {"nil", TokenType::Nil},
      {"or", TokenType::Or},         {"print", TokenType::Print},
      {"return", TokenType::Return}, {"super", TokenType::Super},
      {"this", TokenType::This},     {"true", TokenType::True},
      {"var", TokenType::Var},       {"while", TokenType::While},
  };

  std::size_t start_ = 0;
  std::size_t current_ = 0;
  std::size_t line_ = 1;

  std::string_view source_;
  std::vector<Token> tokens_;
};

} // namespace jlox