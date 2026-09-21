#ifndef CLOX_STACKIFIER_H
#define CLOX_STACKIFIER_H

#include "chunk.h"
#include "ir.h"

typedef struct {
  IROp op;
  int operand;
  int operand2;
  IRUpvalueCapture *captures;
  int captureCount;
  int line;
  IRBlockId target;
  IRBlockId fallthrough;
} IRStackInstruction;

typedef struct {
  IRStackInstruction *instructions;
  int instructionCount;
  int instructionCapacity;
  int *blockFirstInstruction;
  int *blockInstructionCount;
  int blockCount;
} IRStackFunction;

void initIRStackFunction(IRStackFunction *function);
void freeIRStackFunction(IRStackFunction *function);
bool stackifyIR(const IRFunction *function, IRStackFunction *out);
bool lowerStackFunctionToChunk(const IRStackFunction *function,
                               const Chunk *constants, Chunk *out);

#endif
