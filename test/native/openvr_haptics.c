#include "test.h"
#include "headset/openvr_haptics.h"
#include <float.h>
#include <math.h>

typedef struct {
  unsigned calls;
  unsigned failAt;
  Device devices[32];
  float strengths[32];
  float durations[32];
  float frequencies[32];
} FakePulse;

static EVRInputError pulse(void* userdata, Device device, float strength, float duration, float frequency) {
  FakePulse* fake = userdata;
  unsigned index = fake->calls++;
  if (index >= 32) return EVRInputError_VRInputError_InvalidParam;
  fake->devices[index] = device;
  fake->strengths[index] = strength;
  fake->durations[index] = duration;
  fake->frequencies[index] = frequency;
  return fake->calls == fake->failAt ? EVRInputError_VRInputError_NoData : EVRInputError_VRInputError_None;
}

static bool routing(void) {
  OpenVRHaptics haptics = { 0 };
  FakePulse fake = { 0 };
  CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HAND_LEFT, .4f, 1.f, 0.f, 0., pulse, &fake) == 0);
  CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HAND_RIGHT, .7f, .03f, 500.f, 0., pulse, &fake) == 0);
  CHECK(fake.calls == 2 && fake.devices[0] == DEVICE_HAND_LEFT && fake.devices[1] == DEVICE_HAND_RIGHT);
  CHECK(fake.strengths[0] == .4f && fake.strengths[1] == .7f);
  CHECK(fake.frequencies[0] == OPENVR_HAPTICS_DEFAULT_FREQUENCY && fake.frequencies[1] == 500.f);
  CHECK(fake.durations[0] == OPENVR_HAPTICS_MAX_PULSE_SECONDS);
  CHECK(lovrOpenVRHapticsTick(&haptics, 1, .01, pulse, &fake) == 0 && fake.calls == 2);
  CHECK(lovrOpenVRHapticsTick(&haptics, 1, .021, pulse, &fake) == 0 && fake.calls == 4);
  CHECK(fake.devices[2] == DEVICE_HAND_LEFT && fake.devices[3] == DEVICE_HAND_RIGHT);
  CHECK(fake.durations[3] > 0.f && fake.durations[3] < .01f);
  CHECK(lovrOpenVRHapticsTick(&haptics, 1, 5., pulse, &fake) == 0 && fake.calls == 4);
  CHECK(!haptics.hands[0].active && !haptics.hands[1].active);
  return true;
}

static bool replacementAndCancellation(void) {
  OpenVRHaptics haptics = { 0 };
  FakePulse fake = { 0 };
  CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HAND_LEFT, 1.f, 10.f, 50.f, 0., pulse, &fake) == 0);
  CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HAND_LEFT, .2f, .01f, 90.f, .001, pulse, &fake) == 0);
  CHECK(fake.calls == 2 && fake.strengths[1] == .2f);
  CHECK(lovrOpenVRHapticsTick(&haptics, 1, .03, pulse, &fake) == 0 && fake.calls == 2);
  CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HAND_RIGHT, 1.f, 10.f, 1.f, .03, pulse, &fake) == 0);
  CHECK(lovrOpenVRHapticsCancel(&haptics, DEVICE_HAND_RIGHT) == 0);
  CHECK(lovrOpenVRHapticsCancel(&haptics, DEVICE_HEAD) == EVRInputError_VRInputError_InvalidDevice);
  CHECK(lovrOpenVRHapticsTick(&haptics, 1, .1, pulse, &fake) == 0 && fake.calls == 3);
  CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HAND_LEFT, 1.f, 10.f, 1.f, .1, pulse, &fake) == 0);
  CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HAND_LEFT, 0.f, 10.f, 1.f, .1, pulse, &fake) == 0);
  CHECK(fake.calls == 4 && !haptics.hands[0].active);
  CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HAND_LEFT, 1.f, 0.f, 1.f, .1, pulse, &fake) == 0);
  CHECK(fake.calls == 4);
  lovrOpenVRHapticsClear(&haptics);
  lovrOpenVRHapticsClear(&haptics);
  CHECK(!haptics.session && !haptics.hasTime);
  CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HAND_LEFT, .3f, 10.f, 30.f, 1., pulse, &fake) == 0);
  CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HAND_RIGHT, .6f, 10.f, 60.f, 1., pulse, &fake) == 0);
  CHECK(lovrOpenVRHapticsCancel(&haptics, DEVICE_HAND_LEFT) == 0);
  CHECK(!haptics.hands[0].active && haptics.hands[1].active);
  CHECK(haptics.hands[1].strength == .6f && haptics.hands[1].frequency == 60.f);
  CHECK(lovrOpenVRHapticsTick(&haptics, 1, 1.03, pulse, &fake) == 0 && fake.calls == 7);
  CHECK(fake.devices[6] == DEVICE_HAND_RIGHT && fake.strengths[6] == .6f && fake.frequencies[6] == 60.f);
  CHECK(!haptics.hands[0].active && haptics.hands[1].active);
  return true;
}

static bool sessionReset(void) {
  OpenVRHaptics haptics = { 0 };
  FakePulse fake = { 0 };
  CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HAND_LEFT, 1.f, 10.f, 1.f, 20., pulse, &fake) == 0);
  CHECK(lovrOpenVRHapticsTick(&haptics, 2, 0., pulse, &fake) == 0 && fake.calls == 1);
  CHECK(haptics.session == 2 && !haptics.hands[0].active);
  CHECK(lovrOpenVRHapticsSchedule(&haptics, 2, DEVICE_HAND_RIGHT, 1.f, 10.f, 1.f, 0., pulse, &fake) == 0);
  CHECK(lovrOpenVRHapticsTick(&haptics, 0, .1, pulse, &fake) == EVRInputError_VRInputError_InvalidHandle);
  CHECK(!haptics.hands[1].active && fake.calls == 2);
  return true;
}

static bool errors(void) {
  OpenVRHaptics haptics = { 0 };
  FakePulse fake = { .failAt = 1 };
  CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HAND_LEFT, 1.f, 10.f, 1.f, 0., pulse, &fake) == EVRInputError_VRInputError_NoData);
  CHECK(!haptics.hands[0].active);
  fake.failAt = 0;
  CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HAND_LEFT, 1.f, 10.f, 1.f, 0., pulse, &fake) == 0);
  CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HAND_RIGHT, 1.f, 10.f, 1.f, 0., pulse, &fake) == 0);
  fake.failAt = 4;
  CHECK(lovrOpenVRHapticsTick(&haptics, 1, .03, pulse, &fake) == EVRInputError_VRInputError_NoData);
  CHECK(!haptics.hands[0].active && !haptics.hands[1].active && fake.calls == 4);
  CHECK(lovrOpenVRHapticsTick(&haptics, 1, .1, pulse, &fake) == 0 && fake.calls == 4);
  fake.failAt = 8;
  CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HAND_LEFT, .3f, 10.f, 30.f, .1, pulse, &fake) == 0);
  CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HAND_RIGHT, .6f, 10.f, 60.f, .1, pulse, &fake) == 0);
  CHECK(lovrOpenVRHapticsTick(&haptics, 1, .13, pulse, &fake) == EVRInputError_VRInputError_NoData);
  CHECK(fake.calls == 8 && fake.devices[6] == DEVICE_HAND_LEFT && fake.devices[7] == DEVICE_HAND_RIGHT);
  CHECK(fake.strengths[6] == .3f && fake.strengths[7] == .6f);
  CHECK(!haptics.hands[0].active && !haptics.hands[1].active);
  CHECK(lovrOpenVRHapticsTick(&haptics, 1, .2, pulse, &fake) == 0 && fake.calls == 8);
  return true;
}

static bool boundedPulses(void) {
  OpenVRHaptics haptics = { 0 };
  FakePulse fake = { 0 };
  CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HAND_LEFT, 1.f, FLT_MAX, 1.f, 0., pulse, &fake) == 0);
  CHECK(lovrOpenVRHapticsTick(&haptics, 1, 100., pulse, &fake) == 0 && fake.calls == 2);
  CHECK(lovrOpenVRHapticsTick(&haptics, 1, 100., pulse, &fake) == 0 && fake.calls == 2);
  CHECK(fake.durations[0] == OPENVR_HAPTICS_MAX_PULSE_SECONDS && fake.durations[1] == OPENVR_HAPTICS_MAX_PULSE_SECONDS);
  fake.failAt = 3;
  CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HAND_LEFT, .5f, 1.f, 10.f, 100., pulse, &fake) == EVRInputError_VRInputError_NoData);
  CHECK(!haptics.hands[0].active);
  CHECK(lovrOpenVRHapticsTick(&haptics, 1, 101., pulse, &fake) == 0 && fake.calls == 3);
  fake.failAt = 0;
  CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HAND_RIGHT, 1.f, .001f, 1.f, 101., pulse, &fake) == 0);
  CHECK(fake.durations[3] > 0.f && fake.durations[3] <= .001f);
  CHECK(lovrOpenVRHapticsTick(&haptics, 1, 101.01, pulse, &fake) == 0 && fake.calls == 4);
  CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HAND_LEFT, 1.f, 10.f, 1.f, 102., pulse, &fake) == 0);
  CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HAND_RIGHT, 1.f, 10.f, 1.f, 102., pulse, &fake) == 0);
  lovrOpenVRHapticsClear(&haptics);
  CHECK(lovrOpenVRHapticsTick(&haptics, 1, 103., pulse, &fake) == 0 && fake.calls == 6);
  return true;
}

static bool invalidParameters(void) {
  OpenVRHaptics haptics = { 0 };
  FakePulse fake = { 0 };
  const float invalid[][3] = {
    { NAN, 1.f, 1.f }, { INFINITY, 1.f, 1.f }, { -.1f, 1.f, 1.f }, { 1.1f, 1.f, 1.f },
    { 1.f, NAN, 1.f }, { 1.f, INFINITY, 1.f }, { 1.f, -1.f, 1.f },
    { 1.f, 1.f, NAN }, { 1.f, 1.f, INFINITY }, { 1.f, 1.f, -1.f }
  };
  for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
    CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HAND_LEFT, invalid[i][0], invalid[i][1], invalid[i][2], 0., pulse, &fake) == EVRInputError_VRInputError_InvalidParam);
  }
  CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HEAD, 1.f, 1.f, 1.f, 0., pulse, &fake) == EVRInputError_VRInputError_InvalidDevice);
  CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HAND_LEFT, 1.f, 1.f, 1.f, 0., NULL, &fake) == EVRInputError_VRInputError_InvalidParam);
  CHECK(lovrOpenVRHapticsTick(NULL, 1, 0., pulse, &fake) == EVRInputError_VRInputError_InvalidHandle);
  CHECK(lovrOpenVRHapticsCancel(NULL, DEVICE_HAND_LEFT) == EVRInputError_VRInputError_InvalidHandle);
  lovrOpenVRHapticsClear(NULL);
  CHECK(fake.calls == 0);
  return true;
}

static bool invalidTime(void) {
  const double invalid[] = { NAN, INFINITY, -1., DBL_MAX };
  for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
    OpenVRHaptics haptics = { 0 };
    FakePulse fake = { 0 };
    CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HAND_LEFT, 1.f, 1.f, 1.f, invalid[i], pulse, &fake) == EVRInputError_VRInputError_InvalidParam);
    CHECK(fake.calls == 0);
  }
  OpenVRHaptics haptics = { 0 };
  FakePulse fake = { 0 };
  CHECK(lovrOpenVRHapticsSchedule(&haptics, 1, DEVICE_HAND_LEFT, 1.f, 10.f, 1.f, 10., pulse, &fake) == 0);
  CHECK(lovrOpenVRHapticsTick(&haptics, 1, 9., pulse, &fake) == EVRInputError_VRInputError_InvalidParam);
  CHECK(!haptics.hands[0].active && fake.calls == 1);
  CHECK(lovrOpenVRHapticsTick(&haptics, 1, NAN, pulse, &fake) == EVRInputError_VRInputError_InvalidParam);
  return true;
}

int main(int argc, char** argv) {
  const NativeTest tests[] = {
    { "haptic_action_routing", routing },
    { "haptic_replacement_cancel", replacementAndCancellation },
    { "haptic_session_reset", sessionReset },
    { "haptic_error_propagation", errors },
    { "haptic_bounded_pulses", boundedPulses },
    { "haptic_invalid_parameters", invalidParameters },
    { "haptic_invalid_time", invalidTime }
  };
  return nativeRunTests(argc, argv, tests, sizeof(tests) / sizeof(tests[0]));
}
