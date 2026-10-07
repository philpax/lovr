#include "test.h"
#include "headset/openvr_events.h"

static struct {
  struct VREvent_t events[256];
  struct VREvent_t overlayQueues[2][128];
  size_t overlayCount[2];
  size_t overlayNext[2];
  size_t count;
  size_t next;
  unsigned int polls;
  unsigned int acknowledgements;
  unsigned int overlayPolls;
  struct VREvent_t overlayEvent;
  bool overlayPending;
  bool input;
  bool pause;
  bool visible;
  bool dashboard;
  uint32_t renderer;
  uint32_t focus;
  uint32_t scene;
  EVRSceneApplicationState sceneState;
  EDeviceActivityLevel activity;
} fake;

static bool OPENVR_FNTABLE_CALLTYPE poll(struct VREvent_t* event, uint32_t size) {
  fake.polls++;
  if (size != sizeof(*event) || fake.next == fake.count) return false;
  *event = fake.events[fake.next++];
  return true;
}

static bool OPENVR_FNTABLE_CALLTYPE pollOverlay(VROverlayHandle_t handle, struct VREvent_t* event, uint32_t size) {
  if ((handle != 19 && handle != 20) || size != sizeof(*event)) return false;
  size_t index = handle == 19 ? 0 : 1;
  if (fake.overlayNext[index] < fake.overlayCount[index]) {
    *event = fake.overlayQueues[index][fake.overlayNext[index]++];
    return true;
  }
  if (handle != 19) return false;
  fake.overlayPolls++;
  if (!fake.overlayPending) return false;
  fake.overlayPending = false;
  *event = fake.overlayEvent;
  return true;
}

static bool OPENVR_FNTABLE_CALLTYPE input(void) { return fake.input; }
static bool OPENVR_FNTABLE_CALLTYPE pauseApplication(void) { return fake.pause; }
static bool OPENVR_FNTABLE_CALLTYPE visible(VROverlayHandle_t handle) { return handle == 19 && fake.visible; }
static bool OPENVR_FNTABLE_CALLTYPE dashboard(void) { return fake.dashboard; }
static uint32_t OPENVR_FNTABLE_CALLTYPE renderer(void) { return fake.renderer; }
static uint32_t OPENVR_FNTABLE_CALLTYPE focus(void) { return fake.focus; }
static uint32_t OPENVR_FNTABLE_CALLTYPE scene(void) { return fake.scene; }
static EVRSceneApplicationState OPENVR_FNTABLE_CALLTYPE sceneState(void) { return fake.sceneState; }
static EDeviceActivityLevel OPENVR_FNTABLE_CALLTYPE activity(TrackedDeviceIndex_t device) {
  return device == k_unTrackedDeviceIndex_Hmd ? fake.activity : EDeviceActivityLevel_k_EDeviceActivityLevel_Unknown;
}
static void OPENVR_FNTABLE_CALLTYPE acknowledge(void) { fake.acknowledgements++; }

static struct VR_IVRSystem_FnTable systemTable = {
  .PollNextEvent = poll, .IsInputAvailable = input, .ShouldApplicationPause = pauseApplication,
  .GetTrackedDeviceActivityLevel = activity, .AcknowledgeQuit_Exiting = acknowledge
};
static struct VR_IVROverlay_FnTable overlayTable = {
  .IsDashboardVisible = dashboard, .IsOverlayVisible = visible, .PollNextOverlayEvent = pollOverlay
};
static struct VR_IVRCompositor_FnTable compositorTable = {
  .GetLastFrameRenderer = renderer, .GetCurrentSceneFocusProcess = focus
};
static struct VR_IVRApplications_FnTable applicationsTable = {
  .GetCurrentSceneProcessId = scene, .GetSceneApplicationState = sceneState
};
static OpenVRRuntime runtime;
static VROverlayHandle_t handles[] = { 0, 19 };

static void reset(OpenVREvents* state) {
  memset(&fake, 0, sizeof(fake));
  runtime = (OpenVRRuntime) { .initialized = true, .system = &systemTable, .overlay = &overlayTable,
    .compositor = &compositorTable, .applications = &applicationsTable };
  fake.activity = EDeviceActivityLevel_k_EDeviceActivityLevel_Unknown;
  lovrOpenVREventsReset(state);
}

static OpenVREventEffects update(OpenVREvents* state, bool ready) {
  OpenVREventEffects effects;
  lovrOpenVREventsUpdate(state, &runtime, ready, 42, handles, 2, &effects);
  return effects;
}

static void enqueue(uint32_t type, uint32_t pid) {
  fake.events[fake.count++] = (struct VREvent_t) { .eventType = type, .data.process.pid = pid };
}

static bool manifestReadyAndInitialState(void) {
  OpenVREvents state;
  reset(&state);
  enqueue(EVREventType_VREvent_SeatedZeroPoseReset, 0);
  OpenVREventEffects effects = update(&state, false);
  CHECK(fake.polls == 0 && fake.overlayPolls == 0 && !state.sampled && effects.drained == 0);
  fake.input = fake.visible = true;
  fake.activity = EDeviceActivityLevel_k_EDeviceActivityLevel_UserInteraction;
  effects = update(&state, true);
  CHECK(effects.invalidateSeated && !effects.invalidateStanding && effects.drained == 1);
  CHECK(state.visible && state.inputFocus && state.activityKnown && state.active && !state.mainSceneVisible);
  CHECK(effects.visibilityChanged && effects.inputFocusChanged && effects.activityChanged && effects.mainSceneVisibilityChanged);
  effects = update(&state, true);
  CHECK(!effects.visibilityChanged && !effects.inputFocusChanged && !effects.activityChanged && !effects.mainSceneVisibilityChanged);
  return true;
}

static bool visibilityMatrix(void) {
  OpenVREvents state;
  reset(&state);
  for (unsigned int mask = 0; mask < 32; mask++) {
    fake.dashboard = (mask & 1) != 0;
    fake.renderer = (mask & 2) ? 77 : 0;
    fake.focus = (mask & 4) ? 78 : 0;
    fake.scene = (mask & 8) ? 79 : 0;
    fake.sceneState = (mask & 16) ? EVRSceneApplicationState_Running : EVRSceneApplicationState_None;
    update(&state, true);
    CHECK(state.mainSceneVisible == (mask != 0));
    CHECK(!state.visible);
  }
  fake.dashboard = false;
  fake.renderer = fake.focus = fake.scene = 0;
  for (int value = EVRSceneApplicationState_Starting; value <= EVRSceneApplicationState_Waiting; value++) {
    fake.sceneState = (EVRSceneApplicationState) value;
    update(&state, true);
    CHECK(state.mainSceneVisible);
  }
  fake.sceneState = (EVRSceneApplicationState) 1234;
  update(&state, true);
  CHECK(state.mainSceneVisible);
  fake.sceneState = (EVRSceneApplicationState) -1;
  update(&state, true);
  CHECK(state.mainSceneVisible);
  fake.sceneState = EVRSceneApplicationState_None;
  struct VR_IVRApplications_FnTable incompleteApplications = applicationsTable;
  incompleteApplications.GetSceneApplicationState = NULL;
  runtime.applications = &incompleteApplications;
  update(&state, true);
  CHECK(state.mainSceneVisible);
  incompleteApplications = applicationsTable;
  incompleteApplications.GetCurrentSceneProcessId = NULL;
  update(&state, true);
  CHECK(state.mainSceneVisible);
  runtime.applications = &applicationsTable;
  struct VR_IVRCompositor_FnTable incompleteCompositor = compositorTable;
  incompleteCompositor.GetLastFrameRenderer = NULL;
  runtime.compositor = &incompleteCompositor;
  update(&state, true);
  CHECK(state.mainSceneVisible);
  incompleteCompositor = compositorTable;
  incompleteCompositor.GetCurrentSceneFocusProcess = NULL;
  update(&state, true);
  CHECK(state.mainSceneVisible);
  runtime.compositor = &compositorTable;
  struct VR_IVROverlay_FnTable incompleteOverlay = overlayTable;
  incompleteOverlay.IsDashboardVisible = NULL;
  runtime.overlay = &incompleteOverlay;
  update(&state, true);
  CHECK(state.mainSceneVisible);
  runtime.overlay = &overlayTable;
  OpenVRRuntime full = runtime;
  runtime.applications = NULL;
  update(&state, true);
  CHECK(state.mainSceneVisible);
  runtime = full;
  runtime.compositor = NULL;
  update(&state, true);
  CHECK(state.mainSceneVisible);
  runtime = full;
  runtime.overlay = NULL;
  update(&state, true);
  CHECK(state.mainSceneVisible);
  runtime = full;
  enqueue(EVREventType_VREvent_DashboardActivated, 0);
  enqueue(EVREventType_VREvent_DashboardDeactivated, 0);
  update(&state, true);
  CHECK(state.mainSceneVisible);
  update(&state, true);
  CHECK(!state.mainSceneVisible);
  return true;
}

static bool quitFilterAndExit(void) {
  OpenVREvents state;
  reset(&state);
  enqueue(EVREventType_VREvent_ProcessQuit, 77);
  CHECK(!update(&state, true).requestQuit && !state.quitRequested);
  enqueue(EVREventType_VREvent_ProcessQuit, 42);
  CHECK(update(&state, true).requestQuit && state.quitRequested && fake.acknowledgements == 0);
  enqueue(EVREventType_VREvent_Quit, 0);
  CHECK(!update(&state, true).requestQuit && fake.acknowledgements == 0);
  lovrOpenVREventsAcknowledgeExit(&state, &runtime);
  lovrOpenVREventsAcknowledgeExit(&state, &runtime);
  CHECK(fake.acknowledgements == 1);
  reset(&state);
  enqueue(EVREventType_VREvent_Quit, 77);
  CHECK(update(&state, true).requestQuit);
  CHECK(fake.acknowledgements == 0);
  return true;
}

static bool focusActivityAndRestart(void) {
  OpenVREvents state;
  reset(&state);
  fake.input = fake.visible = true;
  update(&state, true);
  fake.pause = true;
  CHECK(update(&state, true).inputFocusChanged && !state.inputFocus && state.visible);
  fake.pause = false;
  fake.input = false;
  CHECK(!update(&state, true).inputFocusChanged && !state.inputFocus);
  const struct { EDeviceActivityLevel activity; bool known; bool active; } levels[] = {
    { EDeviceActivityLevel_k_EDeviceActivityLevel_Unknown, false, false },
    { EDeviceActivityLevel_k_EDeviceActivityLevel_Idle, true, false },
    { EDeviceActivityLevel_k_EDeviceActivityLevel_UserInteraction, true, true },
    { EDeviceActivityLevel_k_EDeviceActivityLevel_UserInteraction_Timeout, true, true },
    { EDeviceActivityLevel_k_EDeviceActivityLevel_Standby, true, false },
    { EDeviceActivityLevel_k_EDeviceActivityLevel_Idle_Timeout, true, false },
    { (EDeviceActivityLevel) 1234, false, false }
  };
  for (size_t i = 0; i < sizeof(levels) / sizeof(levels[0]); i++) {
    fake.activity = levels[i].activity;
    update(&state, true);
    CHECK(state.activityKnown == levels[i].known && state.active == levels[i].active);
  }
  enqueue(EVREventType_VREvent_StandingZeroPoseReset, 0);
  OpenVREventEffects effects = update(&state, true);
  CHECK(effects.invalidateStanding && !effects.invalidateSeated);
  effects = update(&state, true);
  CHECK(!effects.invalidateStanding && !effects.invalidateSeated);
  enqueue(EVREventType_VREvent_ChaperoneUniverseHasChanged, 0);
  enqueue(EVREventType_VREvent_ChaperoneRoomSetupCommitted, 0);
  effects = update(&state, true);
  CHECK(!effects.invalidateStanding && !effects.invalidateSeated);
  fake.input = true;
  fake.activity = EDeviceActivityLevel_k_EDeviceActivityLevel_UserInteraction;
  update(&state, true);
  CHECK(state.inputFocus && state.active && state.visible);
  state.quitRequested = true;
  runtime.initialized = false;
  unsigned int polls = fake.polls;
  effects = update(&state, true);
  CHECK(effects.drained == 0 && fake.polls == polls);
  CHECK(effects.visibilityChanged && effects.inputFocusChanged && effects.activityChanged);
  CHECK(!state.visible && !state.inputFocus && !state.active && !state.activityKnown && !state.sampled);
  CHECK(state.mainSceneVisible && state.quitRequested);
  lovrOpenVREventsStop(&state, &effects);
  CHECK(!effects.visibilityChanged && !effects.inputFocusChanged && !effects.activityChanged);
  CHECK(!effects.mainSceneVisibilityChanged);
  lovrOpenVREventsReset(&state);
  CHECK(!state.sampled && !state.quitRequested && state.mainSceneVisible);
  runtime.initialized = true;
  CHECK(update(&state, true).visibilityChanged);
  return true;
}

static bool ownedOverlayAndBoundedDrain(void) {
  OpenVREvents state;
  reset(&state);
  fake.overlayEvent = (struct VREvent_t) {
    .eventType = EVREventType_VREvent_OverlayShown, .data.overlay.overlayHandle = 77
  };
  fake.overlayPending = true;
  CHECK(update(&state, true).drained == 1);
  CHECK(!state.visible && fake.overlayPolls == 2);
  for (unsigned int i = 0; i < 100; i++) enqueue(EVREventType_VREvent_OverlayHidden, 0);
  OpenVREventEffects effects = update(&state, true);
  CHECK(effects.drained == LOVR_OPENVR_EVENT_LIMIT && effects.drainLimited && state.mainSceneVisible);
  CHECK(fake.next == LOVR_OPENVR_EVENT_LIMIT);
  effects = update(&state, true);
  CHECK(effects.drained == 36 && !effects.drainLimited && !state.mainSceneVisible);
  CHECK(fake.next == fake.count);
  fake.visible = true;
  update(&state, true);
  CHECK(state.visible);
  VROverlayHandle_t other = 77;
  lovrOpenVREventsUpdate(&state, &runtime, true, 42, &other, 1, &effects);
  CHECK(!state.visible);
  reset(&state);
  for (unsigned int i = 0; i < 100; i++) enqueue(EVREventType_VREvent_OverlayHidden, 0);
  fake.overlayEvent = (struct VREvent_t) { .eventType = EVREventType_VREvent_StandingZeroPoseReset };
  fake.overlayPending = true;
  effects = update(&state, true);
  CHECK(effects.drainLimited && effects.invalidateStanding && !fake.overlayPending);
  CHECK(fake.next == LOVR_OPENVR_EVENT_LIMIT - 1);
  effects = update(&state, true);
  CHECK(!effects.drainLimited && fake.next == fake.count && effects.drained == 37);
  reset(&state);
  VROverlayHandle_t busyHandles[] = { 19, 20 };
  for (unsigned int i = 0; i < 200; i++) enqueue(EVREventType_VREvent_OverlayHidden, 0);
  fake.events[30].eventType = EVREventType_VREvent_Quit;
  for (size_t queue = 0; queue < 2; queue++) {
    fake.overlayCount[queue] = 100;
    for (size_t i = 0; i < 100; i++) {
      fake.overlayQueues[queue][i] = (struct VREvent_t) {
        .eventType = EVREventType_VREvent_OverlayHidden, .data.overlay.overlayHandle = busyHandles[queue]
      };
    }
  }
  fake.overlayQueues[0][30].eventType = EVREventType_VREvent_SeatedZeroPoseReset;
  fake.overlayQueues[1][30].eventType = EVREventType_VREvent_StandingZeroPoseReset;
  unsigned int total = 0;
  for (unsigned int turn = 0; turn < 2; turn++) {
    lovrOpenVREventsUpdate(&state, &runtime, true, 42, busyHandles, 2, &effects);
    CHECK(effects.drained == LOVR_OPENVR_EVENT_LIMIT && effects.drainLimited);
    total += effects.drained;
    CHECK(fake.next >= 21 * (turn + 1));
    CHECK(fake.overlayNext[0] >= 21 * (turn + 1) && fake.overlayNext[1] >= 21 * (turn + 1));
    if (turn == 0) {
      CHECK(!effects.requestQuit && !effects.invalidateSeated && !effects.invalidateStanding);
      CHECK(state.pollCursor == 1);
    } else {
      CHECK(effects.requestQuit && effects.invalidateSeated && effects.invalidateStanding);
      CHECK(state.pollCursor == 2);
    }
  }
  for (unsigned int turn = 0; turn < 5; turn++) {
    lovrOpenVREventsUpdate(&state, &runtime, true, 42, busyHandles, 2, &effects);
    total += effects.drained;
  }
  CHECK(total == 400 && fake.next == fake.count);
  CHECK(fake.overlayNext[0] == 100 && fake.overlayNext[1] == 100);
  return true;
}

int main(int argc, char** argv) {
  const NativeTest tests[] = {
    { "manifest_ready_initial_state", manifestReadyAndInitialState },
    { "visibility_state_matrix", visibilityMatrix },
    { "quit_filter_actual_exit", quitFilterAndExit },
    { "focus_activity_recenter_stopstart", focusActivityAndRestart },
    { "owned_overlay_bounded_drain", ownedOverlayAndBoundedDrain }
  };
  return nativeRunTests(argc, argv, tests, sizeof(tests) / sizeof(tests[0]));
}
