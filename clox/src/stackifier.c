#include <stdlib.h>
#include <string.h>

#include "stackifier.h"

void initIRStackFunction(IRStackFunction *function) {
  function->instructions = NULL;
  function->instructionCount = 0;
  function->instructionCapacity = 0;
  function->blockFirstInstruction = NULL;
  function->blockInstructionCount = NULL;
  function->blockCount = 0;
}

void freeIRStackFunction(IRStackFunction *function) {
  for (int i = 0; i < function->instructionCount; i++) {
    free(function->instructions[i].captures);
  }
  free(function->instructions);
  free(function->blockFirstInstruction);
  free(function->blockInstructionCount);
  initIRStackFunction(function);
}

static IRStackInstruction *appendStackInstruction(IRStackFunction *function,
                                                  const IRInstruction *source) {
  if (function->instructionCount == function->instructionCapacity) {
    int oldCapacity = function->instructionCapacity;
    function->instructionCapacity = oldCapacity < 16 ? 16 : oldCapacity * 2;
    IRStackInstruction *instructions =
        realloc(function->instructions,
                sizeof(IRStackInstruction) *
                    (size_t)function->instructionCapacity);
    if (instructions == NULL) exit(1);
    function->instructions = instructions;
  }

  IRStackInstruction *instruction =
      &function->instructions[function->instructionCount++];
  instruction->op = source->op;
  instruction->operand = source->operand;
  instruction->operand2 = source->operand2;
  instruction->captures = NULL;
  instruction->captureCount = source->captureCount;
  instruction->line = source->line;
  instruction->target = source->target;
  instruction->fallthrough = source->fallthrough;

  if (source->captureCount > 0) {
    instruction->captures =
        malloc(sizeof(IRUpvalueCapture) * (size_t)source->captureCount);
    if (instruction->captures == NULL) exit(1);
    memcpy(instruction->captures, source->captures,
           sizeof(IRUpvalueCapture) * (size_t)source->captureCount);
  }
  return instruction;
}

static bool sameValues(const IRValue *a, const IRValue *b, int count) {
  for (int i = 0; i < count; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

static bool popExpected(IRValue *stack, int *depth,
                        const IRInstruction *instruction, int count) {
  if (*depth < count || instruction->inputCount != count) return false;
  int first = *depth - count;
  if (!sameValues(stack + first, instruction->inputs, count)) return false;
  *depth -= count;
  return true;
}

static bool pushValue(IRValue **stack, int *depth, int *capacity,
                      IRValue value) {
  if (value == IR_NO_VALUE) return false;
  if (*depth == *capacity) {
    int oldCapacity = *capacity;
    *capacity = oldCapacity < 8 ? 8 : oldCapacity * 2;
    IRValue *values = realloc(*stack, sizeof(IRValue) * (size_t)*capacity);
    if (values == NULL) exit(1);
    *stack = values;
  }
  (*stack)[(*depth)++] = value;
  return true;
}

static bool stackifyInstruction(const IRInstruction *instruction,
                                IRValue **stack, int *depth, int *capacity) {
  switch (instruction->op) {
  case IR_CONSTANT:
  case IR_NIL:
  case IR_TRUE:
  case IR_FALSE:
  case IR_LOAD_GLOBAL:
  case IR_LOAD_UPVALUE:
  case IR_CLASS:
  case IR_CLOSURE:
    if (instruction->inputCount != 0) return false;
    return pushValue(stack, depth, capacity, instruction->result);

  case IR_LOAD_LOCAL:
    if (instruction->inputCount != 1 || instruction->operand < 0 ||
        instruction->operand >= *depth ||
        (*stack)[instruction->operand] != instruction->inputs[0]) {
      return false;
    }
    return pushValue(stack, depth, capacity, instruction->result);

  case IR_STORE_LOCAL:
    if (instruction->inputCount != 1 || *depth < 1 ||
        instruction->operand < 0 || instruction->operand >= *depth ||
        (*stack)[*depth - 1] != instruction->inputs[0]) {
      return false;
    }
    (*stack)[instruction->operand] = instruction->inputs[0];
    return true;

  case IR_STORE_GLOBAL:
  case IR_STORE_UPVALUE:
    return instruction->inputCount == 1 && *depth >= 1 &&
           (*stack)[*depth - 1] == instruction->inputs[0];

  case IR_DEFINE_GLOBAL:
  case IR_POP:
  case IR_PRINT:
  case IR_CLOSE_UPVALUE:
  case IR_RETURN:
    return popExpected(*stack, depth, instruction, 1);

  case IR_EQUAL:
  case IR_GREATER:
  case IR_LESS:
  case IR_ADD:
  case IR_SUBTRACT:
  case IR_MULTIPLY:
  case IR_DIVIDE:
    if (!popExpected(*stack, depth, instruction, 2)) return false;
    return pushValue(stack, depth, capacity, instruction->result);

  case IR_NOT:
  case IR_NEGATE:
  case IR_GET_PROPERTY:
    if (!popExpected(*stack, depth, instruction, 1)) return false;
    return pushValue(stack, depth, capacity, instruction->result);

  case IR_SET_PROPERTY:
    if (!popExpected(*stack, depth, instruction, 2)) return false;
    return pushValue(stack, depth, capacity, instruction->inputs[1]);

  case IR_GET_SUPER:
    if (!popExpected(*stack, depth, instruction, 2)) return false;
    return pushValue(stack, depth, capacity, instruction->result);

  case IR_CALL: {
    int count = instruction->operand + 1;
    if (!popExpected(*stack, depth, instruction, count)) return false;
    return pushValue(stack, depth, capacity, instruction->result);
  }

  case IR_INVOKE: {
    int count = instruction->operand2 + 1;
    if (!popExpected(*stack, depth, instruction, count)) return false;
    return pushValue(stack, depth, capacity, instruction->result);
  }

  case IR_SUPER_INVOKE: {
    int count = instruction->operand2 + 2;
    if (!popExpected(*stack, depth, instruction, count)) return false;
    return pushValue(stack, depth, capacity, instruction->result);
  }

  case IR_INHERIT:
  case IR_METHOD:
    if (!popExpected(*stack, depth, instruction, 2)) return false;
    return pushValue(stack, depth, capacity, instruction->inputs[0]);

  case IR_BRANCH:
    return instruction->inputCount == 1 && *depth >= 1 &&
           (*stack)[*depth - 1] == instruction->inputs[0];

  case IR_JUMP:
    return instruction->inputCount == 0;
  }
  return false;
}

static int predecessorIndex(const IRBlock *block, IRBlockId predecessor) {
  for (int i = 0; i < block->predecessorCount; i++) {
    if (block->predecessors[i] == predecessor) return i;
  }
  return -1;
}

static bool edgeMatches(const IRFunction *function, IRBlockId predecessor,
                        IRBlockId successor, const IRValue *stack, int depth) {
  if (successor < 0 || successor >= function->blockCount) return false;
  const IRBlock *block = &function->blocks[successor];
  if (block->stackDepth != depth) return false;

  int index = predecessorIndex(block, predecessor);
  if (index < 0 || block->incomingValues == NULL) return false;
  if (depth == 0) return true;
  if (block->incomingValues[index] == NULL) return false;
  return sameValues(stack, block->incomingValues[index], depth);
}

bool stackifyIR(const IRFunction *function, IRStackFunction *out) {
  if (function->blockCount == 0) return true;

  out->blockCount = function->blockCount;
  out->blockFirstInstruction =
      malloc(sizeof(int) * (size_t)out->blockCount);
  out->blockInstructionCount =
      malloc(sizeof(int) * (size_t)out->blockCount);
  if (out->blockFirstInstruction == NULL ||
      out->blockInstructionCount == NULL) {
    exit(1);
  }

  for (int i = 0; i < function->blockCount; i++) {
    const IRBlock *block = &function->blocks[i];
    out->blockFirstInstruction[i] = out->instructionCount;

    if (block->stackDepth < 0) {
      out->blockInstructionCount[i] = 0;
      continue;
    }

    int depth = block->stackDepth;
    int capacity = depth < 8 ? 8 : depth;
    IRValue *stack = malloc(sizeof(IRValue) * (size_t)capacity);
    if (stack == NULL) exit(1);
    for (int s = 0; s < depth; s++) stack[s] = block->parameters[s];

    int emitted = 0;
    for (int j = 0; j < block->instructionCount; j++) {
      const IRInstruction *instruction =
          &function->instructions[block->firstInstruction + j];
      if (instruction->removed) continue;
      if (!stackifyInstruction(instruction, &stack, &depth, &capacity)) {
        free(stack);
        return false;
      }
      appendStackInstruction(out, instruction);
      emitted++;
    }
    out->blockInstructionCount[i] = emitted;

    for (int s = 0; s < block->successorCount; s++) {
      if (!edgeMatches(function, block->id, block->successors[s], stack,
                       depth)) {
        free(stack);
        return false;
      }
    }
    free(stack);
  }
  return true;
}

static int encodedSize(const IRStackInstruction *instruction) {
  switch (instruction->op) {
  case IR_CONSTANT:
  case IR_LOAD_LOCAL:
  case IR_STORE_LOCAL:
  case IR_LOAD_GLOBAL:
  case IR_DEFINE_GLOBAL:
  case IR_STORE_GLOBAL:
  case IR_LOAD_UPVALUE:
  case IR_STORE_UPVALUE:
  case IR_GET_PROPERTY:
  case IR_SET_PROPERTY:
  case IR_GET_SUPER:
  case IR_CALL:
  case IR_CLASS:
  case IR_METHOD:
    return 2;
  case IR_INVOKE:
  case IR_SUPER_INVOKE:
  case IR_JUMP:
  case IR_BRANCH:
    return 3;
  case IR_CLOSURE:
    return 2 + instruction->captureCount * 2;
  default:
    return 1;
  }
}

static OpCode bytecodeOp(IROp op) {
  switch (op) {
  case IR_CONSTANT: return OP_CONSTANT;
  case IR_NIL: return OP_NIL;
  case IR_TRUE: return OP_TRUE;
  case IR_FALSE: return OP_FALSE;
  case IR_POP: return OP_POP;
  case IR_LOAD_LOCAL: return OP_GET_LOCAL;
  case IR_STORE_LOCAL: return OP_SET_LOCAL;
  case IR_LOAD_GLOBAL: return OP_GET_GLOBAL;
  case IR_DEFINE_GLOBAL: return OP_DEFINE_GLOBAL;
  case IR_STORE_GLOBAL: return OP_SET_GLOBAL;
  case IR_LOAD_UPVALUE: return OP_GET_UPVALUE;
  case IR_STORE_UPVALUE: return OP_SET_UPVALUE;
  case IR_GET_PROPERTY: return OP_GET_PROPERTY;
  case IR_SET_PROPERTY: return OP_SET_PROPERTY;
  case IR_GET_SUPER: return OP_GET_SUPER;
  case IR_INVOKE: return OP_INVOKE;
  case IR_SUPER_INVOKE: return OP_SUPER_INVOKE;
  case IR_EQUAL: return OP_EQUAL;
  case IR_GREATER: return OP_GREATER;
  case IR_LESS: return OP_LESS;
  case IR_ADD: return OP_ADD;
  case IR_SUBTRACT: return OP_SUBTRACT;
  case IR_MULTIPLY: return OP_MULTIPLY;
  case IR_DIVIDE: return OP_DIVIDE;
  case IR_NOT: return OP_NOT;
  case IR_NEGATE: return OP_NEGATE;
  case IR_PRINT: return OP_PRINT;
  case IR_BRANCH: return OP_JUMP_IF_FALSE;
  case IR_CALL: return OP_CALL;
  case IR_CLOSURE: return OP_CLOSURE;
  case IR_CLOSE_UPVALUE: return OP_CLOSE_UPVALUE;
  case IR_RETURN: return OP_RETURN;
  case IR_CLASS: return OP_CLASS;
  case IR_INHERIT: return OP_INHERIT;
  case IR_METHOD: return OP_METHOD;
  case IR_JUMP: return OP_JUMP;
  }
  return OP_RETURN;
}

static void writeByte(Chunk *out, int value, int line) {
  writeChunk(out, (uint8_t)value, line);
}

bool lowerStackFunctionToChunk(const IRStackFunction *function,
                               const Chunk *constants, Chunk *out) {
  initChunk(out);
  for (int i = 0; i < constants->constants.count; i++) {
    addConstant(out, constants->constants.values[i]);
  }

  int *blockOffsets = malloc(sizeof(int) * (size_t)function->blockCount);
  if (blockOffsets == NULL) exit(1);
  int offset = 0;
  for (int i = 0; i < function->blockCount; i++) {
    blockOffsets[i] = offset;
    int first = function->blockFirstInstruction[i];
    int count = function->blockInstructionCount[i];
    for (int j = 0; j < count; j++) {
      offset += encodedSize(&function->instructions[first + j]);
    }
  }

  for (int i = 0; i < function->blockCount; i++) {
    int first = function->blockFirstInstruction[i];
    int count = function->blockInstructionCount[i];
    for (int j = 0; j < count; j++) {
      const IRStackInstruction *instruction =
          &function->instructions[first + j];

      int instructionOffset = out->count;
      if (instruction->op == IR_JUMP || instruction->op == IR_BRANCH) {
        if (instruction->target < 0 ||
            instruction->target >= function->blockCount) {
          free(blockOffsets);
          freeChunk(out);
          return false;
        }
        int targetOffset = blockOffsets[instruction->target];
        bool backward = targetOffset <= instructionOffset;
        if (instruction->op == IR_BRANCH && backward) {
          free(blockOffsets);
          freeChunk(out);
          return false;
        }
        OpCode op = instruction->op == IR_BRANCH
                        ? OP_JUMP_IF_FALSE
                        : backward ? OP_LOOP : OP_JUMP;
        int distance = backward ? instructionOffset + 3 - targetOffset
                                : targetOffset - instructionOffset - 3;
        if (distance < 0 || distance > UINT16_MAX) {
          free(blockOffsets);
          freeChunk(out);
          return false;
        }
        writeByte(out, op, instruction->line);
        writeByte(out, (distance >> 8) & 0xff, instruction->line);
        writeByte(out, distance & 0xff, instruction->line);
        continue;
      }

      writeByte(out, bytecodeOp(instruction->op), instruction->line);
      switch (instruction->op) {
      case IR_CONSTANT:
      case IR_LOAD_LOCAL:
      case IR_STORE_LOCAL:
      case IR_LOAD_GLOBAL:
      case IR_DEFINE_GLOBAL:
      case IR_STORE_GLOBAL:
      case IR_LOAD_UPVALUE:
      case IR_STORE_UPVALUE:
      case IR_GET_PROPERTY:
      case IR_SET_PROPERTY:
      case IR_GET_SUPER:
      case IR_CALL:
      case IR_CLASS:
      case IR_METHOD:
        writeByte(out, instruction->operand, instruction->line);
        break;
      case IR_INVOKE:
      case IR_SUPER_INVOKE:
        writeByte(out, instruction->operand, instruction->line);
        writeByte(out, instruction->operand2, instruction->line);
        break;
      case IR_CLOSURE:
        writeByte(out, instruction->operand, instruction->line);
        for (int capture = 0; capture < instruction->captureCount; capture++) {
          writeByte(out, instruction->captures[capture].isLocal ? 1 : 0,
                    instruction->line);
          writeByte(out, instruction->captures[capture].index,
                    instruction->line);
        }
        break;
      default:
        break;
      }
    }
  }

  free(blockOffsets);
  return true;
}
