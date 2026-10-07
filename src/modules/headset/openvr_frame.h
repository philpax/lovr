#ifndef LOVR_OPENVR_FRAME_H
#define LOVR_OPENVR_FRAME_H

#include <openvr_capi.h>

typedef enum {
  OPENVR_FRAME_OK,
  OPENVR_FRAME_TIMEOUT,
  OPENVR_FRAME_INVALID,
  OPENVR_FRAME_RUNTIME_ERROR
} OpenVRFrameStatus;

typedef struct {
  OpenVRFrameStatus status;
  EVROverlayError error;
} OpenVRFrameResult;

typedef struct {
  bool valid;
  float matrix[16];
  float position[3];
  float orientation[4];
} OpenVRFramePose;

typedef struct {
  OpenVRFramePose pose;
  float tangents[4];
  HmdMatrix34_t transform;
} OpenVRFrameEye;

typedef struct {
  ETrackingUniverseOrigin origin;
  uint64_t serial;
  uint32_t width;
  uint32_t height;
  float frequency;
  float prediction;
  double time;
  double displayTime;
  double displayPeriod;
  double delta;
  OpenVRFramePose head;
  bool velocityValid;
  float velocity[3];
  float angularVelocity[3];
  OpenVRFrameEye eyes[2];
} OpenVRFrameSnapshot;

typedef struct {
  struct VR_IVRSystem_FnTable* system;
  struct VR_IVROverlay_FnTable* overlay;
  OpenVRFrameSnapshot snapshot;
  uint64_t serial;
  double lastNow;
  double epoch;
  double lastDisplayTime;
  double lastDisplayPeriod;
  bool updated;
  bool hasEpoch;
} OpenVRFrame;

void lovrOpenVRFrameInit(OpenVRFrame* frame, struct VR_IVRSystem_FnTable* system,
  struct VR_IVROverlay_FnTable* overlay);
typedef double (*OpenVRFrameClock)(void* context);
OpenVRFrameResult lovrOpenVRFrameUpdate(OpenVRFrame* frame, ETrackingUniverseOrigin origin,
  OpenVRFrameClock clock, void* context);
bool lovrOpenVRFrameProjection(const float tangents[4], float near, float far, float matrix[16]);

#endif
