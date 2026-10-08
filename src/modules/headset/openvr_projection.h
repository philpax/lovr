#ifndef LOVR_OPENVR_PROJECTION_H
#define LOVR_OPENVR_PROJECTION_H

#include "core/gpu.h"
#include <openvr_capi.h>

typedef enum {
  OPENVR_PROJECTION_OK,
  OPENVR_PROJECTION_INVALID,
  OPENVR_PROJECTION_UNSUPPORTED,
  OPENVR_PROJECTION_RUNTIME_ERROR
} OpenVRProjectionStatus;

typedef struct {
  OpenVRProjectionStatus status;
  EVROverlayError error;
  EVROverlayError cleanupError;
  const char* operation;
  unsigned int eye;
} OpenVRProjectionResult;

// Each eye's arguments to SetOverlayTransformProjection, in the overlay API's own conventions: views
// map the tracking origin to the eye, and frusta hold the downward edge's tangent in fTop.
typedef struct {
  ETrackingUniverseOrigin origin;
  HmdMatrix34_t views[2];
  VROverlayProjection_t frusta[2];
  EColorSpace colorSpace;
  uint32_t order;
} OpenVRProjectionConfig;

typedef struct {
  struct VR_IVROverlay_FnTable* api;
  VROverlayHandle_t eyes[2];
  bool visible[2];
  bool submitted[2];
  bool configured;
  EColorSpace colorSpace;
} OpenVRProjection;

typedef struct {
  OpenVRProjection* projection;
  unsigned int eye;
  OpenVRProjectionResult result;
} OpenVRProjectionHandoff;

OpenVRProjectionResult lovrOpenVRProjectionCreate(OpenVRProjection* projection,
  struct VR_IVROverlay_FnTable* api, const char* keys[2], const char* names[2], const OpenVRProjectionConfig* config);
OpenVRProjectionResult lovrOpenVRProjectionConfigure(OpenVRProjection* projection, const OpenVRProjectionConfig* config);
OpenVRProjectionResult lovrOpenVRProjectionShow(OpenVRProjection* projection);
OpenVRProjectionResult lovrOpenVRProjectionHide(OpenVRProjection* projection);
OpenVRProjectionResult lovrOpenVRProjectionDestroy(OpenVRProjection* projection);
bool lovrOpenVRProjectionHandoff(const gpu_external_image* image, void* data);
bool lovrOpenVRProjectionPanelOrder(uint32_t mainOrder, uint32_t panelIndex, uint32_t* order);
bool lovrOpenVRProjectionEye(const HmdMatrix34_t* pose, const float tangents[4], HmdMatrix34_t* view,
  VROverlayProjection_t* frustum);

#endif
