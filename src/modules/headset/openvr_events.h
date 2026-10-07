#ifndef LOVR_OPENVR_EVENTS_H
#define LOVR_OPENVR_EVENTS_H

#include "headset/openvr_runtime.h"
#include <stddef.h>

#define LOVR_OPENVR_EVENT_LIMIT 64u

typedef struct {
  bool sampled;
  bool visible;
  bool inputFocus;
  bool activityKnown;
  bool active;
  bool mainSceneVisible;
  bool quitRequested;
  bool quitAcknowledged;
  size_t pollCursor;
} OpenVREvents;

typedef struct {
  bool visibilityChanged;
  bool inputFocusChanged;
  bool activityChanged;
  bool mainSceneVisibilityChanged;
  bool invalidateSeated;
  bool invalidateStanding;
  bool requestQuit;
  bool drainLimited;
  unsigned int drained;
} OpenVREventEffects;

void lovrOpenVREventsReset(OpenVREvents* state);
void lovrOpenVREventsStop(OpenVREvents* state, OpenVREventEffects* effects);
void lovrOpenVREventsUpdate(OpenVREvents* state, const OpenVRRuntime* runtime, bool inputReady,
  uint32_t ownPid, const VROverlayHandle_t* handles, size_t handleCount, OpenVREventEffects* effects);
void lovrOpenVREventsAcknowledgeExit(OpenVREvents* state, const OpenVRRuntime* runtime);

#endif
