#include "token.hpp"

namespace jlox {

namespace {

void printLiteral(std::ostream &stream, const Literal &literal) {
  std::visit(
      [&stream](const auto &value) {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, std::monostate>) {
          stream << "nil";
        } else {
          stream << value;
        }
      },
      literal);
}

} // namespace

std::ostream &operator<<(std::ostream &stream, const Token &token) {
  stream << static_cast<int>(token.type) << ' ' << token.lexeme << ' ';

  printLiteral(stream, token.literal);

  return stream;
}

} // namespace jlox