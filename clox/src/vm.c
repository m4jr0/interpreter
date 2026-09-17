#include "vm.h"

#include <stdio.h>

#include "debug.h"
#include "scanner.h"

VM vm;

static void resetStack(void) { vm.stackTop = vm.stack; }

void initVM(void) { resetStack(); }

void freeVM(void) {}

void push(Value value) {
  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop(void) {
  vm.stackTop--;
  return *vm.stackTop;
}

static InterpretResult run(void) {
#define READ_BYTE() (*vm.ip++)

#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    printf("          ");

    for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }

    printf("\n");

    disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif

    uint8_t instruction;

    switch (instruction = READ_BYTE()) {
    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      push(constant);
      break;
    }

    case OP_RETURN: {
      printValue(pop());
      printf("\n");

      return INTERPRET_OK;
    }

    default:
      return INTERPRET_RUNTIME_ERROR;
    }
  }

#undef READ_BYTE
#undef READ_CONSTANT
}

InterpretResult interpret(const char *source) {
  initScanner(source);

  int line = -1;

  for (;;) {
    Token token = scanToken();

    if (token.line != line) {
      printf("%4d ", token.line);
      line = token.line;
    } else {
      printf("   | ");
    }

    printf("%2d '%.*s'\n", token.type, token.length, token.start);

    if (token.type == TOKEN_EOF) {
      break;
    }
  }

  return INTERPRET_OK;
}