#include "headset/openvr_overlay.h"
#include <math.h>
#include <string.h>

static OpenVRPanelResult result(OpenVRPanelStatus status, const char* operation, EVROverlayError error) {
  return (OpenVRPanelResult) { status, error, EVROverlayError_VROverlayError_None, operation };
}

static OpenVRPanelResult runtimeResult(const char* operation, EVROverlayError error) {
  return result(error == EVROverlayError_VROverlayError_None ? OPENVR_PANEL_OK : OPENVR_PANEL_RUNTIME_ERROR,
    operation, error);
}

static OpenVRPanelResult validate(const OpenVRPanelConfig* config, VRTextureBounds_t* bounds, float* aspect) {
  if (!config) return result(OPENVR_PANEL_INVALID, "configuration", 0);
  if (config->textureLayout != OPENVR_PANEL_TEXTURE_MONO_2D &&
      config->textureLayout != OPENVR_PANEL_TEXTURE_SIDE_BY_SIDE_2D) {
    return result(OPENVR_PANEL_UNSUPPORTED, "texture layout", 0);
  }
  if (config->filter != OPENVR_PANEL_FILTER_RUNTIME_DEFAULT) {
    return result(OPENVR_PANEL_UNSUPPORTED, "texture filtering", 0);
  }
  if (!isfinite(config->curve) || config->curve < 0.f) return result(OPENVR_PANEL_INVALID, "curve", 0);
  if (config->colorSpace != EColorSpace_ColorSpace_Linear && config->colorSpace != EColorSpace_ColorSpace_Gamma) {
    return result(OPENVR_PANEL_INVALID, "color space", 0);
  }
  if (!isfinite(config->width) || config->width <= 0.f || !isfinite(config->height) || config->height <= 0.f ||
      !config->textureWidth || !config->textureHeight) return result(OPENVR_PANEL_INVALID, "dimensions", 0);
  for (int i = 0; i < 4; i++) {
    if (!isfinite(config->color[i]) || config->color[i] < 0.f || config->color[i] > 1.f) {
      return result(OPENVR_PANEL_INVALID, "color", 0);
    }
  }
  if (config->transform == OPENVR_PANEL_ABSOLUTE) {
    if (config->origin != ETrackingUniverseOrigin_TrackingUniverseSeated &&
        config->origin != ETrackingUniverseOrigin_TrackingUniverseStanding &&
        config->origin != ETrackingUniverseOrigin_TrackingUniverseRawAndUncalibrated) {
      return result(OPENVR_PANEL_INVALID, "tracking origin", 0);
    }
  } else if (config->transform == OPENVR_PANEL_DEVICE_RELATIVE) {
    if (config->device >= k_unMaxTrackedDeviceCount) return result(OPENVR_PANEL_INVALID, "tracked device", 0);
  } else {
    return result(OPENVR_PANEL_UNSUPPORTED, "transform", 0);
  }
  for (int row = 0; row < 3; row++) {
    for (int column = 0; column < 4; column++) {
      if (!isfinite(config->pose.m[row][column])) return result(OPENVR_PANEL_INVALID, "pose", 0);
    }
  }
  bool stereo = config->textureLayout == OPENVR_PANEL_TEXTURE_SIDE_BY_SIDE_2D;
  if (stereo && config->textureWidth % 2) return result(OPENVR_PANEL_INVALID, "packed stereo dimensions", 0);
  uint32_t eyeWidth = stereo ? config->textureWidth / 2 : config->textureWidth;
  int64_t x = config->viewport[0], y = config->viewport[1];
  int64_t width = config->viewport[2], height = config->viewport[3];
  if (x < 0 || y < 0 || x >= eyeWidth || y >= config->textureHeight || width < 0 || height < 0) {
    return result(OPENVR_PANEL_INVALID, "viewport", 0);
  }
  if (!width) width = eyeWidth - x;
  if (!height) height = config->textureHeight - y;
  if (x + width > eyeWidth || y + height > config->textureHeight) {
    return result(OPENVR_PANEL_INVALID, "viewport", 0);
  }
  if (stereo && (x != 0 || width != eyeWidth)) {
    return result(OPENVR_PANEL_UNSUPPORTED, "packed stereo horizontal crop requires repacking", 0);
  }
  *bounds = (VRTextureBounds_t) {
    (float) ((double) x / eyeWidth), (float) ((double) y / config->textureHeight),
    (float) ((double) (x + width) / eyeWidth), (float) ((double) (y + height) / config->textureHeight)
  };
  *aspect = (float) (((double) config->width / config->height) * height / width);
  if (!isfinite(*aspect) || *aspect <= 0.f || bounds->uMin >= bounds->uMax || bounds->vMin >= bounds->vMax) {
    return result(OPENVR_PANEL_INVALID, "texel aspect", 0);
  }
  return result(OPENVR_PANEL_OK, NULL, 0);
}

OpenVRPanelResult lovrOpenVRPanelCreate(OpenVRPanel* panel, struct VR_IVROverlay_FnTable* api,
    const char* key, const char* name, const OpenVRPanelConfig* config) {
  if (!panel || panel->handle || panel->api || !api || !key || !*key || !name || !*name) {
    return result(OPENVR_PANEL_INVALID, "create", 0);
  }
  VRTextureBounds_t bounds;
  float aspect;
  OpenVRPanelResult status = validate(config, &bounds, &aspect);
  if (status.status != OPENVR_PANEL_OK) return status;
  if (!api->CreateOverlay || !api->DestroyOverlay || !api->HideOverlay || !api->ShowOverlay ||
      !api->SetOverlayFlag || !api->SetOverlayColor || !api->SetOverlayAlpha || !api->SetOverlayTexelAspect ||
      !api->SetOverlaySortOrder || !api->SetOverlayWidthInMeters || !api->SetOverlayCurvature ||
      !api->SetOverlayTextureColorSpace || !api->SetOverlayTextureBounds ||
      !api->SetOverlayTransformAbsolute || !api->SetOverlayTransformTrackedDeviceRelative) {
    return result(OPENVR_PANEL_UNSUPPORTED, "overlay interface", 0);
  }
  VROverlayHandle_t handle = 0;
  status = runtimeResult("CreateOverlay", api->CreateOverlay((char*) key, (char*) name, &handle));
  if (handle) *panel = (OpenVRPanel) { .api = api, .handle = handle };
  if (status.status != OPENVR_PANEL_OK) {
    if (handle) {
      OpenVRPanelResult cleanup = lovrOpenVRPanelDestroy(panel);
      status.cleanupError = cleanup.error != EVROverlayError_VROverlayError_None ? cleanup.error : cleanup.cleanupError;
    }
    return status;
  }
  if (!handle) return result(OPENVR_PANEL_RUNTIME_ERROR, "CreateOverlay", EVROverlayError_VROverlayError_InvalidHandle);
  status = lovrOpenVRPanelConfigure(panel, config);
  if (status.status != OPENVR_PANEL_OK) {
    OpenVRPanelResult cleanup = lovrOpenVRPanelDestroy(panel);
    status.cleanupError = cleanup.error != EVROverlayError_VROverlayError_None ? cleanup.error : cleanup.cleanupError;
  }
  return status;
}

OpenVRPanelResult lovrOpenVRPanelConfigure(OpenVRPanel* panel, const OpenVRPanelConfig* config) {
  if (!panel || !panel->api || !panel->handle) return result(OPENVR_PANEL_INVALID, "configure", 0);
  VRTextureBounds_t bounds;
  float aspect;
  OpenVRPanelResult status = validate(config, &bounds, &aspect);
  if (status.status != OPENVR_PANEL_OK) return status;
  panel->configured = false;
  struct VR_IVROverlay_FnTable* api = panel->api;
  VROverlayHandle_t handle = panel->handle;
#define APPLY(call) do { status = runtimeResult(#call, api->call); if (status.status != OPENVR_PANEL_OK) return status; } while (0)
  APPLY(HideOverlay(handle));
  APPLY(SetOverlayFlag(handle, VROverlayFlags_NoBackside, true));
  APPLY(SetOverlayFlag(handle, VROverlayFlags_IsPremultiplied, config->premultiplied));
  APPLY(SetOverlayFlag(handle, VROverlayFlags_IgnoreTextureAlpha, config->ignoreTextureAlpha));
  APPLY(SetOverlayFlag(handle, VROverlayFlags_SideBySide_Parallel,
    config->textureLayout == OPENVR_PANEL_TEXTURE_SIDE_BY_SIDE_2D));
  APPLY(SetOverlayTextureColorSpace(handle, config->colorSpace));
  APPLY(SetOverlayColor(handle, config->color[0], config->color[1], config->color[2]));
  APPLY(SetOverlayAlpha(handle, config->color[3]));
  APPLY(SetOverlaySortOrder(handle, config->order));
  APPLY(SetOverlayWidthInMeters(handle, config->width));
  APPLY(SetOverlayTextureBounds(handle, &bounds));
  APPLY(SetOverlayTexelAspect(handle, aspect));
  float curvature = config->curve < 1e-3f ? 0.f :
    (float) fmin(1., (double) config->width * config->curve / 6.28318530717958647692);
  APPLY(SetOverlayCurvature(handle, curvature));
  HmdMatrix34_t pose = config->pose;
  if (config->transform == OPENVR_PANEL_ABSOLUTE) {
    APPLY(SetOverlayTransformAbsolute(handle, config->origin, &pose));
  } else {
    APPLY(SetOverlayTransformTrackedDeviceRelative(handle, config->device, &pose));
  }
#undef APPLY
  panel->configured = true;
  return result(OPENVR_PANEL_OK, NULL, 0);
}

OpenVRPanelResult lovrOpenVRPanelShow(OpenVRPanel* panel) {
  if (!panel || !panel->api || !panel->handle || !panel->configured) return result(OPENVR_PANEL_INVALID, "show", 0);
  return runtimeResult("ShowOverlay", panel->api->ShowOverlay(panel->handle));
}

OpenVRPanelResult lovrOpenVRPanelHide(OpenVRPanel* panel) {
  if (!panel) return result(OPENVR_PANEL_INVALID, "hide", 0);
  if (!panel->handle) return result(OPENVR_PANEL_OK, NULL, 0);
  return runtimeResult("HideOverlay", panel->api->HideOverlay(panel->handle));
}

OpenVRPanelResult lovrOpenVRPanelDestroy(OpenVRPanel* panel) {
  if (!panel) return result(OPENVR_PANEL_INVALID, "destroy", 0);
  if (!panel->handle) return result(OPENVR_PANEL_OK, NULL, 0);
  panel->configured = false;
  OpenVRPanelResult status = lovrOpenVRPanelHide(panel);
  EVROverlayError error = panel->api->DestroyOverlay(panel->handle);
  if (error == EVROverlayError_VROverlayError_None) memset(panel, 0, sizeof(*panel));
  if (status.status != OPENVR_PANEL_OK) {
    status.cleanupError = error;
    return status;
  }
  return runtimeResult("DestroyOverlay", error);
}
