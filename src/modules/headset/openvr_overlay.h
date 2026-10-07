#ifndef LOVR_OPENVR_OVERLAY_H
#define LOVR_OPENVR_OVERLAY_H

#include <openvr_capi.h>

typedef enum {
  OPENVR_PANEL_OK,
  OPENVR_PANEL_INVALID,
  OPENVR_PANEL_UNSUPPORTED,
  OPENVR_PANEL_RUNTIME_ERROR
} OpenVRPanelStatus;

typedef struct {
  OpenVRPanelStatus status;
  EVROverlayError error;
  EVROverlayError cleanupError;
  const char* operation;
} OpenVRPanelResult;

typedef enum {
  OPENVR_PANEL_FILTER_RUNTIME_DEFAULT,
  OPENVR_PANEL_FILTER_NEAREST,
  OPENVR_PANEL_FILTER_LINEAR
} OpenVRPanelFilter;

typedef enum {
  OPENVR_PANEL_ABSOLUTE,
  OPENVR_PANEL_DEVICE_RELATIVE
} OpenVRPanelTransform;

typedef enum {
  OPENVR_PANEL_TEXTURE_MONO_2D,
  OPENVR_PANEL_TEXTURE_SIDE_BY_SIDE_2D,
  OPENVR_PANEL_TEXTURE_ARRAY
} OpenVRPanelTextureLayout;

typedef struct {
  uint32_t textureWidth;
  uint32_t textureHeight;
  int32_t viewport[4];
  float width;
  float height;
  float curve;
  float color[4];
  EColorSpace colorSpace;
  bool premultiplied;
  bool ignoreTextureAlpha;
  OpenVRPanelTextureLayout textureLayout;
  OpenVRPanelFilter filter;
  uint32_t order;
  OpenVRPanelTransform transform;
  ETrackingUniverseOrigin origin;
  TrackedDeviceIndex_t device;
  HmdMatrix34_t pose;
} OpenVRPanelConfig;

typedef struct {
  struct VR_IVROverlay_FnTable* api;
  VROverlayHandle_t handle;
  bool configured;
} OpenVRPanel;

OpenVRPanelResult lovrOpenVRPanelCreate(OpenVRPanel* panel, struct VR_IVROverlay_FnTable* api,
  const char* key, const char* name, const OpenVRPanelConfig* config);
OpenVRPanelResult lovrOpenVRPanelConfigure(OpenVRPanel* panel, const OpenVRPanelConfig* config);
OpenVRPanelResult lovrOpenVRPanelShow(OpenVRPanel* panel);
OpenVRPanelResult lovrOpenVRPanelHide(OpenVRPanel* panel);
OpenVRPanelResult lovrOpenVRPanelDestroy(OpenVRPanel* panel);

#endif
