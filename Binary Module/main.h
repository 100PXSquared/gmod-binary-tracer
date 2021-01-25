#pragma once

#include "GarrysMod/Lua/Interface.h"

constexpr int DEFAULT_RES[2] = { 64, 64 };

// You'll need to change these to valid paths on your system (note that if you don't want to use this with Starfall, you'll still need to specify a valid STARFALL_PATH)
constexpr char OUTPUT_PATH[] = "D:\\Programming\\GitHub\\gmod-binary-tracer\\Binary Module\\Release\\x64\\renders\\";
constexpr char STARFALL_PATH[] = "D:\\Steam\\steamapps\\common\\GarrysMod\\garrysmod\\data\\sf_filedata\\";

void printLua(GarrysMod::Lua::ILuaBase* inst, const char text[]);
void dumpStack(GarrysMod::Lua::ILuaBase* inst);
void checkArgCount(GarrysMod::Lua::ILuaBase* LUA, const int& count);