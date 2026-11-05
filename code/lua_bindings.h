#pragma once
#include "foundation/basic.h"

struct lua_State;

void lua_get_int(struct lua_State *L, const char *name, int *result);
void lua_get_float(struct lua_State *L, const char *name, float *result);
void lua_get_string(struct lua_State *L, const char *name, const char **result);
void lua_get_bool(struct lua_State *L, const char *name, bool *result);

// Wrapper for calling Lua functions from C containing:
// * The name of the function to be called
// * String describing arguments and return type
// * List of arguments
// * Pointer to store the returned result
//
// lua_generic_call("function_name", "dd>d", x, y, &z);
//
void lua_generic_call(struct lua_State *L, const char *func, const char *signature, ...);


//
// Lua to C bindings
//
// All functions registered with Lua must have this same prototype:
// typedef int (*lua_CFunction)(lua_State *L)
//

// Registers all Lua bindings so that the given lua_State can access and call them
// All bindings are defined in `lua_bindings.c`
void register_lua_bindings(struct lua_State *L);
