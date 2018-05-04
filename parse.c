#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bullet_train.h"
#include "value.h"
#include "lex.h"
#include "function.h"

#define MAX_PATCHES 32

typedef struct Local Local;

struct Local {
    Local* prev;
    int scope;
    int idx;
    char name[];
};

typedef struct {
    bt_Context* ctx;
    bt_Function* fn;
    Lexer* lx;
    // Hideous vector counters. Not much I can do since this ain't C++
    int ps, pr; // Program size, program reserved
    int ds, dr; // Data size, data reserved
    int emptyreg; // Index of first empty register
    Local* locals;
} Parser;

static void initparser(Parser* p, bt_Context* ctx, Lexer* lx)
{
    bt_Function* fn = malloc(sizeof(bt_Function));
    fn->program = malloc(sizeof(Instruction) * 4);
    fn->data = malloc(sizeof(FuncData) * 4);
    fn->params = 0;
    fn->registers = 0;
    fn->type = FT_FUNC;
    p->fn = fn;
    p->lx = lx;
    p->ctx = ctx;
    p->ps = 0; p->pr = 4;
    p->ds = 0; p->dr = 4;
    p->emptyreg = 0;
    p->locals = NULL;
}

/*
** Shrinks the function's vectors and deallocates locals.
** Returns the finalized function.
*/
static bt_Function* finalize(Parser* p)
{
    bt_Function* fn = p->fn;
    fn->program = realloc(fn->program, sizeof(Instruction) * p->ps);
    fn->data = realloc(fn->data, sizeof(FuncData) * p->ds);
    Local* l = p->locals;
    while (l != NULL) {
        Local* temp = l->prev;
        free(l);
        l = temp;
    }
    return fn;
}

/* Adds an instruction to the result */
static void addop(Parser* p, Instruction i)
{
    bt_Function* fn = p->fn;
    fn->program[p->ps++] = i;
    if (p->ps == p->pr) {
        p->pr *= 2;
        fn->program = realloc(fn->program, sizeof(Instruction) * p->pr);
    }
}

/* Some shortcuts */
#define arga(a)  ((a) << 8)
#define argb(b)  ((b) << 16)
#define argc(c)  ((c) << 24)

/* Returns the index of an empty instruction to be set later */
static inline int reserve(Parser* p)
{
    addop(p, 0);
    return p->ps - 1;
}

#define setreserved(p, i, op) p->fn->program[i] = (op)

/*
** Sets arg A in the previous instruction.
** Arg A is used as the destination register in every instruction
** that has one, so this is safe to do.
*/
#define setdest(p, d) p->fn->program[p->ps - 1] |= ((d) << 8)

/* Adds an FData to the result, returning the index of the item */
static int adddata(Parser* p, FuncData d)
{
    bt_Function* fn = p->fn;
    fn->data[p->ds++] = d;
    if (p->ds == p->dr) {
        p->dr *= 2;
        fn->data = realloc(fn->data, sizeof(FuncData) * p->dr);
    }
    return p->ds - 1;
}

#define addconstant(p, c) (adddata(p, (FuncData) { .value = (c) }))

/*
** Searches for a local variable in the parser's list.
** Returns NULL if it doesn't exist.
*/
static Local* findlocal(Parser* p, const char* name)
{
    Local* l = p->locals;
    while (l != NULL) {
        if (strcmp(l->name, name) == 0) {
            return l;
        }
        l = l->prev;
    }
    return NULL;
}

/*
** Creates a new local and adds it to the parser's list
** Returns the index of the local's register
*/
static int newlocal(Parser* p, const char* name)
{
    Local* l = malloc(sizeof(Local) + strlen(name) + 1);
    strcpy(l->name, name);
    l->idx = p->emptyreg; // New locals use the first empty register
    l->prev = p->locals;
    l->scope = 0;
    p->locals = l;
    return l->idx;
}

#define NO_PATCHES -1
#define LAST_PATCH 0

static void addpatch(Parser* p, int* list)
{
    addop(p, *list == NO_PATCHES ? LAST_PATCH : *list);
    *list = p->ps - 1;
}

static void patchhere(Parser* p, int* list)
{
    int l = *list;
    if (l != NO_PATCHES) {
        int op;
        do {
            op = p->fn->program[l];
            p->fn->program[l] = OP_JUMP | argb(p->ps);
            l = op;
        } while (op != LAST_PATCH);
        *list = NO_PATCHES;
    }
}

static void patchbool(Parser* p, int* list, int dest, int b)
{
    int l = *list;
    if (l != NO_PATCHES) {
        Instruction ins = OP_LOADBOOL | arga(dest) | argb(b);
        int op;
        do {
            op = p->fn->program[l];
            p->fn->program[l] = ins | argc(p->ps - l - 1);
            l = op;
        } while (op != LAST_PATCH);
        *list = NO_PATCHES;
    }
}

/* Errors if the next token isn't [tk] */
static void expect(Parser* p, int tk)
{
    if (lex_next(p->lx) != tk) {

    }
}

/* If the next token is [tk], advances the lexer and returns true */
static int accept(Parser* p, int tk)
{
    if (lex_peek(p->lx) == tk) {
        lex_next(p->lx);
        return 1;
    }
    return 0;
}

/*
** ============================================================
** Expression parsing
** ============================================================
*/

// Expression types
enum {
    EX_CONST, // Constant
    EX_REG, // Register (usually a local variable)
    EX_ROUTE, // Instruction that can be routed directly to a register
    EX_TRUE, // Boolean true
    EX_FALSE, // Boolean false
    EX_LOGIC // Logical/comparison operators
};

typedef struct {
    union {
        bt_Value value;
        int reg; // Register
    };
    int t, f; // Patch lists
    int type;
} ExpData;

static void initexp(ExpData* e, int t)
{
    e->t = e->f = NO_PATCHES;
    e->type = t;
}

static void exprclimb(Parser* p, ExpData* lhs, int min);

static inline void expression(Parser* p, ExpData* e)
{
    exprclimb(p, e, 0);
}

/* Routes the result of an expression to register [dest] */
static void route(Parser* p, ExpData* e, int dest)
{
    switch (e->type)
    {
        case EX_CONST: {
            int idx = addconstant(p, e->value);
            addop(p, OP_LOAD | arga(dest) | argb(idx));
            break;
        }
        case EX_REG:
            if (e->reg != dest) {
                addop(p, OP_MOVE | arga(dest) | argb(e->reg));
            }
            break;
        case EX_ROUTE:
            setdest(p, dest);
            break;
        case EX_TRUE:
            addop(p, OP_LOADBOOL | arga(dest) | argb(1));
            break;
        case EX_FALSE:
            addop(p, OP_LOADBOOL | arga(dest) | argb(0));
            break;
        case EX_LOGIC:
            // Instructions after last comparison
            addop(p, OP_LOADBOOL | arga(dest) | argb(0) | argc(1));
            addop(p, OP_LOADBOOL | arga(dest) | argb(1));
            // Close earlier patches
            patchbool(p, &e->f, dest, 0);
            patchbool(p, &e->t, dest, 1);
            break;
    }
}

static void toargk(Parser* p, ExpData* e, int* k, int* idx)
{
    if (e->type == EX_CONST) {
        *k = 1;
        *idx = addconstant(p, e->value);
    } else {
        *k = 0;
        if (e->type == EX_REG) {
            *idx = e->reg;
        } else {
            route(p, e, p->emptyreg);
            *idx = p->emptyreg;
        }
    }
}

static int argkb(Parser* p, ExpData* e)
{
    int k, idx;
    toargk(p, e, &k, &idx);
    return (k << 6) | (idx << 16);
}

static int argkc(Parser* p, ExpData* e)
{
    int k, idx;
    toargk(p, e, &k, &idx);
    return (k << 7) | (idx << 24);
}

/*
** Ensures that an expression is logical (comparison operators)
** If not, adds an OP_TEST to the end.
*/
static void checklogic(Parser* p, ExpData* e)
{
    if (e->type != EX_LOGIC) {
        addop(p, OP_TEST | arga(1) | argkc(p, e));
    }
}

/* Reverses the last logical instruction */
static void invert(Parser* p)
{
    Instruction ins = p->fn->program[p->ps - 1];
    int b = !((ins >> 8) & 0xFF);
    ins = (ins & ~(0xFF << 8)) | (b << 8);
    p->fn->program[p->ps - 1] = ins;
}

/*
** Smallest unit of parsing.
** Literals, function calls, things in parentheses.
*/
static void atom(Parser* p, ExpData* e)
{
    switch (lex_next(p->lx))
    {
        case TK_NUMBER:
            initexp(e, EX_CONST);
            e->value = (bt_Value) { .number = lex_getnumber(p->lx), .type = VT_NUMBER };
            break;
        case TK_ID: {
            initexp(e, EX_REG);
            const char* name = lex_gettext(p->lx);
            Local* l = findlocal(p, name);
            e->reg = l->idx;
            break;
        }
        case '!': {
            atom(p, e);
            addop(p, OP_NOT | argkc(p, e));
            e->type = EX_ROUTE;
            break;
        }
        case '-': {
            atom(p, e);
            addop(p, OP_NEG | argkc(p, e));
            e->type = EX_ROUTE;
            break;
        }
        case TK_TRUE: initexp(e, EX_TRUE); break;
        case TK_FALSE: initexp(e, EX_FALSE); break;
        case '(': expression(p, e); expect(p, ')'); break;
        default: // Error
            break;
    }
}

// You should probably find a more elegent way to do this my dude
enum OpType {
    OPT_BIN,
    OPT_AND,
    OPT_OR
};

#define T arga(1)
#define F 0

// THIS SUCKS FIX IT YOU DINGUS

/*
** Mathematical/logical expression.
** Uses precedence climbing.
*/
static void exprclimb(Parser* p, ExpData* lhs, int min)
{
    atom(p, lhs);
    for (;;) {
        int prec = 0, ex = 0;
        enum OpType ty = OPT_BIN;
        Instruction inst = 0;
        switch (lex_peek(p->lx))
        {
            case '*':    prec = 6; inst = OP_MUL; ex = EX_ROUTE; break;
            case '/':    prec = 6; inst = OP_DIV; ex = EX_ROUTE; break;
            case '+':    prec = 5; inst = OP_ADD; ex = EX_ROUTE; break;
            case '-':    prec = 5; inst = OP_SUB; ex = EX_ROUTE; break;
            case '>':    prec = 4; inst = OP_LEQUAL | F; ex = EX_LOGIC; break;
            case '<':    prec = 4; inst = OP_LESS   | T; ex = EX_LOGIC; break;
            case TK_ME:  prec = 4; inst = OP_LESS   | F; ex = EX_LOGIC; break;
            case TK_LE:  prec = 4; inst = OP_LEQUAL | T; ex = EX_LOGIC; break; 
            case TK_EQ:  prec = 3; inst = OP_EQUAL  | T; ex = EX_LOGIC; break;
            case TK_NE:  prec = 3; inst = OP_EQUAL  | F; ex = EX_LOGIC; break;
            case TK_AND: ty = OPT_AND; break;
            case TK_OR:  ty = OPT_OR;  break;
            default: return;
        }
        if (prec >= min) {
            lex_next(p->lx);
            ExpData rhs;
            if (ty == OPT_AND) {
                checklogic(p, lhs);
                addpatch(p, &lhs->f);
                patchhere(p, &lhs->t);
                exprclimb(p, &rhs, 3);
                lhs->t = rhs.t;
                checklogic(p, &rhs);
                lhs->type = EX_LOGIC;
            } else if (ty == OPT_OR) {
                checklogic(p, lhs);
                invert(p);
                addpatch(p, &lhs->t);
                patchhere(p, &lhs->f);
                exprclimb(p, &rhs, 2);
                lhs->f = rhs.f;
                checklogic(p, &rhs);
                lhs->type = EX_LOGIC;
            } else {
                int kb = argkb(p, lhs);
                ++p->emptyreg;
                exprclimb(p, &rhs, prec + 1);
                addop(p, inst | kb | argkc(p, &rhs)); // Destination will be set later
                --p->emptyreg;
                lhs->type = ex;
            }
        } else
            return;
    }
}

/*
** ============================================================
** Statement parsing
** ============================================================
*/

static void block(Parser* p);

/* Variable set, declaration, or function call */
static void varstmt(Parser* p)
{
    const char* name = lex_gettext(p->lx);
    Local* l = findlocal(p, name);
    int dest = l == NULL ? newlocal(p, name) : l->idx; // Local undeclared?
    expect(p, '=');
    ExpData e;
    expression(p, &e);
    route(p, &e, dest);
    if (l == NULL) {
        ++p->emptyreg; // Empty register now in use by new local
    }
}

/* if statement */
static void ifstmt(Parser* p)
{
    ExpData e;
    expression(p, &e);
    checklogic(p, &e);
    int ins = reserve(p);
    patchhere(p, &e.t);
    block(p);
    if (accept(p, TK_ELSE)) {
        setreserved(p, ins, OP_JUMP | argb(p->ps + 1));
        ins = reserve(p);
        patchhere(p, &e.f);
        block(p);
        setreserved(p, ins, OP_JUMP | argb(p->ps));
    } else if (accept(p, TK_ELIF)) {
        setreserved(p, ins, OP_JUMP | argb(p->ps + 1));
        ins = reserve(p);
        patchhere(p, &e.f);
        ifstmt(p);
        setreserved(p, ins, OP_JUMP | argb(p->ps));
    } else {
        setreserved(p, ins, OP_JUMP | argb(p->ps));
        patchhere(p, &e.f);
    }
}

static void whileloop(Parser* p)
{
    ExpData e;
    int start = p->ps;
    expression(p, &e);
    checklogic(p, &e);
    int ins = reserve(p);
    patchhere(p, &e.t);
    block(p);
    addop(p, OP_JUMP | argb(start));
    setreserved(p, ins, OP_JUMP | argb(p->ps));
    patchhere(p, &e.f);
}

static void statement(Parser* p)
{
    switch (lex_next(p->lx))
    {
        case TK_ID: varstmt(p); break;
        case TK_IF: ifstmt(p); break;
        case TK_WHILE: whileloop(p); break;
        case TK_PRINT: {
            ExpData e;
            expression(p, &e);
            addop(p, OP_PRINT | argkc(p, &e));
            break;
        }
        default: // ERROR
            break;
    }
}

/* Block of code in brackets */
static void block(Parser* p)
{
    expect(p, '{');
    while (lex_peek(p->lx) != '}') {
        statement(p);
    }
    expect(p, '}');
}

/*
** ============================================================
** API functions
** ============================================================
*/

/* Compiles a string to a bt_Function */
BT_API bt_Function* bt_compile(bt_Context* bt, const char* src)
{
    Parser p;
    Lexer* lx = lex_new(src);
    initparser(&p, bt, lx);
    while (lex_peek(lx) != TK_EOF) {
        statement(&p);
    }
    addop(&p, OP_RETURN);
    lex_free(lx);
    bt_Function* fn = finalize(&p);
/*
    for (int i = 0; i != p.ps; ++i) {
        int op = fn->program[i];
        printf("|%i| %i %i %i %i\n", i, op & 0x3f, (op >> 8) & 0xff, (op >> 16) & 0xff, (op >> 24));
    }
    printf("registers: %i\n", fn->registers);
*/
    return fn;
}

/* Loads a file and compiles it */
BT_API bt_Function* bt_fcompile(bt_Context* bt, const char* path)
{
    char buffer[512 + 1]; // BAD, FIX IT YOU FUCK
    FILE* file = fopen(path, "r");
    size_t size = fread(buffer, 1, 512, file);
    buffer[size] = 0;
    return bt_compile(bt, buffer);
}