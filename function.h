#ifndef _FUNCTION_H_
#define _FUNCTION_H_

#include "bullet_train.h"
#include "context.h"
#include "value.h"

/* VM instructions */
enum {
    OP_LOAD,
    OP_LOADBOOL,
    OP_NEWSTRUCT,
    OP_GETSTRUCT,
    OP_SETSTRUCT,
    OP_MOVE,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_NEG,
    OP_NOT,
    OP_EQUAL,
    OP_LEQUAL,
    OP_LESS,
    OP_TEST,
    OP_JUMP,
    OP_PRINT,
    OP_RETURN
};

/*
** Instructions are 32 bits long.
** They have a rather odd format compared to other languages:
** - 6 bits for the opcode
** - 1 bit indicating if arg B refers to a register or a constant
** - 1 bit indicating if arg C refers to a register or a constant
** - 8 bits for arg A, arg B, and arg C
** | opcode |kb|kc|   arg A    |   arg B   |   arg C   |
** OR
** - 16 bits for arg BX
** | opcode |kb|kc|   arg A    |        arg BX         |
** Note that kb and kc are not used in every instruction.
*/
typedef unsigned long Instruction;

/* Function types */
typedef enum {
    FT_FUNC, // Normal function
    FT_TASK, // Function that kicks off into a new thread
    FT_GEN   // A "traditional" coroutine
} FuncType;

/*
** Information about a function.
** Not callable on its own -- needs to be part of a bt_Closure
** so that variables in a higher scope are available.
*/
struct bt_Function {
    Instruction* program;
    bt_Value* constants;
    Key** keys;
    int params; // Number of parameters
    int registers; // Number of registers needed by this function
    FuncType type; // Type of function (func, task, or gen)
};

#endif