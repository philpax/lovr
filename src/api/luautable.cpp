// Reads Luau table storage directly for bulk numeric conversions that would otherwise cost API calls per value.
#include "luautable.h"
#include "lobject.h"
#include <string.h>

extern "C" int luax_readfloats(lua_State* L, int index, int start, int count, int components, uint32_t stride, char* data) {
  if (lua_type(L, index) != LUA_TTABLE || components < 1 || components > 4) return 0;
  const LuaTable* table = static_cast<const LuaTable*>(lua_topointer(L, index));
  int copied = 0;
  for (; copied < count; copied++, data += stride) {
    int first = start + copied * components;
    if (first < 1 || first - 1 + components > table->sizearray) break;
    const TValue* values = &table->array[first - 1];
    float element[4];
    int component = 0;
    for (; component < components; component++) {
      if (!ttisnumber(&values[component])) break;
      element[component] = (float) nvalue(&values[component]);
    }
    if (component < components) break;
    memcpy(data, element, components * sizeof(float));
  }
  return copied;
}
