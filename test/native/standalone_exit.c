#include "test.h"

#define LUA_YIELD 1
#define LUA_TSTRING 4
#define LUA_TNUMBER 3

typedef struct {
  int resumeStatus;
  int type;
  const char* string;
  int status;
} lua_State;

static lua_State thread;
static const char* events[16];
static size_t eventCount;
static bool validCalls;
static int exitStatus;

static void record(const char* event) {
  if (eventCount < sizeof(events) / sizeof(events[0])) {
    events[eventCount++] = event;
  } else {
    validCalls = false;
  }
}

static int luax_resume(lua_State* T, int arguments) {
  validCalls &= T == &thread && arguments == 0;
  record("resume");
  return T->resumeStatus;
}

static int lua_type(lua_State* T, int index) {
  validCalls &= T == &thread && index == 1;
  record("type");
  return T->type;
}

static const char* lua_tostring(lua_State* T, int index) {
  validCalls &= T == &thread && index == 1;
  record("string");
  return T->string;
}

static int lua_tointeger(lua_State* T, int index) {
  validCalls &= T == &thread && index == 1;
  record("integer");
  return T->status;
}

#if !defined(LOVR_DISABLE_HEADSET) && !defined(EMSCRIPTEN)
static void lovrHeadsetWillExit(void) {
  record("hint");
}
#endif

static void lovrSetLogCallback(void* callback, void* userdata) {
  validCalls &= callback == NULL && userdata == NULL;
  record("logclear");
}

static void luax_close(lua_State* T) {
  validCalls &= T == &thread;
  record("close");
}

static void os_destroy(void) {
  record("osdestroy");
}

static void fakeExit(int status) {
  exitStatus = status;
  record("exit");
}

#define exit fakeExit
#include "standalone_step.inc"
#undef exit

static void reset(int resumeStatus, int type, const char* string, int status) {
  thread = (lua_State) { resumeStatus, type, string, status };
  eventCount = 0;
  validCalls = true;
  exitStatus = 12345;
}

static bool checkEvents(const char* const* expected, size_t count) {
  CHECK(validCalls);
  CHECK(eventCount == count);
  for (size_t i = 0; i < count; i++) {
    CHECK(strcmp(events[i], expected[i]) == 0);
  }
  return true;
}

static bool testYield(void) {
  reset(LUA_YIELD, LUA_TSTRING, "restart", 42);
  CHECK(step(&thread));
  const char* expected[] = { "resume" };
  CHECK(checkEvents(expected, sizeof(expected) / sizeof(expected[0])));
  CHECK(exitStatus == 12345);
  return true;
}

static bool testRestart(void) {
  reset(0, LUA_TSTRING, "restart", 42);
  CHECK(!step(&thread));
  const char* expected[] = { "resume", "type", "string" };
  CHECK(checkEvents(expected, sizeof(expected) / sizeof(expected[0])));
  CHECK(exitStatus == 12345);
  return true;
}

static bool testActualExit(void) {
  const int statuses[] = { 0, 42, -7 };
  for (size_t i = 0; i < sizeof(statuses) / sizeof(statuses[0]); i++) {
    for (int stringResult = 0; stringResult < 2; stringResult++) {
      reset(i == 2 ? 2 : 0, stringResult ? LUA_TSTRING : LUA_TNUMBER, "quit", statuses[i]);
      CHECK(!step(&thread));
      const char* expected[] = {
        "resume", "type", "string", "integer",
#if !defined(LOVR_DISABLE_HEADSET) && !defined(EMSCRIPTEN)
        "hint",
#endif
        "logclear", "close", "osdestroy", "exit"
      };
      if (!stringResult) {
        for (size_t j = 2; j + 1 < sizeof(expected) / sizeof(expected[0]); j++) {
          expected[j] = expected[j + 1];
        }
      }
      CHECK(checkEvents(expected, sizeof(expected) / sizeof(expected[0]) - !stringResult));
      CHECK(exitStatus == statuses[i]);
    }
  }
  return true;
}

int main(int argc, char** argv) {
  const NativeTest tests[] = {
    { "standalone.yield", testYield },
    { "standalone.restart", testRestart },
    { "standalone.actual-exit", testActualExit }
  };
  return nativeRunTests(argc, argv, tests, sizeof(tests) / sizeof(tests[0]));
}
