#pragma once

#include "luaumetrics.h"
#include <stdbool.h>
#include <lua.h>

// The registry owns one reference; each reserved CodeGen block owns another.
typedef struct {
  LuauMetrics metrics;
  uint64_t references;
} LuauMetricsState;

typedef struct {
  uint64_t elapsedNs;
  uint64_t cpuNs;
} LuauMetricsClock;

#ifdef __cplusplus
extern "C" {
#endif

void luax_releaseluaumetrics(LuauMetricsState* state);
LuauMetricsClock luax_luaumetricsclock(LuauMetrics* metrics);
void luax_luaumetricselapsed(LuauMetrics* metrics, LuauMetricsClock start, uint64_t* elapsedNs, uint64_t* cpuNs);

#ifdef LOVR_LUAU_CODEGEN
bool luax_codegenhascontext(lua_State* L);
bool luax_codegencreate(lua_State* L, LuauMetricsState* state);
void luax_codegencompile(lua_State* L, int index, int nativeScope, LuauMetricsState* state);
#endif

#ifdef __cplusplus
}
#endif
