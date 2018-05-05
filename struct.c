#include "struct.h"
#include "context.h"
#include <stdlib.h>

/* Start size for table */
#define META_BUF 7

struct MetaEntry {
    Key* key;
    union {
        int idx;
        Metatable* child;
    };
    int redir; // Redirect to child?
    struct MetaEntry* next;
};

struct Metatable {
    struct MetaEntry** table;
    int entries;
    int reserved;
};

/* Creates a new metatable */
Metatable* struct_newmeta()
{
    Metatable* meta = malloc(sizeof(Metatable));
    meta->table = malloc(sizeof(struct MetaEntry*) * META_BUF);
    for (int i = 0; i != META_BUF; ++i) {
        meta->table[i] = NULL;
    }
    meta->entries = 0;
    meta->reserved = META_BUF;
    return meta;
}

/* GC destructor for structs */
void struct_destroy(void* st)
{
    free(((bt_Struct*)st)->data);
}