#ifndef LOVR_OPENVR_HAPTICS_H
#define LOVR_OPENVR_HAPTICS_H

#include "headset.h"
#include <openvr_capi.h>

#define OPENVR_HAPTICS_MAX_PULSE_SECONDS .02f
#define OPENVR_HAPTICS_DEFAULT_FREQUENCY 120.f

typedef EVRInputError (*OpenVRHapticsPulse)(void* userdata, Device device, float strength, float duration, float frequency);

typedef struct OpenVRHapticsSchedule {
  bool active;
  float strength;
  float frequency;
  double deadline;
  double nextPulse;
} OpenVRHapticsSchedule;

typedef struct OpenVRHaptics {
  OpenVRHapticsSchedule hands[2];
  uint64_t session;
  double lastNow;
  bool hasTime;
} OpenVRHaptics;

EVRInputError lovrOpenVRHapticsSchedule(OpenVRHaptics* haptics, uint64_t session, Device device,
  float strength, float duration, float frequency, double now, OpenVRHapticsPulse pulse, void* userdata);
EVRInputError lovrOpenVRHapticsTick(OpenVRHaptics* haptics, uint64_t session, double now,
  OpenVRHapticsPulse pulse, void* userdata);
EVRInputError lovrOpenVRHapticsCancel(OpenVRHaptics* haptics, Device device);
void lovrOpenVRHapticsClear(OpenVRHaptics* haptics);

#endif
