#ifndef _BULLET_TRAIN_H_
#define _BULLET_TRAIN_H_

#include <stdlib.h>

/*
** ============================================================
** Glorious main header,
** featuring configuration macros and declarations.
** ============================================================
*/

#ifdef BT_BUILD_DLL
#define BT_API __declspec(dllexport)
#else
#define BT_API __declspec(dllimport)
#endif

#ifdef BT_USE_DOUBLE
#define BT_NUMBER double
#else
#define BT_NUMBER float
#endif

#ifndef BT_TIMER
#define BT_TIMER int
#endif

/*
** ============================================================
** End of configuration, declarations begin here
** ============================================================
*/

typedef struct bt_Context bt_Context;
typedef struct bt_Thread bt_Thread;
typedef struct bt_Value bt_Value;
typedef struct bt_Function bt_Function;
typedef struct bt_Closure bt_Closure;
typedef struct bt_Struct bt_Struct;

/* Function pointer for GC destructors */
typedef void (*bt_Destructor)(void*);

BT_API bt_Context* bt_newcontext();
BT_API void bt_freecontext(bt_Context* bt);
BT_API void* bt_gcalloc(bt_Context* bt, size_t size, bt_Destructor d);

BT_API bt_Struct* bt_newstruct(bt_Context* bt);

BT_API bt_Function* bt_compile(bt_Context* bt, const char* src);
BT_API bt_Function* bt_fcompile(bt_Context* bt, const char* path);

BT_API void bt_call(bt_Context* bt, bt_Function* fn);

#endif