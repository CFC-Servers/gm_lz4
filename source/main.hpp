// main.h
#pragma once

#include "GarrysMod/Lua/Interface.h"
#include <lz4.h>
#include <thread>

int CompressString(lua_State *L);
int DecompressString(lua_State *L);
void RegisterFunctions(lua_State *L);
