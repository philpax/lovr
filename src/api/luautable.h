#pragma once

#include <lua.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Copies whole elements of `components` numbers from the array part of the table at `index`, starting at the 1-based
// position `start`, into float32 values `stride` bytes apart. Returns how many elements it copied; it stops before an
// element that leaves the array part or holds a value that is not a number, so the caller converts the rest generally.
int luax_readfloats(lua_State* L, int index, int start, int count, int components, uint32_t stride, char* data);

#ifdef __cplusplus
}
#endif
