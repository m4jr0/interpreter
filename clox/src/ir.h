#ifndef CLOX_IR_H
#define CLOX_IR_H

#include "chunk.h"
#include "common.h"

typedef int IRBlockId;
typedef int IRValue;

#define IR_NO_BLOCK (-1)
#define IR_NO_VALUE (-1)

typedef enum {
  IR_CONSTANT,
  IR_NIL,
  IR_TRUE,
  IR_FALSE,
  IR_POP,
  IR_LOAD_LOCAL,
  IR_STORE_LOCAL,
  IR_LOAD_GLOBAL,
  IR_DEFINE_GLOBAL,
  IR_STORE_GLOBAL,
  IR_LOAD_UPVALUE,
  IR_STORE_UPVALUE,
  IR_GET_PROPERTY,
  IR_SET_PROPERTY,
  IR_GET_SUPER,
  IR_INVOKE,
  IR_SUPER_INVOKE,
  IR_EQUAL,
  IR_GREATER,
  IR_LESS,
  IR_ADD,
  IR_SUBTRACT,
  IR_MULTIPLY,
  IR_DIVIDE,
  IR_NOT,
  IR_NEGATE,
  IR_PRINT,
  IR_JUMP,
  IR_BRANCH,
  IR_CALL,
  IR_CLOSURE,
  IR_CLOSE_UPVALUE,
  IR_RETURN,
  IR_CLASS,
  IR_INHERIT,
  IR_METHOD
} IROp;

typedef struct {
  bool isLocal;
  uint8_t index;
} IRUpvalueCapture;

typedef struct {
  IROp op;
  IRValue result;
  IRValue *inputs;
  int inputCount;
  int operand;
  int operand2;
  IRUpvalueCapture *captures;
  int captureCount;
  int line;
  IRBlockId target;
  IRBlockId fallthrough;
  bool removed;
} IRInstruction;

typedef struct {
  IRBlockId id;
  int firstInstruction;
  int instructionCount;
  IRBlockId *predecessors;
  int predecessorCount;
  int predecessorCapacity;
  IRBlockId *successors;
  int successorCount;
  int successorCapacity;
  IRValue *entryValues;
  IRValue *parameters;
  int stackDepth;
} IRBlock;

typedef struct {
  IRInstruction *instructions;
  int instructionCount;
  int instructionCapacity;
  IRBlock *blocks;
  int blockCount;
  int blockCapacity;
  IRBlockId entry;
  int nextValue;
} IRFunction;

void initIRFunction(IRFunction *function);
void freeIRFunction(IRFunction *function);
int currentIRPosition(const IRFunction *function);
void emitIRInstruction(IRFunction *function, IROp op, int line);
void emitIROperand(IRFunction *function, IROp op, int operand, int line);
void emitIRInvoke(IRFunction *function, IROp op, int name, int argumentCount,
                  int line);
int emitIRJump(IRFunction *function, IROp op, int line);
void patchIRJump(IRFunction *function, int instruction, int target);
void emitIRClosure(IRFunction *function, int constant,
                   const IRUpvalueCapture *captures, int captureCount,
                   int line);
void sealIRFunction(IRFunction *function);
bool buildIRValues(IRFunction *function, int initialStackDepth);
void rebuildIRControlFlow(IRFunction *function);
bool lowerIRToChunk(const IRFunction *function, const Chunk *constants,
                    Chunk *out);
void printIRFunction(const IRFunction *function, const Chunk *constants,
                     const char *name);

#endif
