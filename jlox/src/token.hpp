#pragma once

#include <cstddef>
#include <ostream>
#include <string_view>
#include <variant>

#include "token_type.hpp"

namespace jlox {

using Literal = std::variant<std::monostate, double, std::string_view>;

struct Token {
  TokenType type;
  std::string_view lexeme;
  Literal literal;
  std::size_t line = 0;
};

std::ostream &operator<<(std::ostream &stream, const Token &token);

} // namespace jlox