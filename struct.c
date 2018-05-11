#include "struct.h"
#include "context.h"

#include <stdlib.h>
#include <stdbool.h>

/* Start size for table */
#define META_BUF 7

/*
** LARGE EXPLANATION INCOMING:
** Bullet Train's structs use an approach similar to JavaScript V8's hidden classes.
**
*/
struct Metatable {
    Metatable* parent;
    Key* key;
    int idx;
    Metatable** children;
    int count;
    int size;
};

/* Creates a new root metatable */
Metatable* newrootmeta()
{
    Metatable* meta = malloc(sizeof(Metatable));
    meta->children = malloc(sizeof(Metatable*) * META_BUF);
    for (int i = 0; i != META_BUF; ++i) {
        meta->children[i] = NULL;
    }
    meta->size = META_BUF;
    meta->key = NULL;
    meta->idx = -1;
    meta->parent = NULL;
    meta->count = 0;
    return meta;
}

/* GC destructor for structs */
void destroystruct(void* st)
{
    free(((bt_Struct*)st)->data);
}

#include <stdio.h>

/* Gets an element of a struct */
bt_Value getstruct(bt_Struct* s, Key* k)
{
    Metatable* meta = s->meta;
    int i = k->hash % meta->size;
    Metatable* c = meta->children[i];
    while (c != NULL) {
        if (c->key == k) {
            return s->data[c->idx];
        }
        i = (i + 1) % meta->size;
        c = meta->children[i];
    }
    // Not found, error
    return (bt_Value) { .type = VT_NIL };
}

/* Resizes a metatable to size [s] */
static void resize(Metatable* m, int s)
{
    Metatable** c = malloc(s * sizeof(Metatable*));
    for (int i = 0; i != m->size; ++i) {
        if (m->children[i] != NULL) {
            int j = m->children[i]->key->hash % s;
            while (c[j] != NULL) {
                j = (j + 1) % s;
            }
            c[j] = m->children[i];
        }
    }
    free(m->children);
    m->children = c;
    m->size = s;
}

/*
** Sets an element of a struct.
** Checks the struct's metatable for an entry with the specified key.
** If that entry is a child, sets it as the struct's metatable.
** Creates a new child if the entry doesn't exist.
*/
void setstruct(bt_Struct* s, Key* k, bt_Value* vl)
{
    Metatable* meta = s->meta;
    int i = k->hash % meta->size;
    Metatable* c = meta->children[i];

    while (c != NULL) {
        if (c->key == k) {
            if (c->parent == s->meta) {
                s->meta = c;
                // Grow struct's array if it isn't big enough
                if (c->idx == s->size) {
                    s->size *= 2;
                    s->data = realloc(s->data, s->size * sizeof(bt_Value));
                }
            }
            s->data[c->idx] = *vl;
            return;
        }
        i = (i + 1) % meta->size;
        c = meta->children[i];
    }

    // Resize if table is going to be full
    if (meta->count == meta->size - 1) {
        resize(meta, meta->size * 2);
    }

    // No entry, create a new child metatable and try again
    c = malloc(sizeof(Metatable));
    meta->children[i] = c;
    ++meta->count;

    // Copy metatable's children to new child
    c->children = malloc(meta->size * sizeof(Metatable*));
    for (int i = 0; i != meta->size; ++i) {
        c->children[i] = meta->children[i];
    }

    c->idx = meta->idx + 1;
    c->key = k;
    c->parent = meta;
    c->size = meta->size;
    c->count = meta->count;

    s->meta = c;
    if (c->idx == s->size) {
        s->size *= 2;
        s->data = realloc(s->data, s->size * sizeof(bt_Value));
    }
    s->data[c->idx] = *vl;
}