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
    int reg; // Current register
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
    p->reg = 0;
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

/* Returns the index of an empty instruction to be set later */
static inline int reserve(Parser* p)
{
    addop(p, 0);
    return p->ps - 1;
}

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

/* Ensures the function's register count is at least [s] */
#define checkreg(p, s) if (s > p->fn->registers) p->fn->registers = s

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
    l->idx = p->reg++;
    l->prev = p->locals;
    l->scope = 0;
    p->locals = l;
    checkreg(p, p->reg); // Ensure there are enough registers
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
    int k;  // Is a cnstant?
} ExpData;

/* Some shortcuts */
#define argkc(e) ((e.k << 7) | (e.idx << 24))
#define argkb(e) ((e.k << 6) | (e.idx << 16))
#define argbx(b) ((b) << 16)
#define arga(d)  ((d) << 8)
#define expdata(i, k) (ExpData) { (i), (k) }

static ExpData exprclimb(Parser* p, int min, int dest);

/*
** Smallest unit of parsing.
** Literals, function calls, things in parentheses
*/
static ExpData atom(Parser* p, int dest)
{
    switch (lex_next(p->lx))
    {
        case TK_NUMBER:
            return expdata(addnumber(p, lex_getnumber(p->lx)), 1);
        case TK_ID: {
            const char* name = lex_gettext(p->lx);
            Local* l = findlocal(p, name);
            return expdata(l->idx, 0);
        }
        case '(': {
            ExpData e = exprclimb(p, 0, dest);
            expect(p, ')');
            return e;
        }
        case '-': {
            ExpData e = atom(p, dest);
            addop(p, OP_NEG | arga(dest) | argkc(e));
            return expdata(dest, 0);
        }
        default: // Error
            return expdata(0, 0);
    }
}

/*
** Mathematical/logical expression.
** Uses the precedence climbing algorithm.
** [dest] is the preferred register the expression should route to.
*/
static ExpData exprclimb(Parser* p, int min, int dest)
{
    ExpData lhs = atom(p, dest);
    for (;;) {
        int prec;
        Instruction inst;
        switch (lex_peek(p->lx))
        {
            case '*': prec = 6; inst = OP_MUL; break;
            case '/': prec = 6; inst = OP_DIV; break;
            case '+': prec = 5; inst = OP_ADD; break;
            case '-': prec = 5; inst = OP_SUB; break;
            default: return lhs;
        }
        if (prec >= min) {
            lex_next(p->lx);
            checkreg(p, dest);
            ExpData rhs = exprclimb(p, prec + 1, dest + 1);
            addop(p, inst | arga(dest) | argkb(lhs) | argkc(rhs));
            lhs = expdata(dest, 0);
        } else
            return lhs;
    }
}
static inline ExpData expression(Parser* p) { return exprclimb(p, 0, p->reg); }

/*
** Variable set, declaration, or function call
*/
static void varstmt(Parser* p)
{
    const char* name = lex_gettext(p->lx);
    Local* l = findlocal(p, name);
    if (l == NULL) { // Variable doesn't exist yet
        l = newlocal(p, name);
    }
    expect(p, '=');
    ExpData e = exprclimb(p, 0, l->idx);
    if (e.k) {
        addop(p, OP_LOAD | arga(l->idx) | argbx(e.idx));
    } else if (e.idx != l->idx) {
        addop(p, OP_MOVE | arga(l->idx) | argbx(e.idx));
    }
} 

static void statement(Parser* p)
{
    switch (lex_next(p->lx))
    {
        case TK_ID: {
            varstmt(p);
            break;
        }
        case TK_PRINT: {
            ExpData e = expression(p);
            addop(p, OP_PRINT | argkc(e));
            break;
        }
        default: // ERROR
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
    bt_Function* fn = finalize(&p);
/*
    for (int i = 0; i != p.ps; ++i) {
        int op = fn->program[i];
        printf("%i: %i %i%i %i %i %i\n", i, op & 0x3f, (op >> 6) & 1, (op >> 7) & 1, (op >> 8) & 0xff, (op >> 16) & 0xff, (op >> 24));
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