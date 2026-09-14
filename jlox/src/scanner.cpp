#include "scanner.hpp"

#include <charconv>
#include <string_view>
#include <system_error>

#include "lox.hpp"
#include "token_type.hpp"

namespace jlox {

namespace {

double ParseDouble(std::string_view text) {
  double value = 0.0;

  const char *begin = text.data();
  const char *end = begin + text.size();

  const auto [ptr, error] = std::from_chars(begin, end, value);

  if (error != std::errc{} || ptr != end) {
    return 0.0;
  }

  return value;
}

} // namespace

std::span<const Token> Scanner::ScanTokens() {
  while (!IsAtEnd()) {
    // We are at the beginning of the next lexeme.
    start_ = current_;
    ScanToken();
  }

  tokens_.emplace_back(TokenType::Eof, "", Literal{}, line_);
  return tokens_;
}

bool Scanner::IsAtEnd() const { return current_ >= source_.size(); }

void Scanner::ScanToken() {
  const char c = Advance();

  switch (c) {
  case '(':
    AddToken(TokenType::LeftParen);
    break;
  case ')':
    AddToken(TokenType::RightParen);
    break;
  case '{':
    AddToken(TokenType::LeftBrace);
    break;
  case '}':
    AddToken(TokenType::RightBrace);
    break;
  case ',':
    AddToken(TokenType::Comma);
    break;
  case '.':
    AddToken(TokenType::Dot);
    break;
  case '-':
    AddToken(TokenType::Minus);
    break;
  case '+':
    AddToken(TokenType::Plus);
    break;
  case ';':
    AddToken(TokenType::Semicolon);
    break;
  case '*':
    AddToken(TokenType::Star);
    break;
  case '?':
    AddToken(TokenType::Question);
    break;
  case ':':
    AddToken(TokenType::Colon);
    break;

  case '!':
    AddToken(Match('=') ? TokenType::BangEqual : TokenType::Bang);
    break;
  case '=':
    AddToken(Match('=') ? TokenType::EqualEqual : TokenType::Equal);
    break;
  case '<':
    AddToken(Match('=') ? TokenType::LessEqual : TokenType::Less);
    break;
  case '>':
    AddToken(Match('=') ? TokenType::GreaterEqual : TokenType::Greater);
    break;

  case '/':
    if (Match('/')) {
      // A comment goes until the end of the line.
      while (Peek() != '\n' && !IsAtEnd())
        Advance();
    } else if (Match('*')) {
      BlockComment();
    } else {
      AddToken(TokenType::Slash);
    }
    break;

  case ' ':
  case '\r':
  case '\t':
    // Ignore whitespace.
    break;

  case '\n':
    ++line_;
    break;

  case '"':
    String();
    break;

  default:
    if (IsDigit(c)) {
      Number();
    } else if (IsAlpha(c)) {
      Identifier();
    } else {
      Lox::Error(line_, "Unexpected character.");
    }

    break;
  }
}

char Scanner::Advance() { return source_[current_++]; }

bool Scanner::Match(char expected) {
  if (IsAtEnd())
    return false;

  if (source_[current_] != expected)
    return false;

  ++current_;
  return true;
}

bool Scanner::IsDigit(char c) const { return c >= '0' && c <= '9'; }

bool Scanner::IsAlpha(char c) const {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool Scanner::IsAlphaNumeric(char c) const { return IsAlpha(c) || IsDigit(c); }

char Scanner::Peek() const {
  if (IsAtEnd())
    return '\0';

  return source_[current_];
}

char Scanner::PeekNext() const {
  if (current_ + 1 >= source_.size())
    return '\0';

  return source_[current_ + 1];
}

void Scanner::String() {
  while (Peek() != '"' && !IsAtEnd()) {
    if (Peek() == '\n')
      ++line_;

    Advance();
  }

  if (IsAtEnd()) {
    Lox::Error(line_, "Unterminated string.");
    return;
  }

  // The closing ".
  Advance();

  // Trim the surrounding quotes.
  const std::string_view value =
      source_.substr(start_ + 1, current_ - start_ - 2);

  AddToken(TokenType::String, value);
}

void Scanner::Number() {
  while (IsDigit(Peek()))
    Advance();

  // Look for a fractional part.
  if (Peek() == '.' && IsDigit(PeekNext())) {
    // Consume the "."
    Advance();

    while (IsDigit(Peek()))
      Advance();
  }

  AddToken(TokenType::Number,
           ParseDouble(source_.substr(start_, current_ - start_)));
}

void Scanner::Identifier() {
  while (IsAlphaNumeric(Peek()))
    Advance();

  const std::string_view text = source_.substr(start_, current_ - start_);

  TokenType type = TokenType::Identifier;

  if (const auto it = keywords_.find(text); it != keywords_.end()) {
    type = it->second;
  }

  AddToken(type);
}

void Scanner::BlockComment() {
  std::size_t depth = 1;

  while (!IsAtEnd()) {
    if (Peek() == '/' && PeekNext() == '*') {
      Advance();
      Advance();
      ++depth;
      continue;
    }

    if (Peek() == '*' && PeekNext() == '/') {
      Advance();
      Advance();
      --depth;

      if (depth == 0)
        return;

      continue;
    }

    if (Peek() == '\n')
      ++line_;

    Advance();
  }

  Lox::Error(line_, "Unterminated block comment.");
}

void Scanner::AddToken(TokenType type) { AddToken(type, Literal{}); }

void Scanner::AddToken(TokenType type, Literal literal) {
  const std::string_view text = source_.substr(start_, current_ - start_);

  tokens_.emplace_back(type, text, literal, line_);
}

} // namespace jlox