#ifndef LOVR_OPENVR_INPUT_H
#define LOVR_OPENVR_INPUT_H

#include "headset.h"
#include <openvr_capi.h>

#define OPENVR_INPUT_BUTTON_COUNT 11
#define OPENVR_INPUT_AXIS_COUNT 4
#define OPENVR_INPUT_ACTION_COUNT (3 + 2 * OPENVR_INPUT_BUTTON_COUNT + OPENVR_INPUT_AXIS_COUNT)

typedef struct OpenVRInputHand {
  InputPoseActionData_t poses[2];
  InputDigitalActionData_t buttons[OPENVR_INPUT_BUTTON_COUNT];
  InputDigitalActionData_t touches[OPENVR_INPUT_BUTTON_COUNT];
  InputAnalogActionData_t axes[OPENVR_INPUT_AXIS_COUNT];
} OpenVRInputHand;

typedef struct OpenVRInput {
  struct VR_IVRInput_FnTable* api;
  VRActionSetHandle_t actionSet;
  VRActionHandle_t actions[OPENVR_INPUT_ACTION_COUNT];
  VRInputValueHandle_t sources[2];
  OpenVRInputHand hands[2];
} OpenVRInput;

const char* lovrOpenVRInputActionPath(uint32_t index);
EVRInputError lovrOpenVRInputInit(OpenVRInput* input, struct VR_IVRInput_FnTable* api, const char* manifestPath);
void lovrOpenVRInputReset(OpenVRInput* input);
void lovrOpenVRInputClear(OpenVRInput* input);
EVRInputError lovrOpenVRInputUpdate(OpenVRInput* input, ETrackingUniverseOrigin origin, float prediction);
bool lovrOpenVRInputGetPose(const OpenVRInput* input, Device device, float* position, float* orientation);
bool lovrOpenVRInputGetVelocity(const OpenVRInput* input, Device device, float* velocity, float* angularVelocity);
bool lovrOpenVRInputIsDown(const OpenVRInput* input, Device device, DeviceButton button, bool* down, bool* changed);
bool lovrOpenVRInputIsTouched(const OpenVRInput* input, Device device, DeviceButton button, bool* touched);
bool lovrOpenVRInputGetAxis(const OpenVRInput* input, Device device, DeviceAxis axis, float* value);
EVRInputError lovrOpenVRInputVibrate(OpenVRInput* input, Device device, float strength, float duration, float frequency);

#endif
