#pragma once

#include <string>
#include <variant>

namespace jlox {

using Value = std::variant<std::monostate, double, std::string, bool>;

} // namespace jlox