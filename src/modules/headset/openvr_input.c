#include "openvr_input.h"
#include "core/maf.h"
#include <math.h>
#include <string.h>

static const char* paths[] = {
  "/actions/lovr/in/grip_pose", "/actions/lovr/in/aim_pose",
  "/actions/lovr/in/trigger", "/actions/lovr/in/thumbstick", "/actions/lovr/in/touchpad",
  "/actions/lovr/in/grip", "/actions/lovr/in/menu", "/actions/lovr/in/a", "/actions/lovr/in/b",
  "/actions/lovr/in/x", "/actions/lovr/in/y", "/actions/lovr/in/thumbrest", "/actions/lovr/in/thumbtap",
  "/actions/lovr/in/trigger_touch", "/actions/lovr/in/thumbstick_touch", "/actions/lovr/in/touchpad_touch",
  "/actions/lovr/in/grip_touch", "/actions/lovr/in/menu_touch", "/actions/lovr/in/a_touch",
  "/actions/lovr/in/b_touch", "/actions/lovr/in/x_touch", "/actions/lovr/in/y_touch",
  "/actions/lovr/in/thumbrest_touch", "/actions/lovr/in/thumbtap_touch",
  "/actions/lovr/in/trigger_axis", "/actions/lovr/in/thumbstick_axis",
  "/actions/lovr/in/touchpad_axis", "/actions/lovr/in/grip_axis", "/actions/lovr/out/haptic"
};

static const DeviceButton buttons[] = {
  BUTTON_TRIGGER, BUTTON_THUMBSTICK, BUTTON_TOUCHPAD, BUTTON_GRIP, BUTTON_MENU,
  BUTTON_A, BUTTON_B, BUTTON_X, BUTTON_Y, BUTTON_THUMBREST, BUTTON_THUMBTAP
};

static const DeviceAxis axes[] = { AXIS_TRIGGER, AXIS_THUMBSTICK, AXIS_TOUCHPAD, AXIS_GRIP };

_Static_assert(sizeof(paths) / sizeof(paths[0]) == OPENVR_INPUT_ACTION_COUNT, "action count");
_Static_assert(sizeof(buttons) / sizeof(buttons[0]) == OPENVR_INPUT_BUTTON_COUNT, "button count");
_Static_assert(sizeof(axes) / sizeof(axes[0]) == OPENVR_INPUT_AXIS_COUNT, "axis count");

static int getHand(Device device) {
  switch (device) {
    case DEVICE_HAND_LEFT:
    case DEVICE_HAND_LEFT_GRIP:
    case DEVICE_HAND_LEFT_POINT: return 0;
    case DEVICE_HAND_RIGHT:
    case DEVICE_HAND_RIGHT_GRIP:
    case DEVICE_HAND_RIGHT_POINT: return 1;
    default: return -1;
  }
}

static const InputPoseActionData_t* getPose(const OpenVRInput* input, Device device) {
  int hand = getHand(device);
  if (!input || !input->api || hand < 0) return NULL;
  unsigned aim = device == DEVICE_HAND_LEFT_POINT || device == DEVICE_HAND_RIGHT_POINT;
  const InputPoseActionData_t* pose = &input->hands[hand].poses[aim];
  return pose->bActive && pose->pose.bDeviceIsConnected && pose->pose.bPoseIsValid ? pose : NULL;
}

static int getButton(DeviceButton button) {
  for (unsigned i = 0; i < OPENVR_INPUT_BUTTON_COUNT; i++) {
    if (buttons[i] == button) return i;
  }
  return -1;
}

const char* lovrOpenVRInputActionPath(uint32_t index) {
  return index < OPENVR_INPUT_ACTION_COUNT ? paths[index] : NULL;
}

EVRInputError lovrOpenVRInputInit(OpenVRInput* input, struct VR_IVRInput_FnTable* api, const char* manifestPath) {
  if (!input) return EVRInputError_VRInputError_InvalidParam;
  lovrOpenVRInputReset(input);
  if (!api || !manifestPath || manifestPath[0] != '/' || !manifestPath[1] ||
      !api->SetActionManifestPath || !api->GetActionSetHandle || !api->GetActionHandle ||
      !api->GetInputSourceHandle || !api->UpdateActionState || !api->GetDigitalActionData ||
      !api->GetAnalogActionData || !api->GetPoseActionDataRelativeToNow || !api->TriggerHapticVibrationAction) {
    return EVRInputError_VRInputError_InvalidParam;
  }
  OpenVRInput next = { .api = api };
  EVRInputError error = api->SetActionManifestPath((char*) manifestPath);
  if (error) return error;
  error = api->GetActionSetHandle("/actions/lovr", &next.actionSet);
  if (error) return error;
  if (!next.actionSet) return EVRInputError_VRInputError_InvalidHandle;
  for (unsigned i = 0; i < OPENVR_INPUT_ACTION_COUNT; i++) {
    error = api->GetActionHandle((char*) paths[i], &next.actions[i]);
    if (error) return error;
    if (!next.actions[i]) return EVRInputError_VRInputError_InvalidHandle;
  }
  char* sources[] = { "/user/hand/left", "/user/hand/right" };
  for (unsigned i = 0; i < 2; i++) {
    error = api->GetInputSourceHandle(sources[i], &next.sources[i]);
    if (error) return error;
    if (!next.sources[i]) return EVRInputError_VRInputError_InvalidHandle;
  }
  *input = next;
  return EVRInputError_VRInputError_None;
}

void lovrOpenVRInputReset(OpenVRInput* input) {
  if (input) memset(input, 0, sizeof(*input));
}

void lovrOpenVRInputClear(OpenVRInput* input) {
  if (input) memset(input->hands, 0, sizeof(input->hands));
}

EVRInputError lovrOpenVRInputUpdate(OpenVRInput* input, ETrackingUniverseOrigin origin, float prediction) {
  lovrOpenVRInputClear(input);
  if (!input || !input->api) return EVRInputError_VRInputError_InvalidHandle;
  if (!isfinite(prediction) || (origin != ETrackingUniverseOrigin_TrackingUniverseSeated &&
      origin != ETrackingUniverseOrigin_TrackingUniverseStanding && origin != ETrackingUniverseOrigin_TrackingUniverseRawAndUncalibrated)) {
    return EVRInputError_VRInputError_InvalidParam;
  }
  VRActiveActionSet_t set = { .ulActionSet = input->actionSet };
  EVRInputError error = input->api->UpdateActionState(&set, sizeof(set), 1);
  if (error) return error;
  OpenVRInputHand next[2] = { 0 };
  EVRInputError firstError = EVRInputError_VRInputError_None;
  for (unsigned h = 0; h < 2; h++) {
    unsigned action = 0;
    OpenVRInputHand* hand = &next[h];
    for (unsigned i = 0; i < 2; i++, action++) {
      InputPoseActionData_t* pose = &hand->poses[i];
      error = input->api->GetPoseActionDataRelativeToNow(input->actions[action], origin, prediction,
        pose, sizeof(*pose), input->sources[h]);
      if (error && !firstError) firstError = error;
      if (error || !pose->bActive || !pose->pose.bDeviceIsConnected) memset(pose, 0, sizeof(*pose));
    }
    for (unsigned i = 0; i < 2 * OPENVR_INPUT_BUTTON_COUNT; i++, action++) {
      InputDigitalActionData_t* data = i < OPENVR_INPUT_BUTTON_COUNT ? &hand->buttons[i] : &hand->touches[i - OPENVR_INPUT_BUTTON_COUNT];
      error = input->api->GetDigitalActionData(input->actions[action], data, sizeof(*data), input->sources[h]);
      if (error && !firstError) firstError = error;
      if (error || !data->bActive) memset(data, 0, sizeof(*data));
    }
    for (unsigned i = 0; i < OPENVR_INPUT_AXIS_COUNT; i++, action++) {
      InputAnalogActionData_t* data = &hand->axes[i];
      error = input->api->GetAnalogActionData(input->actions[action], data, sizeof(*data), input->sources[h]);
      if (error && !firstError) firstError = error;
      if (error || !data->bActive) memset(data, 0, sizeof(*data));
    }
    if (!hand->poses[0].bActive && !hand->poses[1].bActive) memset(hand, 0, sizeof(*hand));
  }
  if (firstError) return firstError;
  memcpy(input->hands, next, sizeof(next));
  return EVRInputError_VRInputError_None;
}

bool lovrOpenVRInputGetPose(const OpenVRInput* input, Device device, float* position, float* orientation) {
  memset(position, 0, 3 * sizeof(float));
  quat_identity(orientation);
  const InputPoseActionData_t* pose = getPose(input, device);
  if (!pose) return false;
  const HmdMatrix34_t* m = &pose->pose.mDeviceToAbsoluteTracking;
  for (unsigned row = 0; row < 3; row++) {
    for (unsigned column = 0; column < 4; column++) {
      if (!isfinite(m->m[row][column])) return false;
    }
  }
  for (unsigned a = 0; a < 3; a++) {
    for (unsigned b = a; b < 3; b++) {
      float dot = 0.f;
      for (unsigned row = 0; row < 3; row++) dot += m->m[row][a] * m->m[row][b];
      if (!isfinite(dot) || fabsf(dot - (a == b ? 1.f : 0.f)) > 1e-3f) return false;
    }
  }
  float determinant =
    m->m[0][0] * (m->m[1][1] * m->m[2][2] - m->m[1][2] * m->m[2][1]) -
    m->m[0][1] * (m->m[1][0] * m->m[2][2] - m->m[1][2] * m->m[2][0]) +
    m->m[0][2] * (m->m[1][0] * m->m[2][1] - m->m[1][1] * m->m[2][0]);
  if (!isfinite(determinant) || fabsf(determinant - 1.f) > 1e-3f) return false;
  float matrix[16] = {
    m->m[0][0], m->m[1][0], m->m[2][0], 0.f,
    m->m[0][1], m->m[1][1], m->m[2][1], 0.f,
    m->m[0][2], m->m[1][2], m->m[2][2], 0.f,
    m->m[0][3], m->m[1][3], m->m[2][3], 1.f
  };
  memcpy(position, matrix + 12, 3 * sizeof(float));
  quat_fromMat4(orientation, matrix);
  return true;
}

bool lovrOpenVRInputGetVelocity(const OpenVRInput* input, Device device, float* velocity, float* angularVelocity) {
  memset(velocity, 0, 3 * sizeof(float));
  memset(angularVelocity, 0, 3 * sizeof(float));
  const InputPoseActionData_t* pose = getPose(input, device);
  if (!pose) return false;
  for (unsigned i = 0; i < 3; i++) {
    if (!isfinite(pose->pose.vVelocity.v[i]) || !isfinite(pose->pose.vAngularVelocity.v[i])) return false;
  }
  memcpy(velocity, pose->pose.vVelocity.v, 3 * sizeof(float));
  memcpy(angularVelocity, pose->pose.vAngularVelocity.v, 3 * sizeof(float));
  return true;
}

bool lovrOpenVRInputIsDown(const OpenVRInput* input, Device device, DeviceButton button, bool* down, bool* changed) {
  *down = *changed = false;
  int hand = getHand(device), index = getButton(button);
  if (!input || !input->api || hand < 0 || index < 0) return false;
  const InputDigitalActionData_t* data = &input->hands[hand].buttons[index];
  if (!data->bActive) return false;
  *down = data->bState;
  *changed = data->bChanged;
  return true;
}

bool lovrOpenVRInputIsTouched(const OpenVRInput* input, Device device, DeviceButton button, bool* touched) {
  *touched = false;
  int hand = getHand(device), index = getButton(button);
  if (!input || !input->api || hand < 0 || index < 0) return false;
  const InputDigitalActionData_t* data = &input->hands[hand].touches[index];
  if (!data->bActive) return false;
  *touched = data->bState;
  return true;
}

bool lovrOpenVRInputGetAxis(const OpenVRInput* input, Device device, DeviceAxis axis, float* value) {
  value[0] = 0.f;
  if (axis == AXIS_THUMBSTICK || axis == AXIS_TOUCHPAD) value[1] = 0.f;
  int hand = getHand(device);
  if (!input || !input->api || hand < 0) return false;
  for (unsigned i = 0; i < OPENVR_INPUT_AXIS_COUNT; i++) {
    if (axes[i] != axis) continue;
    const InputAnalogActionData_t* data = &input->hands[hand].axes[i];
    if (!data->bActive) return false;
    value[0] = data->x;
    if (axis == AXIS_THUMBSTICK || axis == AXIS_TOUCHPAD) value[1] = data->y;
    return true;
  }
  return false;
}

EVRInputError lovrOpenVRInputVibrate(OpenVRInput* input, Device device, float strength, float duration, float frequency) {
  if (!input || !input->api) return EVRInputError_VRInputError_InvalidHandle;
  int hand = getHand(device);
  if (hand < 0) return EVRInputError_VRInputError_InvalidDevice;
  if (!isfinite(strength) || strength < 0.f || strength > 1.f || !isfinite(duration) || duration < 0.f ||
      !isfinite(frequency) || frequency < 0.f) return EVRInputError_VRInputError_InvalidParam;
  return input->api->TriggerHapticVibrationAction(input->actions[OPENVR_INPUT_ACTION_COUNT - 1],
    0.f, duration, frequency, strength, input->sources[hand]);
}
