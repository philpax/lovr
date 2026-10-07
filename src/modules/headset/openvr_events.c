#include "headset/openvr_events.h"
#include <string.h>

static void reduce(OpenVREvents* state, const struct VREvent_t* event, uint32_t ownPid,
  bool* dashboardActivated, OpenVREventEffects* effects) {
  switch (event->eventType) {
    case EVREventType_VREvent_DashboardActivated:
      *dashboardActivated = true;
      break;
    case EVREventType_VREvent_SeatedZeroPoseReset:
      effects->invalidateSeated = true;
      break;
    case EVREventType_VREvent_StandingZeroPoseReset:
      effects->invalidateStanding = true;
      break;
    case EVREventType_VREvent_Quit:
      if (!state->quitRequested) effects->requestQuit = true;
      state->quitRequested = true;
      break;
    case EVREventType_VREvent_ProcessQuit:
      if (ownPid && event->data.process.pid == ownPid) {
        if (!state->quitRequested) effects->requestQuit = true;
        state->quitRequested = true;
      }
      break;
    default:
      break;
  }
}

void lovrOpenVREventsReset(OpenVREvents* state) {
  memset(state, 0, sizeof(*state));
  state->mainSceneVisible = true;
}

void lovrOpenVREventsStop(OpenVREvents* state, OpenVREventEffects* effects) {
  memset(effects, 0, sizeof(*effects));
  effects->visibilityChanged = state->visible;
  effects->inputFocusChanged = state->inputFocus;
  effects->activityChanged = state->active || state->activityKnown;
  effects->mainSceneVisibilityChanged = !state->mainSceneVisible;
  state->sampled = false;
  state->pollCursor = 0;
  state->visible = false;
  state->inputFocus = false;
  state->activityKnown = false;
  state->active = false;
  state->mainSceneVisible = true;
}

void lovrOpenVREventsUpdate(OpenVREvents* state, const OpenVRRuntime* runtime, bool inputReady,
  uint32_t ownPid, const VROverlayHandle_t* handles, size_t handleCount, OpenVREventEffects* effects) {
  memset(effects, 0, sizeof(*effects));
  if (!runtime || !runtime->initialized || !inputReady) {
    lovrOpenVREventsStop(state, effects);
    return;
  }
  if (!handles) handleCount = 0;
  struct VR_IVRSystem_FnTable* system = runtime->system;
  struct VR_IVROverlay_FnTable* overlay = runtime->overlay;
  struct VR_IVRCompositor_FnTable* compositor = runtime->compositor;
  struct VR_IVRApplications_FnTable* applications = runtime->applications;
  bool dashboardActivated = false;
  struct VREvent_t event;
  size_t sources = handleCount + 1;
  size_t empty = 0;
  state->pollCursor %= sources;
  while (effects->drained < LOVR_OPENVR_EVENT_LIMIT && empty < sources) {
    size_t source = state->pollCursor;
    state->pollCursor = (source + 1) % sources;
    VROverlayHandle_t handle = source ? handles[source - 1] : 0;
    bool received = source == 0 ? system && system->PollNextEvent && system->PollNextEvent(&event, sizeof(event)) :
      handle && overlay && overlay->PollNextOverlayEvent && overlay->PollNextOverlayEvent(handle, &event, sizeof(event));
    if (!received) {
      empty++;
      continue;
    }
    empty = 0;
    effects->drained++;
    if (source && (event.eventType == EVREventType_VREvent_OverlayShown ||
                   event.eventType == EVREventType_VREvent_OverlayHidden) &&
        event.data.overlay.overlayHandle != handle) continue;
    reduce(state, &event, ownPid, &dashboardActivated, effects);
  }
  effects->drainLimited = effects->drained == LOVR_OPENVR_EVENT_LIMIT;
  bool visible = false;
  if (overlay && overlay->IsOverlayVisible) {
    for (size_t i = 0; i < handleCount; i++) {
      if (handles[i]) visible |= overlay->IsOverlayVisible(handles[i]);
    }
  }
  bool inputFocus = system && system->IsInputAvailable && system->ShouldApplicationPause &&
    system->IsInputAvailable() && !system->ShouldApplicationPause();
  EDeviceActivityLevel activity = system && system->GetTrackedDeviceActivityLevel ?
    system->GetTrackedDeviceActivityLevel(k_unTrackedDeviceIndex_Hmd) : EDeviceActivityLevel_k_EDeviceActivityLevel_Unknown;
  bool active = activity == EDeviceActivityLevel_k_EDeviceActivityLevel_UserInteraction ||
    activity == EDeviceActivityLevel_k_EDeviceActivityLevel_UserInteraction_Timeout;
  bool activityKnown = active || activity == EDeviceActivityLevel_k_EDeviceActivityLevel_Standby ||
    activity == EDeviceActivityLevel_k_EDeviceActivityLevel_Idle ||
    activity == EDeviceActivityLevel_k_EDeviceActivityLevel_Idle_Timeout;
  bool scene = true;
  if (!effects->drainLimited && !dashboardActivated &&
      overlay && overlay->IsDashboardVisible && !overlay->IsDashboardVisible() &&
      compositor && compositor->GetLastFrameRenderer && compositor->GetCurrentSceneFocusProcess &&
      applications && applications->GetCurrentSceneProcessId && applications->GetSceneApplicationState) {
    scene = compositor->GetLastFrameRenderer() != 0 || compositor->GetCurrentSceneFocusProcess() != 0 ||
      applications->GetCurrentSceneProcessId() != 0 ||
      applications->GetSceneApplicationState() != EVRSceneApplicationState_None;
  }
  effects->visibilityChanged = !state->sampled || state->visible != visible;
  effects->inputFocusChanged = !state->sampled || state->inputFocus != inputFocus;
  effects->activityChanged = !state->sampled || state->activityKnown != activityKnown || state->active != active;
  effects->mainSceneVisibilityChanged = !state->sampled || state->mainSceneVisible != scene;
  state->sampled = true;
  state->visible = visible;
  state->inputFocus = inputFocus;
  state->activityKnown = activityKnown;
  state->active = active;
  state->mainSceneVisible = scene;
}

void lovrOpenVREventsAcknowledgeExit(OpenVREvents* state, const OpenVRRuntime* runtime) {
  if (state->quitRequested && !state->quitAcknowledged && runtime && runtime->initialized &&
      runtime->system && runtime->system->AcknowledgeQuit_Exiting) {
    runtime->system->AcknowledgeQuit_Exiting();
    state->quitAcknowledged = true;
  }
}
