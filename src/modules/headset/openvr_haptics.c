#include "openvr_haptics.h"
#include <math.h>
#include <string.h>

static EVRInputError prepare(OpenVRHaptics* haptics, uint64_t session, double now);
static EVRInputError emit(OpenVRHapticsSchedule* schedule, Device device, double now,
  OpenVRHapticsPulse pulse, void* userdata);
static int getHand(Device device);

EVRInputError lovrOpenVRHapticsSchedule(OpenVRHaptics* haptics, uint64_t session, Device device,
    float strength, float duration, float frequency, double now, OpenVRHapticsPulse pulse, void* userdata) {
  EVRInputError error = prepare(haptics, session, now);
  if (error) return error;
  int hand = getHand(device);
  if (hand < 0) return EVRInputError_VRInputError_InvalidDevice;
  if (!pulse || !isfinite(strength) || strength < 0.f || strength > 1.f ||
      !isfinite(duration) || duration < 0.f || !isfinite(frequency) || frequency < 0.f) {
    return EVRInputError_VRInputError_InvalidParam;
  }
  double deadline = now + (double) duration;
  if (!isfinite(deadline) || (duration > 0.f && deadline <= now)) {
    lovrOpenVRHapticsClear(haptics);
    return EVRInputError_VRInputError_InvalidParam;
  }
  OpenVRHapticsSchedule* schedule = &haptics->hands[hand];
  *schedule = (OpenVRHapticsSchedule) { 0 };
  if (strength == 0.f || duration == 0.f) return EVRInputError_VRInputError_None;
  *schedule = (OpenVRHapticsSchedule) {
    .active = true,
    .strength = strength,
    .frequency = frequency == 0.f ? OPENVR_HAPTICS_DEFAULT_FREQUENCY : frequency,
    .deadline = deadline,
    .nextPulse = now
  };
  error = emit(schedule, device, now, pulse, userdata);
  if (error) *schedule = (OpenVRHapticsSchedule) { 0 };
  return error;
}

EVRInputError lovrOpenVRHapticsTick(OpenVRHaptics* haptics, uint64_t session, double now,
    OpenVRHapticsPulse pulse, void* userdata) {
  EVRInputError error = prepare(haptics, session, now);
  if (error) return error;
  if (!pulse) {
    lovrOpenVRHapticsClear(haptics);
    return EVRInputError_VRInputError_InvalidParam;
  }
  for (int hand = 0; hand < 2; hand++) {
    OpenVRHapticsSchedule* schedule = &haptics->hands[hand];
    if (!schedule->active) continue;
    if (now >= schedule->deadline) {
      *schedule = (OpenVRHapticsSchedule) { 0 };
      continue;
    }
    if (now < schedule->nextPulse) continue;
    error = emit(schedule, hand == 0 ? DEVICE_HAND_LEFT : DEVICE_HAND_RIGHT, now, pulse, userdata);
    if (error) {
      lovrOpenVRHapticsClear(haptics);
      return error;
    }
  }
  return EVRInputError_VRInputError_None;
}

EVRInputError lovrOpenVRHapticsCancel(OpenVRHaptics* haptics, Device device) {
  if (!haptics) return EVRInputError_VRInputError_InvalidHandle;
  int hand = getHand(device);
  if (hand < 0) return EVRInputError_VRInputError_InvalidDevice;
  haptics->hands[hand] = (OpenVRHapticsSchedule) { 0 };
  return EVRInputError_VRInputError_None;
}

void lovrOpenVRHapticsClear(OpenVRHaptics* haptics) {
  if (haptics) memset(haptics, 0, sizeof(*haptics));
}

static EVRInputError prepare(OpenVRHaptics* haptics, uint64_t session, double now) {
  if (!haptics) return EVRInputError_VRInputError_InvalidHandle;
  if (!session) {
    lovrOpenVRHapticsClear(haptics);
    return EVRInputError_VRInputError_InvalidHandle;
  }
  if (haptics->session != session) {
    lovrOpenVRHapticsClear(haptics);
    haptics->session = session;
  }
  if (!isfinite(now) || now < 0. || now + (double) OPENVR_HAPTICS_MAX_PULSE_SECONDS <= now ||
      (haptics->hasTime && now < haptics->lastNow)) {
    lovrOpenVRHapticsClear(haptics);
    return EVRInputError_VRInputError_InvalidParam;
  }
  haptics->lastNow = now;
  haptics->hasTime = true;
  return EVRInputError_VRInputError_None;
}

static EVRInputError emit(OpenVRHapticsSchedule* schedule, Device device, double now,
    OpenVRHapticsPulse pulse, void* userdata) {
  double remaining = schedule->deadline - now;
  float duration = (float) fmin(remaining, (double) OPENVR_HAPTICS_MAX_PULSE_SECONDS);
  if ((double) duration > remaining) duration = nextafterf(duration, 0.f);
  if (duration <= 0.f || now + (double) duration <= now) return EVRInputError_VRInputError_InvalidParam;
  EVRInputError error = pulse(userdata, device, schedule->strength, duration, schedule->frequency);
  if (!error) schedule->nextPulse = now + (double) duration;
  return error;
}

static int getHand(Device device) {
  return device == DEVICE_HAND_LEFT ? 0 : device == DEVICE_HAND_RIGHT ? 1 : -1;
}
