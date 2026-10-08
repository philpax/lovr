#include "test.h"
#include "headset/openvr_projection.h"
#include <math.h>

static struct {
  unsigned int creates, destroys[2], hides[2], shows[2], textures[2];
  unsigned int flags[2], orders[2];
  bool valid, failCreate, failConfigure;
  int failHide, failDestroy, failShow, failTexture;
  EColorSpace spaces[2];
  ETrackingUniverseOrigin origins[2];
  HmdMatrix34_t poses[2];
  VROverlayProjection_t frusta[2];
  VRVulkanTextureData_t images[2];
} fake;

static unsigned int indexOf(VROverlayHandle_t handle) {
  fake.valid &= handle == 71 || handle == 72;
  return handle == 72;
}

static EVROverlayError OPENVR_FNTABLE_CALLTYPE create(char* key, char* name, VROverlayHandle_t* handle) {
  unsigned int eye = fake.creates++;
  fake.valid &= eye < 2 && strcmp(key, eye ? "main.right" : "main.left") == 0 && strcmp(name, "scene") == 0;
  if (fake.failCreate && eye == 1) return EVROverlayError_VROverlayError_RequestFailed;
  *handle = 71 + eye;
  return 0;
}

static EVROverlayError OPENVR_FNTABLE_CALLTYPE destroy(VROverlayHandle_t handle) {
  unsigned int eye = indexOf(handle);
  fake.destroys[eye]++;
  return fake.failDestroy == (int) eye ? EVROverlayError_VROverlayError_RequestFailed : 0;
}

static EVROverlayError OPENVR_FNTABLE_CALLTYPE hide(VROverlayHandle_t handle) {
  unsigned int eye = indexOf(handle);
  fake.hides[eye]++;
  return (fake.failHide == (int) eye || fake.failHide == 2) ? EVROverlayError_VROverlayError_RequestFailed : 0;
}

static EVROverlayError OPENVR_FNTABLE_CALLTYPE show(VROverlayHandle_t handle) {
  unsigned int eye = indexOf(handle);
  fake.shows[eye]++;
  return fake.failShow == (int) eye ? EVROverlayError_VROverlayError_RequestFailed : 0;
}

static EVROverlayError OPENVR_FNTABLE_CALLTYPE flag(VROverlayHandle_t handle, VROverlayFlags flag, bool enabled) {
  unsigned int eye = indexOf(handle);
  fake.valid &= flag == VROverlayFlags_NoBackside || flag == VROverlayFlags_IsPremultiplied ||
    flag == VROverlayFlags_IgnoreTextureAlpha;
  fake.valid &= enabled == (flag != VROverlayFlags_IgnoreTextureAlpha);
  fake.flags[eye]++;
  return fake.failConfigure ? EVROverlayError_VROverlayError_RequestFailed : 0;
}

static EVROverlayError OPENVR_FNTABLE_CALLTYPE order(VROverlayHandle_t handle, uint32_t value) {
  fake.orders[indexOf(handle)] = value;
  return 0;
}

static EVROverlayError OPENVR_FNTABLE_CALLTYPE space(VROverlayHandle_t handle, EColorSpace value) {
  fake.spaces[indexOf(handle)] = value;
  return 0;
}

static EVROverlayError OPENVR_FNTABLE_CALLTYPE projection(VROverlayHandle_t handle,
    ETrackingUniverseOrigin origin, HmdMatrix34_t* pose, VROverlayProjection_t* frustum, EVREye eye) {
  unsigned int i = indexOf(handle);
  fake.valid &= eye == (i ? EVREye_Eye_Right : EVREye_Eye_Left);
  fake.origins[i] = origin;
  fake.poses[i] = *pose;
  fake.frusta[i] = *frustum;
  return 0;
}

static EVROverlayError OPENVR_FNTABLE_CALLTYPE texture(VROverlayHandle_t handle, Texture_t* texture) {
  unsigned int eye = indexOf(handle);
  fake.valid &= texture->eType == ETextureType_TextureType_Vulkan && texture->eColorSpace == fake.spaces[eye];
  fake.images[eye] = *(VRVulkanTextureData_t*) texture->handle;
  fake.textures[eye]++;
  return fake.failTexture == (int) eye ? EVROverlayError_VROverlayError_RequestFailed : 0;
}

static struct VR_IVROverlay_FnTable api = {
  .CreateOverlay = create, .DestroyOverlay = destroy, .HideOverlay = hide, .ShowOverlay = show,
  .SetOverlayFlag = flag, .SetOverlaySortOrder = order, .SetOverlayTextureColorSpace = space,
  .SetOverlayTransformProjection = projection, .SetOverlayTexture = texture
};

static void reset(void) {
  memset(&fake, 0, sizeof(fake));
  fake.valid = true;
  fake.failHide = fake.failDestroy = fake.failShow = fake.failTexture = -1;
}

static OpenVRProjectionConfig config(void) {
  return (OpenVRProjectionConfig) {
    .origin = ETrackingUniverseOrigin_TrackingUniverseStanding,
    .views = { { .m = { { 0, 0, 1, -.03f }, { 0, 1, 0, 2 }, { -1, 0, 0, 3 } } },
      { .m = { { 0, 0, 1, .03f }, { 0, 1, 0, 2 }, { -1, 0, 0, 3 } } } },
    .frusta = { { -1.1f, .9f, -.8f, 1.2f }, { -.9f, 1.1f, -1.2f, .8f } },
    .colorSpace = EColorSpace_ColorSpace_Gamma, .order = 20
  };
}

static OpenVRProjectionResult start(OpenVRProjection* projection) {
  const char* keys[2] = { "main.left", "main.right" };
  const char* names[2] = { "scene", "scene" };
  OpenVRProjectionConfig c = config();
  return lovrOpenVRProjectionCreate(projection, &api, keys, names, &c);
}

static gpu_external_image image(unsigned int eye) {
  return (gpu_external_image) { .instance = 1, .physicalDevice = 2, .device = 3, .queue = 4,
    .image = 900 + eye, .queueFamily = 7, .width = 1200, .height = 1300, .format = 43, .samples = 1 };
}

// Field of view angles in OpenXR's convention: left and down are negative.
typedef struct { float left, right, up, down; } Fov;

// Raw tangents as an OpenVR driver reports them, in Y-down eye space.
static void rawTangents(Fov f, float tangents[4]) {
  tangents[0] = tanf(f.left); tangents[1] = tanf(f.right);
  tangents[2] = tanf(-f.up); tangents[3] = tanf(-f.down);
}

static HmdMatrix34_t rigid(float yaw, float pitch, float x, float y, float z) {
  float cy = cosf(yaw), sy = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch);
  return (HmdMatrix34_t) { .m = { { cy, sy * sp, sy * cp, x }, { 0, cp, -sp, y }, { -sy, cy * sp, cy * cp, z } } };
}

static bool close(float a, float b) { return fabsf(a - b) < 1e-5f; }

static bool eyeConversion(void) {
  Fov fovs[] = {
    { -1.02f, .88f, 1.05f, -.86f }, { -.8f, .8f, .8f, -.8f }, { -.2f, .9f, .4f, .1f }, { -.6f, -.1f, -.2f, -.7f }
  };
  HmdMatrix34_t poses[] = {
    rigid(0.f, 0.f, 0.f, 0.f, 0.f), rigid(.4f, -.35f, -.03f, 1.67f, .06f), rigid(-2.9f, 1.2f, 4.f, -2.f, .5f)
  };
  for (unsigned int i = 0; i < sizeof(poses) / sizeof(poses[0]); i++) {
    for (unsigned int j = 0; j < sizeof(fovs) / sizeof(fovs[0]); j++) {
      float tangents[4];
      rawTangents(fovs[j], tangents);
      HmdMatrix34_t view;
      VROverlayProjection_t frustum;
      CHECK(lovrOpenVRProjectionEye(&poses[i], tangents, &view, &frustum));
      const HmdMatrix34_t* m = &poses[i];
      // The view undoes the pose: view * pose is the identity.
      for (unsigned int row = 0; row < 3; row++) {
        for (unsigned int column = 0; column < 4; column++) {
          float value = column == 3 ? view.m[row][3] : 0.f;
          for (unsigned int k = 0; k < 3; k++) value += view.m[row][k] * m->m[k][column];
          CHECK(close(value, row == column ? 1.f : 0.f));
        }
      }
      // A point one metre ahead of the eye lands one metre ahead in the view's space.
      float ahead[3];
      for (unsigned int row = 0; row < 3; row++) ahead[row] = m->m[row][3] - m->m[row][2];
      for (unsigned int row = 0; row < 3; row++) {
        float value = view.m[row][3];
        for (unsigned int k = 0; k < 3; k++) value += view.m[row][k] * ahead[k];
        CHECK(close(value, row == 2 ? -1.f : 0.f));
      }
      // SteamVR's OpenXR runtime fills the overlay frustum as tan(left), tan(right), tan(down), tan(up).
      CHECK(close(frustum.fLeft, tanf(fovs[j].left)) && close(frustum.fRight, tanf(fovs[j].right)));
      CHECK(close(frustum.fTop, tanf(fovs[j].down)) && close(frustum.fBottom, tanf(fovs[j].up)));
      CHECK(frustum.fTop < frustum.fBottom);
    }
  }
  float tangents[4];
  rawTangents(fovs[0], tangents);
  HmdMatrix34_t view;
  VROverlayProjection_t frustum;
  CHECK(!lovrOpenVRProjectionEye(NULL, tangents, &view, &frustum));
  CHECK(!lovrOpenVRProjectionEye(&poses[1], NULL, &view, &frustum));
  CHECK(!lovrOpenVRProjectionEye(&poses[1], tangents, NULL, &frustum));
  CHECK(!lovrOpenVRProjectionEye(&poses[1], tangents, &view, NULL));
  HmdMatrix34_t broken = poses[1];
  broken.m[1][3] = NAN;
  CHECK(!lovrOpenVRProjectionEye(&broken, tangents, &view, &frustum));
  float bad[4] = { tangents[0], tangents[1], tangents[2], INFINITY };
  CHECK(!lovrOpenVRProjectionEye(&poses[1], bad, &view, &frustum));
  float reversed[4] = { tangents[1], tangents[0], tangents[2], tangents[3] };
  CHECK(!lovrOpenVRProjectionEye(&poses[1], reversed, &view, &frustum));
  float inverted[4] = { tangents[0], tangents[1], tangents[3], tangents[2] };
  CHECK(!lovrOpenVRProjectionEye(&poses[1], inverted, &view, &frustum));
  return true;
}

static bool metadata(void) {
  reset();
  OpenVRProjection p = { 0 };
  CHECK(start(&p).status == OPENVR_PROJECTION_OK);
  OpenVRProjectionConfig c = config();
  for (unsigned int eye = 0; eye < 2; eye++) {
    CHECK(p.eyes[eye] == 71 + eye && fake.flags[eye] == 3 && fake.orders[eye] == c.order);
    CHECK(fake.origins[eye] == c.origin && fake.spaces[eye] == c.colorSpace);
    CHECK(memcmp(&fake.poses[eye], &c.views[eye], sizeof(HmdMatrix34_t)) == 0);
    CHECK(memcmp(&fake.frusta[eye], &c.frusta[eye], sizeof(VROverlayProjection_t)) == 0);
    gpu_external_image source = image(eye);
    OpenVRProjectionHandoff h = { .projection = &p, .eye = eye };
    CHECK(lovrOpenVRProjectionHandoff(&source, &h) && h.result.status == OPENVR_PROJECTION_OK);
    VRVulkanTextureData_t* v = &fake.images[eye];
    CHECK(v->m_nImage == source.image && (uintptr_t) v->m_pInstance == source.instance);
    CHECK((uintptr_t) v->m_pDevice == source.device && (uintptr_t) v->m_pPhysicalDevice == source.physicalDevice);
    CHECK((uintptr_t) v->m_pQueue == source.queue && v->m_nQueueFamilyIndex == source.queueFamily);
    CHECK(v->m_nWidth == source.width && v->m_nHeight == source.height);
    CHECK(v->m_nFormat == source.format && v->m_nSampleCount == 1);
  }
  CHECK(lovrOpenVRProjectionShow(&p).status == OPENVR_PROJECTION_OK && p.visible[0] && p.visible[1]);
  uint32_t panelOrder;
  CHECK(lovrOpenVRProjectionPanelOrder(c.order, 0, &panelOrder) && panelOrder > fake.orders[0]);
  const uint32_t formats[] = { 37, 43, 44, 50, 97 };
  const EColorSpace matchingSpaces[] = { EColorSpace_ColorSpace_Linear, EColorSpace_ColorSpace_Gamma,
    EColorSpace_ColorSpace_Linear, EColorSpace_ColorSpace_Gamma, EColorSpace_ColorSpace_Linear };
  c.origin = ETrackingUniverseOrigin_TrackingUniverseSeated;
  for (unsigned int space = 0; space < 2; space++) {
    c.colorSpace = space ? EColorSpace_ColorSpace_Linear : EColorSpace_ColorSpace_Gamma;
    CHECK(lovrOpenVRProjectionConfigure(&p, &c).status == OPENVR_PROJECTION_OK);
    for (unsigned int eye = 0; eye < 2; eye++) {
      OpenVRProjectionHandoff h = { .projection = &p, .eye = eye };
      gpu_external_image source = image(eye);
      for (unsigned int i = 0; i < sizeof(formats) / sizeof(formats[0]); i++) {
        source.format = formats[i];
        unsigned int calls = fake.textures[eye];
        bool accepted = matchingSpaces[i] == c.colorSpace;
        CHECK(lovrOpenVRProjectionHandoff(&source, &h) == accepted);
        CHECK(p.submitted[eye] == accepted && fake.textures[eye] == calls + accepted);
        CHECK(h.result.status == (accepted ? OPENVR_PROJECTION_OK : OPENVR_PROJECTION_INVALID));
        if (accepted) CHECK(fake.images[eye].m_nFormat == formats[i]);
      }
      CHECK(fake.spaces[eye] == c.colorSpace && fake.origins[eye] == c.origin);
    }
  }
  CHECK(lovrOpenVRProjectionDestroy(&p).status == OPENVR_PROJECTION_OK && !p.api);
  CHECK(fake.valid);
  return true;
}

static bool cleanupRetry(void) {
  for (int eye = 0; eye < 2; eye++) {
    reset();
    OpenVRProjection p = { 0 };
    CHECK(start(&p).status == OPENVR_PROJECTION_OK);
    p.visible[0] = p.visible[1] = true;
    fake.failHide = eye;
    CHECK(lovrOpenVRProjectionHide(&p).status == OPENVR_PROJECTION_RUNTIME_ERROR);
    CHECK(p.visible[eye] && !p.visible[1 - eye]);
    CHECK(lovrOpenVRProjectionDestroy(&p).status == OPENVR_PROJECTION_RUNTIME_ERROR);
    CHECK(p.eyes[eye] == (VROverlayHandle_t) (71 + eye) && !p.eyes[1 - eye] && p.api);
    CHECK(fake.destroys[eye] == 0 && fake.destroys[1 - eye] == 1);
    fake.failHide = -1;
    fake.failDestroy = eye;
    CHECK(lovrOpenVRProjectionDestroy(&p).status == OPENVR_PROJECTION_RUNTIME_ERROR);
    CHECK(p.eyes[eye] && !p.visible[eye] && p.api);
    fake.failDestroy = -1;
    CHECK(lovrOpenVRProjectionDestroy(&p).status == OPENVR_PROJECTION_OK && !p.api);
    CHECK(lovrOpenVRProjectionDestroy(&p).status == OPENVR_PROJECTION_OK);
  }
  reset();
  OpenVRProjection p = { 0 };
  CHECK(start(&p).status == OPENVR_PROJECTION_OK);
  p.visible[0] = p.visible[1] = true;
  fake.failHide = 2;
  OpenVRProjectionResult r = lovrOpenVRProjectionHide(&p);
  CHECK(r.status == OPENVR_PROJECTION_RUNTIME_ERROR && r.error && r.cleanupError);
  CHECK(p.eyes[0] == 71 && p.eyes[1] == 72 && p.visible[0] && p.visible[1]);
  r = lovrOpenVRProjectionDestroy(&p);
  CHECK(r.status == OPENVR_PROJECTION_RUNTIME_ERROR && r.error && r.cleanupError);
  CHECK(p.eyes[0] == 71 && p.eyes[1] == 72 && !fake.destroys[0] && !fake.destroys[1]);
  fake.failHide = -1;
  CHECK(lovrOpenVRProjectionDestroy(&p).status == OPENVR_PROJECTION_OK);
  return true;
}

static bool partialCreation(void) {
  reset();
  OpenVRProjection p = { 0 };
  fake.failCreate = true;
  fake.failDestroy = 0;
  OpenVRProjectionResult r = start(&p);
  CHECK(r.status == OPENVR_PROJECTION_RUNTIME_ERROR && r.cleanupError && p.eyes[0] == 71 && !p.eyes[1]);
  CHECK(start(&p).status == OPENVR_PROJECTION_INVALID);
  fake.failDestroy = -1;
  CHECK(lovrOpenVRProjectionDestroy(&p).status == OPENVR_PROJECTION_OK && !p.api);
  reset();
  fake.failConfigure = true;
  fake.failHide = 1;
  r = start(&p);
  CHECK(r.status == OPENVR_PROJECTION_RUNTIME_ERROR && r.cleanupError && p.eyes[1] == 72);
  fake.failHide = -1;
  CHECK(lovrOpenVRProjectionDestroy(&p).status == OPENVR_PROJECTION_OK);
  return true;
}

static bool failureChecks(void) {
  reset();
  uint32_t panelOrder = 17;
  CHECK(!lovrOpenVRProjectionPanelOrder(UINT32_MAX, 0, &panelOrder) && panelOrder == 17);
  CHECK(!lovrOpenVRProjectionPanelOrder(0, UINT32_MAX, &panelOrder) && panelOrder == 17);
  CHECK(!lovrOpenVRProjectionPanelOrder(UINT32_MAX - 1, 1, &panelOrder) && panelOrder == 17);
  CHECK(lovrOpenVRProjectionPanelOrder(UINT32_MAX - 1, 0, &panelOrder) && panelOrder == UINT32_MAX);
  CHECK(lovrOpenVRProjectionPanelOrder(20, 3, &panelOrder) && panelOrder == 24);
  CHECK(!lovrOpenVRProjectionPanelOrder(0, 0, NULL));
  OpenVRProjection p = { 0 };
  CHECK(start(&p).status == OPENVR_PROJECTION_OK);
  CHECK(lovrOpenVRProjectionShow(&p).status == OPENVR_PROJECTION_INVALID);
  for (unsigned int eye = 0; eye < 2; eye++) {
    OpenVRProjectionHandoff h = { .projection = &p, .eye = eye };
    gpu_external_image source = image(eye);
    fake.failTexture = eye;
    CHECK(!lovrOpenVRProjectionHandoff(&source, &h) && h.result.status == OPENVR_PROJECTION_RUNTIME_ERROR);
    fake.failTexture = -1;
    CHECK(lovrOpenVRProjectionHandoff(&source, &h));
  }
  fake.failShow = 1;
  CHECK(lovrOpenVRProjectionShow(&p).status == OPENVR_PROJECTION_RUNTIME_ERROR);
  CHECK(p.visible[0] && !p.visible[1]);
  fake.failShow = -1;
  CHECK(lovrOpenVRProjectionShow(&p).status == OPENVR_PROJECTION_OK);
  OpenVRProjectionConfig c = config();
  c.order = UINT32_MAX;
  CHECK(lovrOpenVRProjectionConfigure(&p, &c).status == OPENVR_PROJECTION_INVALID);
  CHECK(p.visible[0] && p.visible[1]);
  c = config();
  c.frusta[1].fRight = NAN;
  CHECK(lovrOpenVRProjectionConfigure(&p, &c).status == OPENVR_PROJECTION_INVALID);
  OpenVRProjectionHandoff h = { .projection = &p, .eye = 0 };
  gpu_external_image source = image(0);
  source.samples = 4;
  CHECK(!lovrOpenVRProjectionHandoff(&source, &h) && h.result.status == OPENVR_PROJECTION_INVALID);
  CHECK(!p.submitted[0] && p.submitted[1]);
  CHECK(lovrOpenVRProjectionShow(&p).status == OPENVR_PROJECTION_INVALID);
  source = image(0);
  CHECK(lovrOpenVRProjectionHandoff(&source, &h));
  CHECK(lovrOpenVRProjectionShow(&p).status == OPENVR_PROJECTION_OK);
  source = image(0); source.image = 0;
  CHECK(!lovrOpenVRProjectionHandoff(&source, &h));
  source = image(0); source.format = 126;
  CHECK(!lovrOpenVRProjectionHandoff(&source, &h));
  h.eye = 2;
  source = image(0);
  CHECK(!lovrOpenVRProjectionHandoff(&source, &h));
  CHECK(lovrOpenVRProjectionDestroy(&p).status == OPENVR_PROJECTION_OK);
  h.eye = 0;
  CHECK(!lovrOpenVRProjectionHandoff(&source, &h));
  return true;
}

int main(int argc, char** argv) {
  NativeTest tests[] = {
    { "projection_eye_conversion", eyeConversion },
    { "projection_eye_mapping", metadata },
    { "projection_cleanup_retry", cleanupRetry },
    { "projection_partial_creation", partialCreation },
    { "projection_failure_checks", failureChecks }
  };
  return nativeRunTests(argc, argv, tests, sizeof(tests) / sizeof(tests[0]));
}
