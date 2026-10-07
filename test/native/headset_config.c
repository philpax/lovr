#include "test.h"
#define LOVR_DISABLE_TASK
#include "../../src/api/api.c"
#define luax_yieldjob(...) 0
#undef luax_register
#undef luax_registertype
#define luax_register(L, functions) ((void) (L))
#define luax_registertype(L, type) ((void) (L))
#include "../../src/api/l_headset.c"
#include <lualib.h>

static HeadsetConfig captured;
static unsigned initCount;

bool lovrHeadsetInit(HeadsetConfig* config) {
  captured = *config;
  initCount++;
  return true;
}

void lovrHeadsetDestroy(void) {
  lovrFree(captured.extensions);
  captured.extensions = NULL;
}

static bool parse(const char* script, HeadsetBackend backend, bool connect, const char* extensions, size_t length) {
  lua_State* L = luaL_newstate();
  CHECK(L);
  luaL_openlibs(L);
  lua_pushcfunction(L, luaopen_lovr_headset);
  lua_setglobal(L, "openHeadset");
  memset(&captured, 0, sizeof(captured));
  initCount = 0;
  bool success = luax_loadbufferx(L, script, strlen(script), "headset config", "t") == 0 && lua_pcall(L, 0, 1, 0) == 0;
  if (!success) fprintf(stderr, "%s\n", lua_tostring(L, -1));
  if (success) {
    lua_setfield(L, LUA_REGISTRYINDEX, "_lovrconf");
    lua_getglobal(L, "openHeadset");
    success = lua_pcall(L, 0, 1, 0) == 0;
    if (!success) fprintf(stderr, "%s\n", lua_tostring(L, -1));
  }
  bool matches = success && initCount == 1 && captured.backend == backend && captured.connect == connect;
  if (matches && extensions) {
    matches = captured.extensionCount == 2 && captured.extensions && !memcmp(captured.extensions, extensions, length);
  } else if (matches) {
    matches = captured.extensionCount == 0 && captured.extensions == NULL;
  }
  luax_close(L);
  CHECK(matches);
  return true;
}

static bool defaults(void) {
  CHECK(parse("return nil", HEADSET_BACKEND_OPENXR, true, NULL, 0));
  CHECK(parse("return {}", HEADSET_BACKEND_OPENXR, true, NULL, 0));
  CHECK(parse("return { headset = {} }", HEADSET_BACKEND_OPENXR, true, NULL, 0));
  CHECK(parse("return { headset = false }", HEADSET_BACKEND_OPENXR, true, NULL, 0));
  return true;
}

static bool backends(void) {
  CHECK(parse("return { headset = { backend = 'openxr', connect = false } }", HEADSET_BACKEND_OPENXR, false, NULL, 0));
  CHECK(parse("return { headset = { backend = 'openvr', connect = true } }", HEADSET_BACKEND_OPENVR, true, NULL, 0));
  CHECK(parse("return { headset = { backend = 'auto' } }", HEADSET_BACKEND_AUTO, true, NULL, 0));
  return true;
}

static bool invalidBackend(void) {
  const char* values[] = { "'invalid'", "'OPENXR'", "'openxr\\0extra'", "false", "{}", "42" };
  for (size_t i = 0; i < COUNTOF(values); i++) {
    lua_State* L = luaL_newstate();
    CHECK(L);
    luaL_openlibs(L);
    char script[128];
    snprintf(script, sizeof(script), "return { headset = { backend = %s } }", values[i]);
    bool loaded = luax_loadbufferx(L, script, strlen(script), "invalid backend", "t") == 0 && lua_pcall(L, 0, 1, 0) == 0;
    initCount = 0;
    int status = 0;
    if (loaded) {
      lua_setfield(L, LUA_REGISTRYINDEX, "_lovrconf");
      lua_pushcfunction(L, luaopen_lovr_headset);
      status = lua_pcall(L, 0, 1, 0);
    }
    bool rejected = loaded && status != 0 && initCount == 0;
    luax_close(L);
    CHECK(rejected);
  }
  return true;
}

static bool extensions(void) {
  static const char expected[] = "XR_first\0XR_second";
  CHECK(parse("return { headset = { extensions = { 'XR_first', 'XR_second' } } }", HEADSET_BACKEND_OPENXR, true, expected, sizeof(expected)));
  CHECK(parse("return { headset = { extensions = {} } }", HEADSET_BACKEND_OPENXR, true, NULL, 0));
  return true;
}

int main(int argc, char** argv) {
  const NativeTest tests[] = {
    { "headset.config.defaults", defaults },
    { "headset.config.backends", backends },
    { "headset.config.invalid-backend", invalidBackend },
    { "headset.config.extensions", extensions }
  };
  return nativeRunTests(argc, argv, tests, COUNTOF(tests));
}
