#include "headset/openvr_projection.h"
#include <math.h>
#include <string.h>

typedef enum {
  OPENVR_FORMAT_RGBA8_UNORM = 37,
  OPENVR_FORMAT_RGBA8_SRGB = 43,
  OPENVR_FORMAT_BGRA8_UNORM = 44,
  OPENVR_FORMAT_BGRA8_SRGB = 50,
  OPENVR_FORMAT_RGBA16_FLOAT = 97
} OpenVRColorFormat;

static OpenVRProjectionResult result(OpenVRProjectionStatus status, const char* operation,
    unsigned int eye, EVROverlayError error) {
  return (OpenVRProjectionResult) { .status = status, .operation = operation, .eye = eye, .error = error };
}

static OpenVRProjectionResult runtimeResult(const char* operation, unsigned int eye, EVROverlayError error) {
  return result(error ? OPENVR_PROJECTION_RUNTIME_ERROR : OPENVR_PROJECTION_OK, operation, eye, error);
}

bool lovrOpenVRProjectionPanelOrder(uint32_t mainOrder, uint32_t panelIndex, uint32_t* order) {
  if (!order || mainOrder == UINT32_MAX || panelIndex > UINT32_MAX - mainOrder - 1u) return false;
  *order = mainOrder + 1u + panelIndex;
  return true;
}

static bool validConfig(const OpenVRProjectionConfig* config) {
  if (!config || config->order == UINT32_MAX ||
      (config->colorSpace != EColorSpace_ColorSpace_Gamma && config->colorSpace != EColorSpace_ColorSpace_Linear) ||
      (config->origin != ETrackingUniverseOrigin_TrackingUniverseSeated &&
       config->origin != ETrackingUniverseOrigin_TrackingUniverseStanding &&
       config->origin != ETrackingUniverseOrigin_TrackingUniverseRawAndUncalibrated)) return false;
  for (unsigned int eye = 0; eye < 2; eye++) {
    const VROverlayProjection_t* f = &config->frusta[eye];
    if (!isfinite(f->fLeft) || !isfinite(f->fRight) || !isfinite(f->fTop) || !isfinite(f->fBottom) ||
        f->fLeft >= f->fRight || f->fTop >= f->fBottom) return false;
    for (unsigned int row = 0; row < 3; row++) {
      for (unsigned int column = 0; column < 4; column++) {
        if (!isfinite(config->poses[eye].m[row][column])) return false;
      }
    }
  }
  return true;
}

OpenVRProjectionResult lovrOpenVRProjectionCreate(OpenVRProjection* p,
    struct VR_IVROverlay_FnTable* api, const char* keys[2], const char* names[2], const OpenVRProjectionConfig* config) {
  if (!p || p->api || p->eyes[0] || p->eyes[1] || !api || !validConfig(config) || !keys || !names ||
      !keys[0] || !keys[1] || !*keys[0] || !*keys[1] || !strcmp(keys[0], keys[1]) ||
      !names[0] || !names[1] || !*names[0] || !*names[1]) return result(OPENVR_PROJECTION_INVALID, "create", 0, 0);
  if (!api->CreateOverlay || !api->DestroyOverlay || !api->HideOverlay || !api->ShowOverlay ||
      !api->SetOverlayFlag || !api->SetOverlaySortOrder || !api->SetOverlayTextureColorSpace ||
      !api->SetOverlayTransformProjection || !api->SetOverlayTexture) {
    return result(OPENVR_PROJECTION_UNSUPPORTED, "overlay interface", 0, 0);
  }
  p->api = api;
  OpenVRProjectionResult status = result(OPENVR_PROJECTION_OK, NULL, 0, 0);
  for (unsigned int eye = 0; eye < 2; eye++) {
    status = runtimeResult("CreateOverlay", eye, api->CreateOverlay((char*) keys[eye], (char*) names[eye], &p->eyes[eye]));
    if (status.status != OPENVR_PROJECTION_OK) break;
    if (!p->eyes[eye]) {
      status = runtimeResult("CreateOverlay", eye, EVROverlayError_VROverlayError_InvalidHandle);
      break;
    }
  }
  if (status.status == OPENVR_PROJECTION_OK) status = lovrOpenVRProjectionConfigure(p, config);
  if (status.status != OPENVR_PROJECTION_OK) {
    OpenVRProjectionResult cleanup = lovrOpenVRProjectionDestroy(p);
    status.cleanupError = cleanup.error;
  }
  return status;
}

OpenVRProjectionResult lovrOpenVRProjectionConfigure(OpenVRProjection* p, const OpenVRProjectionConfig* config) {
  if (!p || !p->api || !p->eyes[0] || !p->eyes[1] || !validConfig(config)) {
    return result(OPENVR_PROJECTION_INVALID, "configure", 0, 0);
  }
  p->configured = false;
  OpenVRProjectionResult status = lovrOpenVRProjectionHide(p);
  if (status.status != OPENVR_PROJECTION_OK) return status;
  p->submitted[0] = p->submitted[1] = false;
  for (unsigned int eye = 0; eye < 2; eye++) {
    VROverlayHandle_t handle = p->eyes[eye];
#define APPLY(call) do { status = runtimeResult(#call, eye, p->api->call); if (status.status != OPENVR_PROJECTION_OK) return status; } while (0)
    APPLY(SetOverlayFlag(handle, VROverlayFlags_NoBackside, true));
    APPLY(SetOverlayFlag(handle, VROverlayFlags_IsPremultiplied, true));
    APPLY(SetOverlayFlag(handle, VROverlayFlags_IgnoreTextureAlpha, false));
    APPLY(SetOverlaySortOrder(handle, config->order));
    APPLY(SetOverlayTextureColorSpace(handle, config->colorSpace));
    HmdMatrix34_t pose = config->poses[eye];
    VROverlayProjection_t frustum = config->frusta[eye];
    APPLY(SetOverlayTransformProjection(handle, config->origin, &pose, &frustum,
      eye ? EVREye_Eye_Right : EVREye_Eye_Left));
#undef APPLY
  }
  p->colorSpace = config->colorSpace;
  p->configured = true;
  return result(OPENVR_PROJECTION_OK, NULL, 0, 0);
}

OpenVRProjectionResult lovrOpenVRProjectionShow(OpenVRProjection* p) {
  if (!p || !p->api || !p->configured || !p->eyes[0] || !p->eyes[1] || !p->submitted[0] || !p->submitted[1]) {
    return result(OPENVR_PROJECTION_INVALID, "show", 0, 0);
  }
  OpenVRProjectionResult status = result(OPENVR_PROJECTION_OK, NULL, 0, 0);
  for (unsigned int eye = 0; eye < 2; eye++) {
    OpenVRProjectionResult next = runtimeResult("ShowOverlay", eye, p->api->ShowOverlay(p->eyes[eye]));
    if (next.status == OPENVR_PROJECTION_OK) p->visible[eye] = true;
    else if (status.status == OPENVR_PROJECTION_OK) status = next;
    else status.cleanupError = next.error;
  }
  return status;
}

OpenVRProjectionResult lovrOpenVRProjectionHide(OpenVRProjection* p) {
  if (!p || ((p->eyes[0] || p->eyes[1]) && !p->api)) return result(OPENVR_PROJECTION_INVALID, "hide", 0, 0);
  OpenVRProjectionResult status = result(OPENVR_PROJECTION_OK, NULL, 0, 0);
  for (unsigned int eye = 0; eye < 2; eye++) {
    if (!p->eyes[eye]) continue;
    OpenVRProjectionResult next = runtimeResult("HideOverlay", eye, p->api->HideOverlay(p->eyes[eye]));
    if (next.status == OPENVR_PROJECTION_OK) p->visible[eye] = false;
    else if (status.status == OPENVR_PROJECTION_OK) status = next;
    else status.cleanupError = next.error;
  }
  return status;
}

OpenVRProjectionResult lovrOpenVRProjectionDestroy(OpenVRProjection* p) {
  if (!p || ((p->eyes[0] || p->eyes[1]) && !p->api)) return result(OPENVR_PROJECTION_INVALID, "destroy", 0, 0);
  p->configured = false;
  OpenVRProjectionResult status = result(OPENVR_PROJECTION_OK, NULL, 0, 0);
  for (unsigned int eye = 0; eye < 2; eye++) {
    if (!p->eyes[eye]) continue;
    OpenVRProjectionResult next = runtimeResult("HideOverlay", eye, p->api->HideOverlay(p->eyes[eye]));
    if (next.status == OPENVR_PROJECTION_OK) {
      p->visible[eye] = false;
      next = runtimeResult("DestroyOverlay", eye, p->api->DestroyOverlay(p->eyes[eye]));
      if (next.status == OPENVR_PROJECTION_OK) {
        p->eyes[eye] = 0;
        p->submitted[eye] = false;
      }
    }
    if (status.status == OPENVR_PROJECTION_OK && next.status != OPENVR_PROJECTION_OK) status = next;
    else if (next.status != OPENVR_PROJECTION_OK) status.cleanupError = next.error;
  }
  if (!p->eyes[0] && !p->eyes[1]) memset(p, 0, sizeof(*p));
  return status;
}

bool lovrOpenVRProjectionHandoff(const gpu_external_image* image, void* data) {
  OpenVRProjectionHandoff* h = data;
  if (!h) return false;
  OpenVRProjection* p = h->projection;
  h->result = result(OPENVR_PROJECTION_INVALID, "texture handoff", h->eye, 0);
  if (!p || h->eye > 1) return false;
  p->submitted[h->eye] = false;
  if (!p->api || !p->configured || !p->eyes[h->eye] || !image ||
      !image->instance || !image->physicalDevice || !image->device || !image->queue || !image->image ||
      !image->width || !image->height || image->samples != 1) return false;
  bool srgb = image->format == OPENVR_FORMAT_RGBA8_SRGB || image->format == OPENVR_FORMAT_BGRA8_SRGB;
  bool linear = image->format == OPENVR_FORMAT_RGBA8_UNORM || image->format == OPENVR_FORMAT_BGRA8_UNORM ||
    image->format == OPENVR_FORMAT_RGBA16_FLOAT;
  if ((!srgb && !linear) || (srgb && p->colorSpace != EColorSpace_ColorSpace_Gamma) ||
      (linear && p->colorSpace != EColorSpace_ColorSpace_Linear)) return false;
  VRVulkanTextureData_t vulkan = {
    .m_nImage = image->image, .m_pDevice = (struct VkDevice_T*) image->device,
    .m_pPhysicalDevice = (struct VkPhysicalDevice_T*) image->physicalDevice,
    .m_pInstance = (struct VkInstance_T*) image->instance, .m_pQueue = (struct VkQueue_T*) image->queue,
    .m_nQueueFamilyIndex = image->queueFamily, .m_nWidth = image->width, .m_nHeight = image->height,
    .m_nFormat = image->format, .m_nSampleCount = image->samples
  };
  Texture_t texture = { .handle = &vulkan, .eType = ETextureType_TextureType_Vulkan, .eColorSpace = p->colorSpace };
  h->result = runtimeResult("SetOverlayTexture", h->eye, p->api->SetOverlayTexture(p->eyes[h->eye], &texture));
  p->submitted[h->eye] = h->result.status == OPENVR_PROJECTION_OK;
  return p->submitted[h->eye];
}
