#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "lex.h"


struct Lexer {
    const char* current; /* Current character */
    char buffer[128]; /* Buffer that stores identifiers, strings, numbers, etc. */
    int lookahead; /* Peeked token */
    int line; /* Line number */
};


Lexer* lex_new(const char* src)
{
    Lexer* lx = malloc(sizeof(Lexer));
    lx->current = src;
    lx->lookahead = -1;
    lx->line = 0;
    return lx;
}


void lex_free(Lexer* lx)
{
    free(lx);
}

/*
** ============================================================
** Actual lexer stuff
** ===========================================================
*/

static int scannumber(Lexer* lx)
{
    char* buf = lx->buffer;
    do {
        *buf++ = *lx->current++;
    } while (isdigit(*lx->current));
    // Decimal place?
    if (*lx->current == '.') {
        do {
            *buf++ = *lx->current++;
        } while (isdigit(*lx->current));  
    }
    *buf = 0; // Null terminator
    return TK_NUMBER;
}

/*
** Scans a keyword, identifier, or boolean literal
*/
static int scanident(Lexer* lx)
{
    char* buf = lx->buffer;
    do {
        *buf++ = *lx->current++;
    } while (isalnum(*lx->current) || *lx->current == '_');
    *buf = 0; // Null terminator

    switch (lx->buffer[0])
    {
        case 'e':
            if (0 == strcmp(lx->buffer, "else"))  return TK_ELSE;
            if (0 == strcmp(lx->buffer, "elif"))  return TK_ELIF;
        case 'f':
            if (0 == strcmp(lx->buffer, "false")) return TK_FALSE;
        case 'i':
            if (0 == strcmp(lx->buffer, "if"))    return TK_IF;
        case 'n':
            if (0 == strcmp(lx->buffer, "nil"))   return TK_NIL;
        case 'p':
            if (0 == strcmp(lx->buffer, "print")) return TK_PRINT;
        case 'r':
            if (0 == strcmp(lx->buffer, "ret"))   return TK_RET;
        case 't':
            if (0 == strcmp(lx->buffer, "true"))  return TK_TRUE;
            if (0 == strcmp(lx->buffer, "task"))  return TK_TASK;
        case 'w':
            if (0 == strcmp(lx->buffer, "while")) return TK_WHILE;
    }

    return TK_ID;
}

/*
** Scans the next token
*/
int lex_next(Lexer* lx)
{
    if (lx->lookahead != -1) {
        int temp = lx->lookahead;
        lx->lookahead = -1;
        return temp;
    }

Retry:
    switch (*lx->current)
    {
        case '\0':
            return TK_EOF;

        case '\n': case '\r':
            ++lx->line;
        case ' ': case '\t':
            ++lx->current; // Skip whitespace
            goto Retry;

        case '=':
            if (*(++lx->current) == '=') {
                ++lx->current;
                return TK_EQ;
            }
            return '=';
        
        case '!':
            if (*(++lx->current) == '=') {
                ++lx->current;
                return TK_NE;
            }
            return '!';

        case '<':
            if (*(++lx->current) == '=') {
                ++lx->current;
                return TK_LE;
            }
            return '<';

        case '>':
            if (*(++lx->current) == '=') {
                ++lx->current;
                return TK_ME;
            }
            return '>';
        
        case '&':
            if (*(++lx->current) == '&') {
                ++lx->current;
                return TK_AND;
            }
            return '&';
         
        case '|':
            if (*(++lx->current) == '|') {
                ++lx->current;
                return TK_OR;
            }
            return '|';
        
        case '-':
            // Negative number literal?
            if (isdigit(*(lx->current + 1))) { 
                return scannumber(lx);
            }
            ++lx->current;
            return '-';

        default:
            if (isdigit(*lx->current)) {
                return scannumber(lx);
            } else if (isalpha(*lx->current) || *lx->current == '_') {
                return scanident(lx);
            } else {
                return *lx->current++; // Single character token
            }
    }
}

/*
** Scans, but doesn't consume the next token
*/
int lex_peek(Lexer* lx)
{
    if (lx->lookahead == -1) {
        lx->lookahead = lex_next(lx);
    }
    return lx->lookahead;
}


BT_NUMBER lex_getnumber(Lexer* lx)
{
    return (BT_NUMBER)atof(lx->buffer);
}

const char* lex_gettext(Lexer* lx)
{
    return lx->buffer;
}