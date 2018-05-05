#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "thread.h"
#include "struct.h"

#define BT_REG_SIZE 127

/*
** Information about a GC memory allocation .
** Stored at the beginning of each memory block.
*/
typedef struct GCBlock {
    struct GCBlock* next;
    bt_Destructor destructor;
    int refs;
} GCBlock;

struct bt_Context {
    Key* key_regist[BT_REG_SIZE];
    Metatable* root_meta;
    GCBlock* gclist;
    bt_Thread* inactive;
    bt_Thread* active;
};

/*
** Creates a new virtual machine.
** Should be the first thing you call.
*/
BT_API bt_Context* bt_newcontext()
{
    bt_Context* bt = malloc(sizeof(bt_Context));
    for (int i = 0; i != BT_REG_SIZE; ++i) {
        bt->key_regist[i] = NULL;
    }
    bt->root_meta = struct_newmeta();
    bt->inactive = NULL;
    bt->active = NULL;
    bt->gclist = NULL;
    return bt;
}

/*
** Destroys the VM.
** Collects garbage, frees threads, and everything else.
*/
BT_API void bt_freecontext(bt_Context* bt)
{
    GCBlock* gc = bt->gclist;
    while (gc != NULL) {
        if (gc->destructor != NULL) {
            gc->destructor(gc + sizeof(GCBlock));
        }
        GCBlock* temp = gc;
        free(gc);
        gc = temp;
    }
    free(bt);
}

/*
** djb2 hash function
*/
static unsigned long keyhash(const char* str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

/*
** Retrieves a key from the key registry.
** Creates a new entry if it doesn't exist.
*/
Key* ctx_getkey(bt_Context* bt, const char* name)
{
    unsigned long hash = keyhash(name);
    Key** loc = &bt->key_regist[hash % BT_REG_SIZE];
    Key* key = *loc;
    do {
        if (strcmp(key->text, name) == 0) {
            return key;
        }
    } while ((key = key->next));
    // Key not found, make a new one
    key = malloc(sizeof(Key) + strlen(name) + 1);
    strcpy(key->text, name);
    key->hash = hash;
    key->next = *loc;
    *loc = key;
    return key;
}

/*
** ============================================================
** Thread management
** ============================================================
*/

/*
** Gets an inactive thread.
** If no inactive threads are available, creates a new one.
*/
bt_Thread* ctx_getthread(bt_Context* bt)
{
    bt_Thread* result;
    if (bt->inactive) {
        result = bt->inactive;
        bt->inactive = result->next;
    } else {
        result = thread_new();
    }
    return result;
}

/*
** ============================================================
** Garbage collection
** ============================================================
*/

/* Allocates garbage collected memory */
BT_API void* bt_gcalloc(bt_Context* bt, size_t size, bt_Destructor d)
{
    GCBlock* gc = malloc(sizeof(GCBlock) + size);
    gc->refs = 0;
    gc->destructor = d;
    gc->next = bt->gclist;
    bt->gclist = gc;
    return gc + sizeof(GCBlock);
}

/* Start vector size for structs */
#define STRUCT_BUF 4

/* Creates a new bt_Struct */
BT_API bt_Struct* bt_newstruct(bt_Context* bt)
{
    bt_Struct* st = bt_gcalloc(bt, sizeof(bt_Struct), struct_destroy);
    st->data = malloc(sizeof(bt_Value) * STRUCT_BUF);
    st->size = 0;
    st->reserved = STRUCT_BUF;
    st->meta = bt->root_meta;
    return st;
}