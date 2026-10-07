#include "core/os.h"
#include "event/event.h"
#include "graphics/graphics.h"
#include "headset/headset.h"
#include "headset/headset_ops.h"
#include "headset/openvr_diagnostic.h"
#include "util.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint32_t expectedScenePID;
static uint32_t framesPerSession = 120;
static double deadline;
static bool osQuit;

static void onQuit(void) {
  osQuit = true;
}

static bool fail(const char* operation) {
  const char* error = lovrGetError();
  fprintf(stderr, "openvr coexistence: %s: %s\n", operation, error && *error ? error : "requirement not satisfied");
  return false;
}

static bool withinDeadline(void) {
  double now = os_get_time();
  if (!isfinite(now) || now >= deadline) {
    lovrSetError("internal monotonic deadline expired (75 seconds); external runner must enforce 90 seconds");
    return fail("timeout");
  }
  return true;
}

static bool positiveInteger(const char* text, uint32_t maximum, uint32_t* value) {
  if (!text || !*text) return false;
  uint32_t result = 0;
  for (const unsigned char* p = (const unsigned char*) text; *p; p++) {
    if (*p < '0' || *p > '9') return false;
    uint32_t digit = *p - '0';
    if (result > maximum / 10 || (result == maximum / 10 && digit > maximum % 10)) return false;
    result = result * 10 + digit;
  }
  if (!result) return false;
  *value = result;
  return true;
}

static bool arguments(int argc, char** argv) {
  bool seenPID = false, seenFrames = false;
  for (int i = 1; i < argc; i += 2) {
    if (i + 1 >= argc) return false;
    if (!strcmp(argv[i], "--expected-scene-pid") && !seenPID) {
      if (!positiveInteger(argv[i + 1], UINT32_MAX, &expectedScenePID)) return false;
      seenPID = true;
    } else if (!strcmp(argv[i], "--frames-per-session") && !seenFrames) {
      if (!positiveInteger(argv[i + 1], 120, &framesPerSession)) return false;
      seenFrames = true;
    } else {
      return false;
    }
  }
  return seenPID;
}

static bool observe(OpenVRDiagnosticObservation* observation) {
  if (!withinDeadline()) return false;
  if (!lovrOpenVRDiagnosticObserve(observation)) return fail("runtime observation");
  if (!observation->connected || observation->currentScenePID != expectedScenePID) {
    lovrSetError("blocked prerequisite or coexistence failure: expected scene PID %" PRIu32 ", sampled %" PRIu32 ", connected %d",
      expectedScenePID, observation->currentScenePID, observation->connected);
    return fail("scene application oracle");
  }
  return withinDeadline();
}

static bool poll(void) {
  if (!withinDeadline()) return false;
  os_poll_events(0.);
  if (!lovrHeadsetPollEvents()) return fail("headset events");
  Event event;
  bool quit = osQuit;
  while (lovrEventPoll(&event)) {
    quit |= event.type == EVENT_QUIT || event.type == EVENT_RESTART;
  }
  lovrEventClear();
  if (quit) {
    lovrSetError("runtime or OS requests termination before completion");
    return fail("events");
  }
  return withinDeadline();
}

static bool session(uint32_t index, uint32_t* previousGeneration, Layer** ownedLayer) {
  OpenVRDiagnosticObservation before, running, after;
  if (!observe(&before)) return false;
  if (!lovrHeadsetStart()) return fail("headset start (runtime/GPU/assets prerequisite)");
  if (!observe(&running)) return false;
  uint32_t generation = lovrHeadsetGetSessionGeneration();
  if (!generation || generation == *previousGeneration || running.generation != generation || running.cleanupPending) {
    lovrSetError("session %" PRIu32 " has no fresh usable generation", index);
    return fail("session generation");
  }
  *previousGeneration = generation;

  LayerInfo info = { .width = 256, .height = 256, .immutable = false, .transparent = true, .filter = true };
  *ownedLayer = lovrLayerCreate(&info);
  if (!*ownedLayer) return fail("mutable transparent panel creation");
  float position[3] = { 0.f, 0.f, -1.5f };
  float orientation[4] = { 0.f, 0.f, 0.f, 1.f };
  lovrLayerSetOrigin(*ownedLayer, DEVICE_HEAD);
  lovrLayerSetPose(*ownedLayer, position, orientation);
  lovrLayerSetDimensions(*ownedLayer, .4f, .4f);
  if (!lovrHeadsetSetLayers(ownedLayer, 1, false)) return fail("panel selection");
  if (!observe(&running)) return false;
  if (running.liveOverlayHandles != 1) {
    lovrSetError("expected exactly one live panel handle, sampled %" PRIu32, running.liveOverlayHandles);
    return fail("panel ownership");
  }

  for (uint32_t frame = 0; frame < framesPerSession; frame++) {
    if (!poll()) return false;
    if (!lovrHeadsetUpdate()) return fail("frame update (runtime prerequisite)");
    if (!lovrHeadsetIsActive() || lovrHeadsetGetSessionGeneration() != generation) {
      lovrSetError("headset session changes during bounded frame loop");
      return fail("frame generation");
    }
    Pass* pass = lovrLayerGetPass(*ownedLayer);
    if (!pass || !lovrLayerGetTexture(*ownedLayer)) return fail("managed panel render target");
    LoadAction loads[4] = { LOAD_CLEAR, LOAD_DISCARD, LOAD_DISCARD, LOAD_DISCARD };
    float clears[4][4] = { { .1f, .025f, .2f, .25f } };
    clears[0][0] = frame % 2 ? .025f : .1f;
    if (!lovrPassSetClear(pass, loads, clears, LOAD_CLEAR, 0.f)) return fail("panel clear pass");
    if (!lovrGraphicsSubmit(&pass, 1)) return fail("managed graphics submit");
    if (!lovrHeadsetSubmit()) return fail("overlay texture submission");
    if (!lovrGraphicsPresent()) return fail("managed graphics frame completion");
    if (!observe(&after)) return false;
    if (after.generation != generation || after.cleanupPending ||
        after.setOverlayTextureCalls <= running.setOverlayTextureCalls ||
        after.setOverlayTextureSuccesses <= running.setOverlayTextureSuccesses || after.lastSetOverlayTextureError != 0) {
      lovrSetError("session %" PRIu32 " frame %" PRIu32 " lacks a successful real SetOverlayTexture", index, frame);
      return fail("delegated overlay oracle");
    }
    running = after;
  }

  lovrHeadsetStop();
  if (!observe(&after)) return false;
  if (lovrHeadsetIsActive() || after.generation || after.cleanupPending || after.liveOverlayHandles ||
      after.retiredOverlayHandles <= before.retiredOverlayHandles) {
    lovrSetError("session %" PRIu32 " does not retire its overlay handle completely", index);
    return fail("stop ownership");
  }
  lovrRelease(*ownedLayer, lovrLayerDestroy);
  *ownedLayer = NULL;
  fprintf(stderr, "openvr coexistence: session %" PRIu32 " generation %" PRIu32 ": %" PRIu32 " frames, scene PID %" PRIu32 ", retired handles %" PRIu64 "\n",
    index, generation, framesPerSession, after.currentScenePID, after.retiredOverlayHandles);
  return true;
}

int main(int argc, char** argv) {
  if (!arguments(argc, argv)) {
    fprintf(stderr, "openvr coexistence: usage: --expected-scene-pid <1..4294967295> [--frames-per-session <1..120>]\n");
    return 2;
  }
  if (!os_init()) {
    fail("blocked prerequisite: OS initialization");
    return 1;
  }
  double start = os_get_time();
  if (!isfinite(start)) {
    fail("monotonic clock");
    os_destroy();
    return 1;
  }
  deadline = start + 75.;
  bool eventAttempted = true;
  bool headsetAttempted = false;
  bool graphicsAttempted = false;
  bool success = false;
  Layer* ownedLayer = NULL;
  if (!lovrEventInit()) { fail("event initialization"); goto cleanup; }
  os_on_quit(onQuit);
  HeadsetConfig headset = {
    .backend = HEADSET_BACKEND_OPENVR, .connect = true, .connectConfigured = true, .supersample = 1.f,
    .overlay = true, .controllerSkeleton = SKELETON_NONE
  };
  headsetAttempted = true;
  if (!lovrHeadsetInit(&headset)) { fail("headset initialization"); goto cleanup; }
  if (!withinDeadline()) goto cleanup;
  if (!lovrHeadsetConnect()) { fail("blocked prerequisite: OpenVR runtime/action assets"); goto cleanup; }
  OpenVRDiagnosticObservation connected;
  if (!observe(&connected)) goto cleanup;
  GraphicsConfig graphics = { .vsync = false, .antialias = false };
  graphicsAttempted = true;
  if (!lovrGraphicsInit(&graphics)) {
    fail("blocked prerequisite: managed Vulkan GPU initialization; partial GPU ownership is unknown, retain all modules until process exit");
    lovrHeadsetWillExit();
    return 1;
  }
  uint32_t previousGeneration = 0;
  for (uint32_t i = 1; i <= 2; i++) {
    if (!session(i, &previousGeneration, &ownedLayer)) goto cleanup;
  }
  success = withinDeadline();

cleanup:
  lovrHeadsetWillExit();
  if (headsetAttempted) {
    lovrHeadsetStop();
    OpenVRDiagnosticObservation stopped;
    if (!lovrOpenVRDiagnosticObserve(&stopped) || stopped.cleanupPending || stopped.liveOverlayHandles) {
      fail("cleanup pending: retain backend, panel, graphics, event, and OS ownership until process exit");
      return 1;
    }
    if (ownedLayer) {
      lovrRelease(ownedLayer, lovrLayerDestroy);
      ownedLayer = NULL;
    }
    if (!lovrHeadsetBeforeGraphicsDestroy()) {
      fail("disconnect pending: retain graphics, headset, event, and OS ownership until process exit");
      return 1;
    }
  }
  if (graphicsAttempted) {
    lovrGraphicsDestroy();
    if (lovrGraphicsIsInitialized()) {
      fail("graphics teardown deferred: retain remaining module ownership until process exit");
      return 1;
    }
  }
  if (headsetAttempted) lovrHeadsetDestroy();
  if (eventAttempted) lovrEventDestroy();
  if (!withinDeadline()) success = false;
  os_destroy();
  return success ? 0 : 1;
}
