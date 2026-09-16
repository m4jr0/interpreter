#include "common.h"

#include "chunk.h"
#include "vm.h"

int main(int argc, const char *argv[]) {
  (void)argc;
  (void)argv;

  initVM();

  Chunk chunk;
  initChunk(&chunk);

  int constant = addConstant(&chunk, 1.2);

  writeChunk(&chunk, OP_CONSTANT, 123);

  writeChunk(&chunk, (uint8_t)constant, 123);

  writeChunk(&chunk, OP_RETURN, 123);

  interpret(&chunk);

  freeVM();
  freeChunk(&chunk);

  return 0;
}