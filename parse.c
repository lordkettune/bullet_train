#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bullet_train.h"
#include "value.h"
#include "lex.h"
#include "function.h"

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
    int regsize; // Number of registers in use
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
    p->locals = NULL;
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

/* Returns the index of an empty instruction to be set later */
static inline int reserve(Parser* p)
{
    addop(p, 0);
    return p->ps - 1;
}

/* Shortcuts for creating instructions */
#define rkc(e) ((e.k << 7) | (e.idx << 24))
#define rkb(e) ((e.k << 6) | (e.idx << 16))

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

static inline int addnumber(Parser* p, BT_NUMBER number)
{
    return adddata(p, (FuncData) {
        .value = (bt_Value) {
            .number = number,
            .type = VT_NUMBER
        }
    });
}

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

/* Creates a new local and adds it to the parser's list */
static Local* newlocal(Parser* p, const char* name)
{
    Local* l = malloc(sizeof(Local) + strlen(name) + 1);
    strcpy(l->name, name);
    Local* last = p->locals;
    l->idx = last ? last->idx + 1 : 0;
    l->prev = last;
    l->scope = 0;
    p->locals = l;
    ++p->regsize;
    return l;
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
** Recursive descent parser stuff
** ============================================================
*/

typedef struct {
    int idx; // Index of register/constant
    int k;  // Is a constant?
} ExpData;

#define expdata(i, k) (ExpData) { (i), (k) }

static inline ExpData expression(Parser* p);

/*
** Smallest unit of parsing.
** Literals, function calls, things in parentheses
*/
static ExpData atom(Parser* p)
{
    switch (lex_next(p->lx))
    {
        case TK_NUMBER:
            return expdata(addnumber(p, lex_getnumber(p->lx)), 1);
    }
    return expdata(0, 0);
}

/*
** Mathematical/logical expression.
** Uses the precedence climbing algorithm.
*/
static ExpData exprclimb(Parser* p, int min)
{
    ExpData lhs = atom(p);
    return lhs;
}

static inline ExpData expression(Parser* p) { return exprclimb(p, 0); }


static void statement(Parser* p)
{
    switch (lex_next(p->lx))
    {
        case TK_PRINT: {
            ExpData e = expression(p);
            addop(p, OP_PRINT | rkc(e));
            break;
        }

        default:
            break;
    }
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

    bt_Function* fn = p.fn;

    for (int i = 0; i != p.ps; ++i) {
        int op = fn->program[i];
        printf("%i: %i %i%i %i %i %i\n", i, op & 0x3f, (op >> 6) & 1, (op >> 7) & 1, (op >> 8) & 0xff, (op >> 16) & 0xff, (op >> 24));
    }

    return fn;
}

/* Loads a file and compiles it */
BT_API bt_Function* bt_fcompile(bt_Context* bt, const char* path)
{
    int size;
    FILE* file = fopen(path, "r");
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    rewind(file);
    char buffer[size + 1];
    fread(buffer, 1, size, file);
    fclose(file);
    buffer[size] = 0;
    return bt_compile(bt, buffer);
}