#include "test.h"
#include "headset/openvr_input.h"
#include <math.h>

static struct {
  unsigned calls, failAt, zeroAt, updates, reads, failRead, haptics;
  bool valid, active, connected, invalidPose, down, changed;
  EVRInputError updateError, readError, hapticError;
  VRActionHandle_t haptic;
  VRInputValueHandle_t restriction;
  float strength, duration, frequency;
} fake;

static EVRInputError step(void) {
  fake.calls++;
  return fake.calls == fake.failAt ? EVRInputError_VRInputError_NameNotFound : EVRInputError_VRInputError_None;
}

static EVRInputError OPENVR_FNTABLE_CALLTYPE manifest(char* path) {
  fake.valid &= fake.calls == 0 && strcmp(path, "/installed/actions.json") == 0;
  return step();
}

static EVRInputError OPENVR_FNTABLE_CALLTYPE actionSet(char* path, VRActionSetHandle_t* handle) {
  fake.valid &= fake.calls == 1 && strcmp(path, "/actions/lovr") == 0;
  *handle = 100;
  return step();
}

static EVRInputError OPENVR_FNTABLE_CALLTYPE action(char* path, VRActionHandle_t* handle) {
  unsigned index = fake.calls - 2;
  fake.valid &= index < OPENVR_INPUT_ACTION_COUNT && strcmp(path, lovrOpenVRInputActionPath(index)) == 0;
  *handle = fake.calls + 1 == fake.zeroAt ? 0 : 200 + index;
  return step();
}

static EVRInputError OPENVR_FNTABLE_CALLTYPE source(char* path, VRInputValueHandle_t* handle) {
  unsigned hand = fake.calls - 2 - OPENVR_INPUT_ACTION_COUNT;
  fake.valid &= hand < 2 && strcmp(path, hand == 0 ? "/user/hand/left" : "/user/hand/right") == 0;
  *handle = 300 + hand;
  return step();
}

static EVRInputError OPENVR_FNTABLE_CALLTYPE update(VRActiveActionSet_t* sets, uint32_t size, uint32_t count) {
  fake.valid &= size == sizeof(*sets) && count == 1 && sets->ulActionSet == 100;
  fake.valid &= sets->ulRestrictedToDevice == 0 && sets->ulSecondaryActionSet == 0 && sets->nPriority == 0;
  fake.updates++;
  fake.reads = 0;
  return fake.updateError;
}

static void readCheck(VRActionHandle_t handle, VRInputValueHandle_t restriction, unsigned start, unsigned count) {
  unsigned perHand = 2 + 2 * OPENVR_INPUT_BUTTON_COUNT + OPENVR_INPUT_AXIS_COUNT;
  unsigned hand = fake.reads / perHand;
  unsigned slot = fake.reads % perHand;
  fake.valid &= fake.updates > 0 && restriction == 300 + hand && handle >= 200 + start && handle < 200 + start + count;
  fake.valid &= handle == 200 + slot;
  fake.reads++;
}

static EVRInputError OPENVR_FNTABLE_CALLTYPE pose(VRActionHandle_t handle, ETrackingUniverseOrigin origin,
    float prediction, InputPoseActionData_t* data, uint32_t size, VRInputValueHandle_t restriction) {
  readCheck(handle, restriction, 0, 2);
  fake.valid &= size == sizeof(*data) && origin == ETrackingUniverseOrigin_TrackingUniverseStanding && prediction == .02f;
  data->bActive = fake.active;
  data->pose.bDeviceIsConnected = fake.connected;
  data->pose.bPoseIsValid = !fake.invalidPose;
  data->pose.mDeviceToAbsoluteTracking = (HmdMatrix34_t) { .m = { { 0, 0, 1, 2 }, { 0, 1, 0, 3 }, { -1, 0, 0, 4 } } };
  data->pose.vVelocity = (HmdVector3_t) { .v = { 5, 6, 7 } };
  data->pose.vAngularVelocity = (HmdVector3_t) { .v = { 8, 9, 10 } };
  return fake.failRead && fake.reads == fake.failRead ? EVRInputError_VRInputError_IPCError : fake.readError;
}

static EVRInputError OPENVR_FNTABLE_CALLTYPE digital(VRActionHandle_t handle, InputDigitalActionData_t* data,
    uint32_t size, VRInputValueHandle_t restriction) {
  readCheck(handle, restriction, 2, 2 * OPENVR_INPUT_BUTTON_COUNT);
  fake.valid &= size == sizeof(*data);
  data->bActive = fake.active;
  data->bState = fake.down;
  data->bChanged = fake.changed;
  return fake.failRead && fake.reads == fake.failRead ? EVRInputError_VRInputError_IPCError : fake.readError;
}

static EVRInputError OPENVR_FNTABLE_CALLTYPE analog(VRActionHandle_t handle, InputAnalogActionData_t* data,
    uint32_t size, VRInputValueHandle_t restriction) {
  readCheck(handle, restriction, 2 + 2 * OPENVR_INPUT_BUTTON_COUNT, OPENVR_INPUT_AXIS_COUNT);
  fake.valid &= size == sizeof(*data);
  data->bActive = fake.active;
  data->x = .7f;
  data->y = -.3f;
  return fake.failRead && fake.reads == fake.failRead ? EVRInputError_VRInputError_IPCError : fake.readError;
}

static EVRInputError OPENVR_FNTABLE_CALLTYPE haptic(VRActionHandle_t handle, float start, float duration,
    float frequency, float amplitude, VRInputValueHandle_t restriction) {
  fake.valid &= start == 0.f;
  fake.haptics++;
  fake.haptic = handle;
  fake.restriction = restriction;
  fake.strength = amplitude;
  fake.duration = duration;
  fake.frequency = frequency;
  return fake.hapticError;
}

static struct VR_IVRInput_FnTable api = {
  .SetActionManifestPath = manifest, .GetActionSetHandle = actionSet, .GetActionHandle = action,
  .GetInputSourceHandle = source, .UpdateActionState = update, .GetDigitalActionData = digital,
  .GetAnalogActionData = analog, .GetPoseActionDataRelativeToNow = pose, .TriggerHapticVibrationAction = haptic
};

static void resetFake(void) {
  memset(&fake, 0, sizeof(fake));
  fake.valid = fake.active = fake.connected = true;
}

static bool initialization(void) {
  OpenVRInput input;
  for (unsigned failure = 1; failure <= 4 + OPENVR_INPUT_ACTION_COUNT; failure++) {
    resetFake();
    fake.failAt = failure;
    memset(&input, 0xff, sizeof(input));
    CHECK(lovrOpenVRInputInit(&input, &api, "/installed/actions.json") == EVRInputError_VRInputError_NameNotFound);
    OpenVRInput empty = { 0 };
    CHECK(memcmp(&input, &empty, sizeof(input)) == 0);
    CHECK(fake.calls == failure && fake.valid && fake.updates == 0);
  }
  resetFake();
  CHECK(lovrOpenVRInputInit(&input, &api, "relative.json") == EVRInputError_VRInputError_InvalidParam);
  CHECK(fake.calls == 0);
  CHECK(lovrOpenVRInputInit(&input, &api, "/installed/actions.json") == 0);
  CHECK(fake.valid && fake.calls == 4 + OPENVR_INPUT_ACTION_COUNT);
  CHECK(strcmp(lovrOpenVRInputActionPath(0), "/actions/lovr/in/grip_pose") == 0);
  CHECK(strcmp(lovrOpenVRInputActionPath(1), "/actions/lovr/in/aim_pose") == 0);
  CHECK(strcmp(lovrOpenVRInputActionPath(2), "/actions/lovr/in/trigger") == 0);
  CHECK(strcmp(lovrOpenVRInputActionPath(OPENVR_INPUT_ACTION_COUNT - 1), "/actions/lovr/out/haptic") == 0);
  CHECK(lovrOpenVRInputActionPath(OPENVR_INPUT_ACTION_COUNT) == NULL);
  resetFake();
  fake.zeroAt = 3;
  CHECK(lovrOpenVRInputInit(&input, &api, "/installed/actions.json") == EVRInputError_VRInputError_InvalidHandle);
  CHECK(input.api == NULL && input.actionSet == 0 && input.actions[0] == 0);
  struct VR_IVRInput_FnTable incomplete = api;
  incomplete.GetAnalogActionData = NULL;
  CHECK(lovrOpenVRInputInit(&input, &incomplete, "/installed/actions.json") == EVRInputError_VRInputError_InvalidParam);
  lovrOpenVRInputReset(&input);
  CHECK(lovrOpenVRInputUpdate(&input, ETrackingUniverseOrigin_TrackingUniverseStanding, .02f) == EVRInputError_VRInputError_InvalidHandle);
  return true;
}

static bool snapshots(void) {
  resetFake();
  OpenVRInput input;
  CHECK(lovrOpenVRInputInit(&input, &api, "/installed/actions.json") == 0);
  bool down, changed, touched;
  float axis[2], p[3], q[4], v[3], w[3];
  fake.down = fake.changed = true;
  CHECK(lovrOpenVRInputUpdate(&input, ETrackingUniverseOrigin_TrackingUniverseStanding, .02f) == 0);
  CHECK(fake.valid && fake.updates == 1 && fake.reads == 2 * (OPENVR_INPUT_ACTION_COUNT - 1));
  CHECK(lovrOpenVRInputIsDown(&input, DEVICE_HAND_LEFT, BUTTON_TRIGGER, &down, &changed) && down && changed);
  CHECK(lovrOpenVRInputIsDown(&input, DEVICE_HAND_LEFT, BUTTON_TRIGGER, &down, &changed) && down && changed);
  CHECK(lovrOpenVRInputIsTouched(&input, DEVICE_HAND_RIGHT, BUTTON_TRIGGER, &touched) && touched);
  CHECK(lovrOpenVRInputGetAxis(&input, DEVICE_HAND_LEFT, AXIS_THUMBSTICK, axis) && axis[0] == .7f && axis[1] == -.3f);
  CHECK(lovrOpenVRInputGetPose(&input, DEVICE_HAND_RIGHT_POINT, p, q));
  CHECK(p[0] == 2 && p[1] == 3 && p[2] == 4 && fabsf(q[1] - sqrtf(.5f)) < 1e-5f && fabsf(q[3] - sqrtf(.5f)) < 1e-5f);
  CHECK(lovrOpenVRInputGetVelocity(&input, DEVICE_HAND_LEFT_GRIP, v, w) && v[0] == 5 && v[2] == 7 && w[0] == 8 && w[2] == 10);
  fake.invalidPose = true;
  CHECK(lovrOpenVRInputUpdate(&input, ETrackingUniverseOrigin_TrackingUniverseStanding, .02f) == 0);
  CHECK(!lovrOpenVRInputGetPose(&input, DEVICE_HAND_LEFT, p, q) && p[0] == 0 && q[3] == 1);
  CHECK(!lovrOpenVRInputGetVelocity(&input, DEVICE_HAND_RIGHT_POINT, v, w) && v[0] == 0 && w[0] == 0);
  CHECK(lovrOpenVRInputIsDown(&input, DEVICE_HAND_LEFT, BUTTON_TRIGGER, &down, &changed) && down);
  fake.invalidPose = false;
  lovrOpenVRInputClear(&input);
  CHECK(input.api == &api && input.actionSet == 100 && input.sources[1] == 301);
  CHECK(!lovrOpenVRInputIsDown(&input, DEVICE_HAND_RIGHT, BUTTON_TRIGGER, &down, &changed));
  fake.changed = false;
  CHECK(lovrOpenVRInputUpdate(&input, ETrackingUniverseOrigin_TrackingUniverseStanding, .02f) == 0);
  CHECK(lovrOpenVRInputIsDown(&input, DEVICE_HAND_LEFT, BUTTON_TRIGGER, &down, &changed) && down && !changed);
  fake.down = false;
  fake.changed = true;
  CHECK(lovrOpenVRInputUpdate(&input, ETrackingUniverseOrigin_TrackingUniverseStanding, .02f) == 0);
  CHECK(lovrOpenVRInputIsDown(&input, DEVICE_HAND_LEFT, BUTTON_TRIGGER, &down, &changed) && !down && changed);
  fake.connected = false;
  CHECK(lovrOpenVRInputUpdate(&input, ETrackingUniverseOrigin_TrackingUniverseStanding, .02f) == 0);
  CHECK(!lovrOpenVRInputIsDown(&input, DEVICE_HAND_LEFT, BUTTON_TRIGGER, &down, &changed) && !down && !changed);
  CHECK(!lovrOpenVRInputGetAxis(&input, DEVICE_HAND_LEFT, AXIS_THUMBSTICK, axis) && axis[0] == 0 && axis[1] == 0);
  CHECK(!lovrOpenVRInputGetPose(&input, DEVICE_HAND_LEFT, p, q) && p[0] == 0 && q[3] == 1);
  fake.connected = true;
  fake.readError = EVRInputError_VRInputError_IPCError;
  CHECK(lovrOpenVRInputUpdate(&input, ETrackingUniverseOrigin_TrackingUniverseStanding, .02f) == fake.readError);
  CHECK(!lovrOpenVRInputGetAxis(&input, DEVICE_HAND_LEFT, AXIS_TRIGGER, axis) && axis[0] == 0);
  fake.readError = 0;
  fake.active = false;
  CHECK(lovrOpenVRInputUpdate(&input, ETrackingUniverseOrigin_TrackingUniverseStanding, .02f) == 0);
  CHECK(!lovrOpenVRInputIsTouched(&input, DEVICE_HAND_RIGHT, BUTTON_TRIGGER, &touched) && !touched);
  fake.active = true;
  CHECK(lovrOpenVRInputUpdate(&input, ETrackingUniverseOrigin_TrackingUniverseStanding, .02f) == 0);
  fake.failRead = 2 + 2 * OPENVR_INPUT_BUTTON_COUNT + 1;
  CHECK(lovrOpenVRInputUpdate(&input, ETrackingUniverseOrigin_TrackingUniverseStanding, .02f) == EVRInputError_VRInputError_IPCError);
  CHECK(!lovrOpenVRInputGetPose(&input, DEVICE_HAND_RIGHT, p, q));
  CHECK(!lovrOpenVRInputGetAxis(&input, DEVICE_HAND_RIGHT, AXIS_THUMBSTICK, axis) && axis[0] == 0);
  fake.failRead = 0;
  CHECK(lovrOpenVRInputUpdate(&input, ETrackingUniverseOrigin_TrackingUniverseStanding, NAN) == EVRInputError_VRInputError_InvalidParam);
  fake.updateError = EVRInputError_VRInputError_NoActiveActionSet;
  CHECK(lovrOpenVRInputUpdate(&input, ETrackingUniverseOrigin_TrackingUniverseStanding, .02f) == fake.updateError);
  CHECK(fake.reads == 0 && !lovrOpenVRInputGetVelocity(&input, DEVICE_HAND_LEFT, v, w) && v[0] == 0 && w[0] == 0);
  CHECK(!lovrOpenVRInputIsDown(&input, DEVICE_HEAD, BUTTON_TRIGGER, &down, &changed) && !down && !changed);
  CHECK(!lovrOpenVRInputGetAxis(&input, DEVICE_HAND_LEFT, AXIS_NIB, axis) && axis[0] == 0);
  CHECK(!lovrOpenVRInputIsDown(&input, DEVICE_HAND_LEFT, BUTTON_NIB, &down, &changed));
  CHECK(fake.valid);
  return true;
}

static bool scalarAxes(void) {
  resetFake();
  OpenVRInput input;
  CHECK(lovrOpenVRInputInit(&input, &api, "/installed/actions.json") == 0);
  CHECK(lovrOpenVRInputUpdate(&input, ETrackingUniverseOrigin_TrackingUniverseStanding, .02f) == 0);
  DeviceAxis scalar[] = { AXIS_TRIGGER, AXIS_GRIP, AXIS_NIB, AXIS_THUMBREST, MAX_AXES, (DeviceAxis) -1 };
  for (unsigned i = 0; i < sizeof(scalar) / sizeof(scalar[0]); i++) {
    struct { float value; float canary; } out = { 99.f, 123.f };
    bool supported = i < 2;
    CHECK(lovrOpenVRInputGetAxis(&input, DEVICE_HAND_LEFT, scalar[i], &out.value) == supported);
    CHECK(out.canary == 123.f);
    CHECK(out.value == (supported ? .7f : 0.f));
    out.value = 99.f;
    CHECK(!lovrOpenVRInputGetAxis(&input, DEVICE_HEAD, scalar[i], &out.value));
    CHECK(out.value == 0.f && out.canary == 123.f);
  }
  float scalarValue;
  CHECK(lovrOpenVRInputGetAxis(&input, DEVICE_HAND_LEFT, AXIS_TRIGGER, &scalarValue));
  CHECK(scalarValue == .7f);
  return true;
}

static bool malformedTracking(void) {
  resetFake();
  OpenVRInput input;
  CHECK(lovrOpenVRInputInit(&input, &api, "/installed/actions.json") == 0);
  CHECK(lovrOpenVRInputUpdate(&input, ETrackingUniverseOrigin_TrackingUniverseStanding, .02f) == 0);
  InputPoseActionData_t valid = input.hands[0].poses[0];
  for (unsigned kind = 0; kind < 5; kind++) {
    InputPoseActionData_t* data = &input.hands[0].poses[0];
    *data = valid;
    if (kind == 0) memset(&data->pose.mDeviceToAbsoluteTracking, 0, sizeof(HmdMatrix34_t));
    if (kind == 1) data->pose.mDeviceToAbsoluteTracking.m[1][1] = -1.f;
    if (kind == 2) data->pose.mDeviceToAbsoluteTracking.m[0][3] = NAN;
    if (kind == 3) data->pose.mDeviceToAbsoluteTracking.m[0][2] = INFINITY;
    if (kind == 4) data->pose.mDeviceToAbsoluteTracking.m[0][2] = 2.f;
    float p[3] = { 99, 99, 99 }, q[4] = { 99, 99, 99, 99 };
    CHECK(!lovrOpenVRInputGetPose(&input, DEVICE_HAND_LEFT, p, q));
    CHECK(p[0] == 0 && p[1] == 0 && p[2] == 0);
    CHECK(q[0] == 0 && q[1] == 0 && q[2] == 0 && q[3] == 1);
  }
  for (unsigned component = 0; component < 6; component++) {
    InputPoseActionData_t* data = &input.hands[0].poses[0];
    *data = valid;
    if (component < 3) data->pose.vVelocity.v[component] = NAN;
    else data->pose.vAngularVelocity.v[component - 3] = INFINITY;
    float v[3] = { 99, 99, 99 }, w[3] = { 99, 99, 99 };
    CHECK(!lovrOpenVRInputGetVelocity(&input, DEVICE_HAND_LEFT, v, w));
    CHECK(v[0] == 0 && v[1] == 0 && v[2] == 0 && w[0] == 0 && w[1] == 0 && w[2] == 0);
  }
  return true;
}

static bool haptics(void) {
  resetFake();
  OpenVRInput input;
  CHECK(lovrOpenVRInputInit(&input, &api, "/installed/actions.json") == 0);
  CHECK(lovrOpenVRInputVibrate(&input, DEVICE_HAND_RIGHT, .4f, .25f, 120.f) == 0);
  CHECK(fake.valid && fake.haptic == 200 + OPENVR_INPUT_ACTION_COUNT - 1 && fake.restriction == 301);
  CHECK(fake.strength == .4f && fake.duration == .25f && fake.frequency == 120.f);
  fake.hapticError = EVRInputError_VRInputError_IPCError;
  CHECK(lovrOpenVRInputVibrate(&input, DEVICE_HAND_LEFT, 1.f, 1.f, 0.f) == fake.hapticError);
  CHECK(fake.restriction == 300);
  CHECK(lovrOpenVRInputVibrate(&input, DEVICE_HEAD, 1.f, 1.f, 1.f) == EVRInputError_VRInputError_InvalidDevice);
  CHECK(lovrOpenVRInputVibrate(&input, DEVICE_HAND_LEFT, NAN, 1.f, 1.f) == EVRInputError_VRInputError_InvalidParam);
  CHECK(lovrOpenVRInputVibrate(&input, DEVICE_HAND_LEFT, 1.1f, 1.f, 1.f) == EVRInputError_VRInputError_InvalidParam);
  CHECK(lovrOpenVRInputVibrate(&input, DEVICE_HAND_LEFT, 1.f, -1.f, 1.f) == EVRInputError_VRInputError_InvalidParam);
  CHECK(lovrOpenVRInputVibrate(&input, DEVICE_HAND_LEFT, 1.f, 1.f, INFINITY) == EVRInputError_VRInputError_InvalidParam);
  CHECK(fake.haptics == 2);
  return true;
}

int main(int argc, char** argv) {
  NativeTest tests[] = {
    { "openvr.input.initialization", initialization },
    { "openvr.input.action-snapshot-transitions", snapshots },
    { "openvr.input.scalar-axis-canary", scalarAxes },
    { "openvr.input.malformed-tracking", malformedTracking },
    { "openvr.input.haptic-action-routing", haptics }
  };
  return nativeRunTests(argc, argv, tests, sizeof(tests) / sizeof(tests[0]));
}
