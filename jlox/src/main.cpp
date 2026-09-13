#include "essentials.hpp"
#include "lox.hpp"

int main(int argc, char *argv[]) {
  if (argc > 2) {
    JLOX_ERROR("Usage: jlox [script]");
    return 64;
  }

  if (argc == 2) {
    return jlox::Lox::RunFile(argv[1]);
  }

  jlox::Lox::RunPrompt();
  return 0;
}