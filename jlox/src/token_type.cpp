#include "token_type.hpp"

namespace jlox {

std::ostream &operator<<(std::ostream &stream, TokenType type) {
  switch (type) {
  case TokenType::LeftParen:
    return stream << "LEFT_PAREN";
  case TokenType::RightParen:
    return stream << "RIGHT_PAREN";
  case TokenType::LeftBrace:
    return stream << "LEFT_BRACE";
  case TokenType::RightBrace:
    return stream << "RIGHT_BRACE";
  case TokenType::Comma:
    return stream << "COMMA";
  case TokenType::Dot:
    return stream << "DOT";
  case TokenType::Minus:
    return stream << "MINUS";
  case TokenType::Plus:
    return stream << "PLUS";
  case TokenType::Semicolon:
    return stream << "SEMICOLON";
  case TokenType::Slash:
    return stream << "SLASH";
  case TokenType::Star:
    return stream << "STAR";

  case TokenType::Bang:
    return stream << "BANG";
  case TokenType::BangEqual:
    return stream << "BANG_EQUAL";
  case TokenType::Equal:
    return stream << "EQUAL";
  case TokenType::EqualEqual:
    return stream << "EQUAL_EQUAL";
  case TokenType::Greater:
    return stream << "GREATER";
  case TokenType::GreaterEqual:
    return stream << "GREATER_EQUAL";
  case TokenType::Less:
    return stream << "LESS";
  case TokenType::LessEqual:
    return stream << "LESS_EQUAL";

  case TokenType::Identifier:
    return stream << "IDENTIFIER";
  case TokenType::String:
    return stream << "STRING";
  case TokenType::Number:
    return stream << "NUMBER";

  case TokenType::And:
    return stream << "AND";
  case TokenType::Class:
    return stream << "CLASS";
  case TokenType::Else:
    return stream << "ELSE";
  case TokenType::False:
    return stream << "FALSE";
  case TokenType::Fun:
    return stream << "FUN";
  case TokenType::For:
    return stream << "FOR";
  case TokenType::If:
    return stream << "IF";
  case TokenType::Nil:
    return stream << "NIL";
  case TokenType::Or:
    return stream << "OR";
  case TokenType::Print:
    return stream << "PRINT";
  case TokenType::Return:
    return stream << "RETURN";
  case TokenType::Super:
    return stream << "SUPER";
  case TokenType::This:
    return stream << "THIS";
  case TokenType::True:
    return stream << "TRUE";
  case TokenType::Var:
    return stream << "VAR";
  case TokenType::While:
    return stream << "WHILE";

  case TokenType::Eof:
    return stream << "EOF";
  }

  return stream << "<unknown token>";
}

} // namespace jlox