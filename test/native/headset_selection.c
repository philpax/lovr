#include "headset/headset_ops.h"
#include "headset/headset_openxr.h"
#include "util.h"
#include <stdlib.h>
#include "test.h"

typedef struct {
  HeadsetConfig* borrowed;
  HeadsetConfig snapshot;
  unsigned inits;
  unsigned destroys;
  unsigned connects;
  unsigned disconnects;
  unsigned starts;
  unsigned exits;
  bool connected;
  bool live;
  bool disconnectFails;
  bool startFails;
  bool initFails;
} FakeBackend;

static FakeBackend xr;
#ifdef LOVR_ENABLE_OPENVR
static FakeBackend vr;
static bool vrAvailable;
#endif
static OpenXRConnectResult ordinaryResult;
static OpenXRConnectResult overlayResult;
static bool pendingConnected;
static bool longDiagnostic;
static char attempts[32];
static size_t attemptCount;
static unsigned liveCount;
static unsigned maximumLive;
static bool invalid;
static void* allocations[8];
static unsigned frees[8];
static unsigned allocationCount;

void __real_free(void* pointer);
void __wrap_free(void* pointer) {
  if (pointer) {
    for (unsigned i = 0; i < allocationCount; i++) {
      if (allocations[i] == pointer) {
        if (++frees[i] != 1) abort();
        break;
      }
    }
  }
  __real_free(pointer);
}

static char* extension(void) {
  char* pointer = lovrMalloc(32);
  strcpy(pointer, "XR_test_selection");
  if (allocationCount == 8) abort();
  allocations[allocationCount] = pointer;
  frees[allocationCount++] = 0;
  return pointer;
}

static void record(char attempt) {
  if (attemptCount + 1 >= sizeof(attempts)) abort();
  attempts[attemptCount++] = attempt;
  attempts[attemptCount] = '\0';
  invalid |= liveCount != 0;
}

static void acquire(FakeBackend* fake, bool connected) {
  invalid |= fake->live;
  fake->live = true;
  fake->connected = connected;
  liveCount++;
  if (liveCount > maximumLive) maximumLive = liveCount;
}

static bool initialize(FakeBackend* fake, HeadsetConfig* config) {
  fake->inits++;
  fake->borrowed = config;
  fake->snapshot = *config;
  if (fake->initFails) {
    lovrSetError("Backend initialization failed");
    return false;
  }
  return true;
}

static bool disconnect(FakeBackend* fake) {
  fake->disconnects++;
  if (fake->disconnectFails) return false;
  if (fake->live) liveCount--;
  fake->live = false;
  fake->connected = false;
  return true;
}

static void destroy(FakeBackend* fake) {
  invalid |= fake->live;
  fake->destroys++;
  fake->borrowed = NULL;
}

static bool start(FakeBackend* fake) {
  fake->starts++;
  return !fake->startFails;
}

static bool xrInit(HeadsetConfig* config) { return initialize(&xr, config); }
static void xrDestroy(void) { destroy(&xr); }
static bool xrDisconnect(void) { return disconnect(&xr); }
static bool xrIsConnected(void) { return xr.connected; }
static bool xrStart(void) { return start(&xr); }
static uint32_t xrGeneration(void) { return xr.connected ? 1 : 0; }

OpenXRConnectResult lovrOpenXRConnect(OpenXRConnectMode mode) {
  xr.connects++;
  if (longDiagnostic) {
    char reason[1000];
    memset(reason, 'x', sizeof(reason) - 1);
    reason[sizeof(reason) - 1] = '\0';
    lovrSetError("%s", reason);
  }
  record(mode == OPENXR_CONNECT_REQUIRE_OVERLAY ? 'R' : 'X');
  OpenXRConnectResult result = mode == OPENXR_CONNECT_REQUIRE_OVERLAY ? overlayResult : ordinaryResult;
  if (result == OPENXR_CONNECT_SELECTED) acquire(&xr, true);
  if (result == OPENXR_CONNECT_CLEANUP_PENDING) acquire(&xr, pendingConnected);
  return result;
}

static bool xrConnect(void) {
  return lovrOpenXRConnect(OPENXR_CONNECT_ORDINARY) == OPENXR_CONNECT_SELECTED;
}

const HeadsetOps lovrHeadsetOpenXROps = {
  .HeadsetInit = xrInit,
  .HeadsetDestroy = xrDestroy,
  .HeadsetConnect = xrConnect,
  .HeadsetDisconnect = xrDisconnect,
  .HeadsetIsConnected = xrIsConnected,
  .HeadsetStart = xrStart,
  .HeadsetGetSessionGeneration = xrGeneration
};

#ifdef LOVR_ENABLE_OPENVR
static bool vrInit(HeadsetConfig* config) { return initialize(&vr, config); }
static void vrDestroy(void) { destroy(&vr); }
static bool vrDisconnect(void) { return disconnect(&vr); }
static bool vrIsConnected(void) { return vr.connected; }
static bool vrStart(void) { return start(&vr); }
static void vrWillExit(void) { vr.exits++; }
static uint32_t vrGeneration(void) { return vr.connected ? 2 : 0; }
static bool vrConnect(void) {
  vr.connects++;
  record('V');
  if (vrAvailable) acquire(&vr, true);
  return vrAvailable;
}

const HeadsetOps lovrHeadsetOpenVROps = {
  .WillExit = vrWillExit,
  .HeadsetInit = vrInit,
  .HeadsetDestroy = vrDestroy,
  .HeadsetConnect = vrConnect,
  .HeadsetDisconnect = vrDisconnect,
  .HeadsetIsConnected = vrIsConnected,
  .HeadsetStart = vrStart,
  .HeadsetGetSessionGeneration = vrGeneration
};
#endif

static void reset(void) {
  memset(&xr, 0, sizeof(xr));
#ifdef LOVR_ENABLE_OPENVR
  memset(&vr, 0, sizeof(vr));
  vrAvailable = false;
#endif
  ordinaryResult = OPENXR_CONNECT_UNAVAILABLE_CLEANED;
  overlayResult = OPENXR_CONNECT_UNAVAILABLE_CLEANED;
  pendingConnected = longDiagnostic = false;
  memset(attempts, 0, sizeof(attempts));
  attemptCount = 0;
  liveCount = 0;
  maximumLive = 0;
  invalid = false;
  allocationCount = 0;
}

static bool finish(void) {
  lovrHeadsetDestroy();
  lovrHeadsetGraphicsDestroyed();
  CHECK(liveCount == 0);
  CHECK(maximumLive <= 1);
  CHECK(!invalid);
  for (unsigned i = 0; i < allocationCount; i++) CHECK(frees[i] == 1);
  return true;
}

static bool matrix(void) {
  CHECK(HEADSET_BACKEND_OPENXR == 0);
  for (unsigned backend = 0; backend < 3; backend++) {
    for (unsigned overlay = 0; overlay < 2; overlay++) {
      for (unsigned available = 0; available < 8; available++) {
        reset();
        ordinaryResult = available & 1 ? OPENXR_CONNECT_SELECTED : OPENXR_CONNECT_UNAVAILABLE_CLEANED;
        overlayResult = available & 2 ? OPENXR_CONNECT_SELECTED : OPENXR_CONNECT_UNAVAILABLE_CLEANED;
#ifdef LOVR_ENABLE_OPENVR
        vrAvailable = (available & 4) != 0;
#endif
        HeadsetConfig config = {
          .backend = backend == 0 ? HEADSET_BACKEND_OPENXR : backend == 1 ? HEADSET_BACKEND_OPENVR : HEADSET_BACKEND_AUTO,
          .connect = true, .overlay = overlay != 0
        };
        CHECK(lovrHeadsetInit(&config));
        lovrHeadsetConnect();
        const char* expected = "";
        bool connected = false;
        if (backend == 0 || (backend == 2 && !overlay)) {
          expected = "X";
          connected = (available & 1) != 0;
        } else if (backend == 1) {
#ifdef LOVR_ENABLE_OPENVR
          expected = "V";
          connected = (available & 4) != 0;
          CHECK(vr.snapshot.overlay);
#endif
        } else if (available & 2) {
          expected = "R";
          connected = true;
        } else {
#ifdef LOVR_ENABLE_OPENVR
          expected = available & 4 ? "RV" : "RVX";
          connected = (available & 5) != 0;
#else
          expected = "RX";
          connected = (available & 1) != 0;
#endif
        }
        CHECK(strcmp(attempts, expected) == 0);
        CHECK(lovrHeadsetIsConnected() == connected);
        #ifdef LOVR_ENABLE_OPENVR
        CHECK(lovrHeadsetRequiresPhysicalDevice() == (connected && vr.connected));
#else
        CHECK(!lovrHeadsetRequiresPhysicalDevice());
#endif
        if (connected) {
          CHECK(lovrHeadsetConnect());
          CHECK(strcmp(attempts, expected) == 0);
        }
        CHECK(lovrHeadsetPrepareGraphics());
        lovrHeadsetConnect();
        CHECK(strcmp(attempts, expected) == 0);
        CHECK(finish());
      }
    }
  }
  return true;
}

static bool legacyConnectDefault(void) {
  reset();
  ordinaryResult = OPENXR_CONNECT_SELECTED;
  HeadsetConfig config = { .supersample = 1.f };
  CHECK(lovrHeadsetInit(&config));
  CHECK(xr.borrowed->connect);
  CHECK(lovrHeadsetConnect());
  CHECK(strcmp(attempts, "X") == 0);
  CHECK(finish());
  return true;
}

static bool connectFalse(void) {
  for (unsigned backend = 0; backend < 3; backend++) {
    reset();
    ordinaryResult = overlayResult = OPENXR_CONNECT_SELECTED;
#ifdef LOVR_ENABLE_OPENVR
    vrAvailable = true;
#endif
    HeadsetConfig config = {
      .backend = backend == 0 ? HEADSET_BACKEND_OPENXR : backend == 1 ? HEADSET_BACKEND_OPENVR : HEADSET_BACKEND_AUTO,
      .connect = false, .connectConfigured = true, .overlay = true
    };
    CHECK(lovrHeadsetInit(&config));
    lovrHeadsetConnect();
    CHECK(attemptCount == 0);
    CHECK(!lovrHeadsetIsConnected());
    CHECK(!lovrHeadsetRequiresPhysicalDevice());
    CHECK(lovrHeadsetPrepareGraphics());
    CHECK(attemptCount == 0);
    CHECK(finish());
  }
  return true;
}

static bool cleanupPending(void) {
  reset();
  longDiagnostic = true;
  HeadsetConfig diagnostic = { .backend = HEADSET_BACKEND_AUTO, .overlay = true };
  CHECK(lovrHeadsetInit(&diagnostic));
  CHECK(!lovrHeadsetConnect());
  CHECK(strstr(lovrGetError(), "[diagnostics truncated]") != NULL);
#ifdef LOVR_ENABLE_OPENVR
  CHECK(strcmp(attempts, "RVX") == 0);
#else
  CHECK(strcmp(attempts, "RX") == 0);
#endif
  CHECK(finish());
  for (unsigned connected = 0; connected < 2; connected++) {
    reset();
    overlayResult = OPENXR_CONNECT_CLEANUP_PENDING;
    pendingConnected = connected != 0;
    xr.disconnectFails = true;
    HeadsetConfig config = { .backend = HEADSET_BACKEND_AUTO, .connect = true, .overlay = true };
    CHECK(lovrHeadsetInit(&config));
    CHECK(!lovrHeadsetConnect());
    CHECK(strcmp(attempts, "R") == 0);
    CHECK(!lovrHeadsetPrepareGraphics());
    CHECK(!lovrHeadsetConnect());
    CHECK(strcmp(attempts, "R") == 0);
    CHECK(!lovrHeadsetBeforeGraphicsDestroy());
    CHECK(xr.destroys == 0);
    xr.disconnectFails = false;
    overlayResult = OPENXR_CONNECT_SELECTED;
    CHECK(lovrHeadsetConnect());
    CHECK(strcmp(attempts, "RR") == 0);
    CHECK(lovrHeadsetIsConnected());
    CHECK(lovrHeadsetPrepareGraphics());
    CHECK(finish());
  }
  return true;
}

static bool freeze(void) {
  reset();
  HeadsetConfig desktop = { .backend = HEADSET_BACKEND_AUTO, .connect = true, .overlay = true };
  CHECK(lovrHeadsetInit(&desktop));
  CHECK(lovrHeadsetPrepareGraphics());
  overlayResult = ordinaryResult = OPENXR_CONNECT_SELECTED;
#ifdef LOVR_ENABLE_OPENVR
  vrAvailable = true;
#endif
  CHECK(!lovrHeadsetConnect());
  CHECK(attemptCount == 0);
  CHECK(finish());

  reset();
  overlayResult = OPENXR_CONNECT_SELECTED;
#ifdef LOVR_ENABLE_OPENVR
  vrAvailable = true;
#endif
  HeadsetConfig config = { .backend = HEADSET_BACKEND_AUTO, .connect = true, .overlay = true };
  CHECK(lovrHeadsetInit(&config));
  CHECK(lovrHeadsetConnect());
  CHECK(lovrHeadsetPrepareGraphics());
  xr.startFails = true;
  CHECK(!lovrHeadsetStart());
  CHECK(strcmp(attempts, "R") == 0);
  CHECK(lovrHeadsetBeforeGraphicsDestroy());
  overlayResult = ordinaryResult = OPENXR_CONNECT_UNAVAILABLE_CLEANED;
  lovrHeadsetConnect();
  CHECK(strchr(attempts, 'V') == NULL);
  CHECK(strchr(attempts, 'X') == NULL);
  CHECK(finish());

  reset();
  overlayResult = OPENXR_CONNECT_SELECTED;
  CHECK(lovrHeadsetInit(&config));
  CHECK(lovrHeadsetConnect());
  CHECK(lovrHeadsetPrepareGraphics());
  CHECK(lovrHeadsetBeforeGraphicsDestroy());
  overlayResult = ordinaryResult = OPENXR_CONNECT_UNAVAILABLE_CLEANED;
#ifdef LOVR_ENABLE_OPENVR
  vrAvailable = true;
#endif
  lovrHeadsetConnect();
  CHECK(strchr(attempts, 'V') == NULL);
  CHECK(strchr(attempts, 'X') == NULL);
  CHECK(finish());
  return true;
}

static bool actualExit(void) {
  reset();
  lovrHeadsetWillExit();
  CHECK(xr.exits == 0);
  HeadsetConfig config = { .backend = HEADSET_BACKEND_OPENXR, .connect = true };
  CHECK(lovrHeadsetInit(&config));
  lovrHeadsetWillExit();
  CHECK(xr.exits == 0);
  CHECK(finish());
#ifdef LOVR_ENABLE_OPENVR
  reset();
  config.backend = HEADSET_BACKEND_OPENVR;
  vrAvailable = true;
  CHECK(lovrHeadsetInit(&config));
  lovrHeadsetWillExit();
  CHECK(vr.exits == 0);
  CHECK(lovrHeadsetConnect());
  CHECK(vr.exits == 0);
  lovrHeadsetWillExit();
  CHECK(vr.exits == 1);
  lovrHeadsetWillExit();
  CHECK(vr.exits == 2);
  CHECK(finish());
  lovrHeadsetWillExit();
  CHECK(vr.exits == 2);
#endif
  return true;
}

static bool ownership(void) {
  reset();
  ordinaryResult = OPENXR_CONNECT_SELECTED;
  HeadsetConfig first = {
    .backend = HEADSET_BACKEND_OPENXR, .connect = true, .supersample = 1.25f,
    .overlayOrder = 17, .extensionCount = 1, .extensions = extension()
  };
  char* originalExtensions = first.extensions;
  CHECK(lovrHeadsetInit(&first));
  CHECK(xr.borrowed != &first);
  CHECK(xr.snapshot.extensions == originalExtensions);
  CHECK(frees[0] == 0);
  first.supersample = 8.f;
  first.overlayOrder = 99;
  HeadsetConfig second = {
    .backend = HEADSET_BACKEND_AUTO, .connect = false, .connectConfigured = true, .overlay = true,
    .extensionCount = 1, .extensions = extension()
  };
  CHECK(lovrHeadsetInit(&second));
  CHECK(frees[1] == 1);
  CHECK(frees[0] == 0);
  CHECK(xr.inits == 1);
  CHECK(xr.borrowed->supersample == 1.25f);
  CHECK(xr.borrowed->overlayOrder == 17);
  CHECK(xr.borrowed->extensions == originalExtensions);
  CHECK(strcmp(xr.borrowed->extensions, "XR_test_selection") == 0);
  CHECK(lovrHeadsetConnect());
  CHECK(strcmp(attempts, "X") == 0);
  lovrHeadsetDestroy();
  CHECK(xr.destroys == 0);
  CHECK(frees[0] == 0);
  CHECK(lovrHeadsetIsConnected());
  xr.disconnectFails = true;
  lovrHeadsetDestroy();
  CHECK(xr.destroys == 0);
  CHECK(frees[0] == 0);
  CHECK(xr.borrowed->extensions == originalExtensions);
  CHECK(lovrHeadsetIsConnected());
  xr.disconnectFails = false;
  CHECK(finish());
  CHECK(xr.destroys == 1);
  CHECK(frees[0] == 1);
  lovrHeadsetDestroy();
  CHECK(xr.destroys == 1);
  CHECK(frees[0] == 1);
  return true;
}

static bool gpuLifetime(void) {
  reset();
  CHECK(lovrHeadsetPrepareGraphics());
  HeadsetConfig first = { .supersample = 1.f };
  HeadsetConfig second = { .supersample = 2.f };
  ordinaryResult = OPENXR_CONNECT_SELECTED;
  CHECK(lovrHeadsetInit(&first));
  CHECK(!lovrHeadsetConnect());
  CHECK(attemptCount == 0);
  lovrHeadsetDestroy();
  CHECK(lovrHeadsetInit(&second));
  CHECK(!lovrHeadsetConnect());
  CHECK(attemptCount == 0);
  xr.disconnectFails = true;
  CHECK(!lovrHeadsetBeforeGraphicsDestroy());
  CHECK(!lovrHeadsetConnect());
  CHECK(attemptCount == 0);
  xr.disconnectFails = false;
  CHECK(lovrHeadsetBeforeGraphicsDestroy());
  CHECK(!lovrHeadsetConnect());
  CHECK(attemptCount == 0);
  lovrHeadsetGraphicsDestroyed();
  CHECK(lovrHeadsetConnect());
  CHECK(strcmp(attempts, "X") == 0);
  CHECK(xr.borrowed->supersample == 2.f);
  CHECK(lovrHeadsetPrepareGraphics());
  CHECK(finish());
  return true;
}

static bool deferredAcquire(void) {
  reset();
  HeadsetConfig first = { .supersample = 1.f, .extensionCount = 1, .extensions = extension() };
  HeadsetConfig rejected = { .supersample = 2.f, .extensionCount = 1, .extensions = extension() };
  HeadsetConfig fresh = { .supersample = 3.f, .extensionCount = 1, .extensions = extension() };
  CHECK(lovrHeadsetInit(&first));
  ordinaryResult = OPENXR_CONNECT_SELECTED;
  CHECK(lovrHeadsetConnect());
  xr.disconnectFails = true;
  lovrHeadsetDestroy();
  CHECK(!lovrHeadsetInit(&rejected));
  CHECK(strstr(lovrGetError(), "cleanup") != NULL);
  CHECK(frees[0] == 0 && frees[1] == 1 && xr.inits == 1 && xr.destroys == 0);
  CHECK(xr.borrowed->supersample == 1.f);
  xr.disconnectFails = false;
  CHECK(lovrHeadsetInit(&fresh));
  CHECK(xr.inits == 2 && xr.destroys == 1 && frees[0] == 1);
  CHECK(xr.borrowed->supersample == 3.f && xr.borrowed->extensions == fresh.extensions);
  CHECK(finish());
  CHECK(xr.destroys == 2);
  lovrHeadsetDestroy();
  CHECK(xr.destroys == 2);
  return true;
}

static bool failedInitAcquire(void) {
  for (unsigned retained = 0; retained < 2; retained++) {
    reset();
    HeadsetConfig first = { .supersample = 1.f, .extensionCount = 1, .extensions = extension() };
    HeadsetConfig rejected = { .supersample = 2.f, .extensionCount = 1, .extensions = extension() };
    HeadsetConfig fresh = { .supersample = 3.f, .extensionCount = 1, .extensions = extension() };
    xr.initFails = true;
    xr.disconnectFails = retained;
    CHECK(!lovrHeadsetInit(&first));
    CHECK(frees[0] == !retained && xr.destroys == !retained);
    if (retained) {
      CHECK(!lovrHeadsetInit(&rejected));
      CHECK(strstr(lovrGetError(), "cleanup") != NULL);
    } else {
      lovrFree(rejected.extensions);
    }
    xr.initFails = xr.disconnectFails = false;
    CHECK(lovrHeadsetInit(&fresh));
    CHECK(xr.inits == 2 && xr.destroys == 1 && frees[0] == 1);
    CHECK(xr.borrowed->supersample == 3.f);
    CHECK(finish());
    CHECK(xr.destroys == 2);
  }
  return true;
}

int main(int argc, char** argv) {
  const NativeTest tests[] = {
    { "selection.matrix", matrix },
    { "selection.connect-false", connectFalse },
    { "selection.legacy-connect-default", legacyConnectDefault },
    { "selection.cleanup-pending", cleanupPending },
    { "selection.freeze", freeze },
    { "selection.gpu-lifetime", gpuLifetime },
    { "selection.ownership", ownership },
    { "selection.deferred-acquire", deferredAcquire },
    { "selection.failed-init-acquire", failedInitAcquire },
    { "selection.actual-exit", actualExit }
  };
  return nativeRunTests(argc, argv, tests, COUNTOF(tests));
}
