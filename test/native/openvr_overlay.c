#include "test.h"
#include "headset/openvr_overlay.h"
#include <math.h>

static struct {
  unsigned int calls, fail, destroys, hides, shows;
  bool destroyFail, valid;
  char trace[32];
  VROverlayFlags flags[4];
  bool enabled[4];
  unsigned int flagCount;
  float color[4], width, aspect, curve;
  uint32_t order;
  EColorSpace space;
  VRTextureBounds_t bounds;
  ETrackingUniverseOrigin origin;
  TrackedDeviceIndex_t device;
  HmdMatrix34_t pose;
} fake;

static EVROverlayError record(char call, VROverlayHandle_t handle) {
  fake.valid &= handle == 73;
  fake.trace[fake.calls++] = call;
  return fake.calls == fake.fail ? EVROverlayError_VROverlayError_RequestFailed : EVROverlayError_VROverlayError_None;
}

static EVROverlayError OPENVR_FNTABLE_CALLTYPE create(char* key, char* name, VROverlayHandle_t* handle) {
  fake.valid &= strcmp(key, "lunette.panel.17") == 0 && strcmp(name, "panel fixture") == 0;
  EVROverlayError error = record('C', 73);
  if (!error) *handle = 73;
  return error;
}

static EVROverlayError OPENVR_FNTABLE_CALLTYPE destroy(VROverlayHandle_t handle) {
  fake.destroys++;
  EVROverlayError error = record('D', handle);
  return fake.destroyFail ? EVROverlayError_VROverlayError_PermissionDenied : error;
}

static EVROverlayError OPENVR_FNTABLE_CALLTYPE hide(VROverlayHandle_t handle) {
  fake.hides++;
  return record('H', handle);
}

static EVROverlayError OPENVR_FNTABLE_CALLTYPE show(VROverlayHandle_t handle) {
  fake.shows++;
  return record('S', handle);
}

static EVROverlayError OPENVR_FNTABLE_CALLTYPE flag(VROverlayHandle_t handle, VROverlayFlags value, bool enabled) {
  unsigned int i = fake.flagCount++ % 4;
  fake.flags[i] = value;
  fake.enabled[i] = enabled;
  return record('F', handle);
}

static EVROverlayError OPENVR_FNTABLE_CALLTYPE color(VROverlayHandle_t handle, float r, float g, float b) {
  fake.color[0] = r; fake.color[1] = g; fake.color[2] = b;
  return record('R', handle);
}

#define FLOAT_SETTER(name, field, code) \
  static EVROverlayError OPENVR_FNTABLE_CALLTYPE name(VROverlayHandle_t handle, float value) { \
    fake.field = value; return record(code, handle); \
  }
FLOAT_SETTER(alpha, color[3], 'A')
FLOAT_SETTER(width, width, 'W')
FLOAT_SETTER(aspect, aspect, 'X')
FLOAT_SETTER(curve, curve, 'U')

static EVROverlayError OPENVR_FNTABLE_CALLTYPE order(VROverlayHandle_t handle, uint32_t value) {
  fake.order = value;
  return record('O', handle);
}

static EVROverlayError OPENVR_FNTABLE_CALLTYPE space(VROverlayHandle_t handle, EColorSpace value) {
  fake.space = value;
  return record('G', handle);
}

static EVROverlayError OPENVR_FNTABLE_CALLTYPE bounds(VROverlayHandle_t handle, VRTextureBounds_t* value) {
  fake.bounds = *value;
  return record('B', handle);
}

static EVROverlayError OPENVR_FNTABLE_CALLTYPE absolute(VROverlayHandle_t handle, ETrackingUniverseOrigin origin,
    HmdMatrix34_t* pose) {
  fake.origin = origin;
  fake.pose = *pose;
  return record('P', handle);
}

static EVROverlayError OPENVR_FNTABLE_CALLTYPE relative(VROverlayHandle_t handle, TrackedDeviceIndex_t device,
    HmdMatrix34_t* pose) {
  fake.device = device;
  fake.pose = *pose;
  return record('T', handle);
}

static struct VR_IVROverlay_FnTable api = {
  .CreateOverlay = create, .DestroyOverlay = destroy, .HideOverlay = hide, .ShowOverlay = show,
  .SetOverlayFlag = flag, .SetOverlayColor = color, .SetOverlayAlpha = alpha, .SetOverlaySortOrder = order,
  .SetOverlayWidthInMeters = width, .SetOverlayTexelAspect = aspect, .SetOverlayCurvature = curve,
  .SetOverlayTextureColorSpace = space, .SetOverlayTextureBounds = bounds,
  .SetOverlayTransformAbsolute = absolute, .SetOverlayTransformTrackedDeviceRelative = relative
};

static OpenVRPanelConfig config(void) {
  return (OpenVRPanelConfig) {
    .textureWidth = 800, .textureHeight = 400, .viewport = { 200, 100, 400, 100 },
    .width = 3.f, .height = 2.f, .color = { .2f, .4f, .6f, .8f },
    .colorSpace = EColorSpace_ColorSpace_Linear, .premultiplied = true,
    .order = 19, .origin = ETrackingUniverseOrigin_TrackingUniverseStanding,
    .pose = { .m = { { 1, 0, 0, 2 }, { 0, 1, 0, 3 }, { 0, 0, 1, -4 } } }
  };
}

static void reset(void) {
  memset(&fake, 0, sizeof(fake));
  fake.valid = true;
}

static bool properties(void) {
  reset();
  OpenVRPanel panel = { 0 };
  OpenVRPanelConfig c = config();
  CHECK(lovrOpenVRPanelCreate(&panel, &api, "lunette.panel.17", "panel fixture", &c).status == OPENVR_PANEL_OK);
  CHECK(strcmp(fake.trace, "CHFFFFGRAOWBXUP") == 0);
  CHECK(panel.handle == 73 && panel.configured && fake.valid);
  CHECK(fake.flags[0] == VROverlayFlags_NoBackside && fake.enabled[0]);
  CHECK(fake.flags[1] == VROverlayFlags_IsPremultiplied && fake.enabled[1]);
  CHECK(fake.flags[2] == VROverlayFlags_IgnoreTextureAlpha && !fake.enabled[2]);
  CHECK(fake.flags[3] == VROverlayFlags_SideBySide_Parallel && !fake.enabled[3]);
  CHECK(memcmp(fake.color, c.color, sizeof(c.color)) == 0);
  CHECK(fake.width == 3.f && fake.aspect == .375f && fake.curve == 0.f && fake.order == 19);
  CHECK(fake.space == c.colorSpace && fake.origin == c.origin && memcmp(&fake.pose, &c.pose, sizeof(c.pose)) == 0);
  CHECK(fake.bounds.uMin == .25f && fake.bounds.uMax == .75f && fake.bounds.vMin == .25f && fake.bounds.vMax == .5f);
  CHECK(lovrOpenVRPanelShow(&panel).status == OPENVR_PANEL_OK && fake.shows == 1);
  reset();
  c.viewport[2] = c.viewport[3] = 0;
  c.transform = OPENVR_PANEL_DEVICE_RELATIVE;
  c.device = 7;
  c.ignoreTextureAlpha = true;
  c.premultiplied = false;
  c.colorSpace = EColorSpace_ColorSpace_Gamma;
  CHECK(lovrOpenVRPanelConfigure(&panel, &c).status == OPENVR_PANEL_OK);
  CHECK(strcmp(fake.trace, "HFFFFGRAOWBXUT") == 0);
  CHECK(fake.aspect == .75f && fake.bounds.uMax == 1.f && fake.bounds.vMax == 1.f);
  CHECK(fake.device == 7 && fake.space == c.colorSpace && !fake.enabled[1] && fake.enabled[2]);
  CHECK(lovrOpenVRPanelHide(&panel).status == OPENVR_PANEL_OK);
  CHECK(lovrOpenVRPanelDestroy(&panel).status == OPENVR_PANEL_OK);
  CHECK(!panel.handle && !panel.api && !panel.configured);
  unsigned int calls = fake.calls;
  CHECK(lovrOpenVRPanelDestroy(&panel).status == OPENVR_PANEL_OK && fake.calls == calls);
  return true;
}

static bool stereoCurve(void) {
  reset();
  OpenVRPanel panel = { 0 };
  OpenVRPanelConfig c = config();
  c.textureLayout = OPENVR_PANEL_TEXTURE_SIDE_BY_SIDE_2D;
  c.viewport[0] = 0;
  c.curve = .5f;
  CHECK(lovrOpenVRPanelCreate(&panel, &api, "lunette.panel.17", "panel fixture", &c).status == OPENVR_PANEL_OK);
  CHECK(fake.flags[3] == VROverlayFlags_SideBySide_Parallel && fake.enabled[3]);
  CHECK(fake.aspect == .375f);
  CHECK(fake.bounds.uMin == 0.f && fake.bounds.uMax == 1.f);
  CHECK(fake.bounds.vMin == .25f && fake.bounds.vMax == .5f);
  CHECK(fabsf(fake.curve - .2387324146f) < 1e-7f);
  CHECK(memcmp(&fake.pose, &c.pose, sizeof(c.pose)) == 0);
  reset();
  c.curve = 100.f;
  CHECK(lovrOpenVRPanelConfigure(&panel, &c).status == OPENVR_PANEL_OK && fake.curve == 1.f);
  reset();
  c.curve = .0005f;
  CHECK(lovrOpenVRPanelConfigure(&panel, &c).status == OPENVR_PANEL_OK && fake.curve == 0.f);
  reset();
  c.viewport[2] = 399;
  CHECK(lovrOpenVRPanelConfigure(&panel, &c).status == OPENVR_PANEL_UNSUPPORTED && !fake.calls);
  c.viewport[0] = 1;
  c.viewport[2] = 0;
  CHECK(lovrOpenVRPanelConfigure(&panel, &c).status == OPENVR_PANEL_UNSUPPORTED && !fake.calls);
  c.viewport[0] = 0;
  c.textureWidth = 801;
  CHECK(lovrOpenVRPanelConfigure(&panel, &c).status == OPENVR_PANEL_INVALID && !fake.calls);
  CHECK(lovrOpenVRPanelDestroy(&panel).status == OPENVR_PANEL_OK);
  return true;
}

static bool failures(void) {
  for (unsigned int fail = 1; fail <= 15; fail++) {
    reset();
    fake.fail = fail;
    OpenVRPanel panel = { 0 };
    OpenVRPanelConfig c = config();
    OpenVRPanelResult r = lovrOpenVRPanelCreate(&panel, &api, "lunette.panel.17", "panel fixture", &c);
    CHECK(r.status == OPENVR_PANEL_RUNTIME_ERROR && r.error == EVROverlayError_VROverlayError_RequestFailed);
    CHECK(r.operation && !r.cleanupError && !panel.handle && fake.valid);
    CHECK(fake.destroys == (fail > 1));
    reset();
    CHECK(lovrOpenVRPanelCreate(&panel, &api, "lunette.panel.17", "panel fixture", &c).status == OPENVR_PANEL_OK);
    CHECK(lovrOpenVRPanelDestroy(&panel).status == OPENVR_PANEL_OK);
  }
  reset();
  OpenVRPanel panel = { 0 };
  OpenVRPanelConfig c = config();
  fake.fail = 5;
  fake.destroyFail = true;
  OpenVRPanelResult r = lovrOpenVRPanelCreate(&panel, &api, "lunette.panel.17", "panel fixture", &c);
  CHECK(r.error == EVROverlayError_VROverlayError_RequestFailed && r.cleanupError == EVROverlayError_VROverlayError_PermissionDenied);
  CHECK(panel.handle == 73 && !panel.configured);
  CHECK(lovrOpenVRPanelShow(&panel).status == OPENVR_PANEL_INVALID);
  CHECK(lovrOpenVRPanelCreate(&panel, &api, "lunette.panel.17", "panel fixture", &c).status == OPENVR_PANEL_INVALID);
  reset();
  CHECK(lovrOpenVRPanelDestroy(&panel).status == OPENVR_PANEL_OK && !panel.handle);
  CHECK(lovrOpenVRPanelCreate(&panel, &api, "lunette.panel.17", "panel fixture", &c).status == OPENVR_PANEL_OK);
  reset();
  fake.fail = 4;
  CHECK(lovrOpenVRPanelConfigure(&panel, &c).status == OPENVR_PANEL_RUNTIME_ERROR && !panel.configured);
  CHECK(lovrOpenVRPanelShow(&panel).status == OPENVR_PANEL_INVALID);
  reset();
  CHECK(lovrOpenVRPanelConfigure(&panel, &c).status == OPENVR_PANEL_OK);
  reset();
  fake.fail = 1;
  CHECK(lovrOpenVRPanelDestroy(&panel).error == EVROverlayError_VROverlayError_RequestFailed);
  CHECK(!panel.handle && fake.destroys == 1);
  reset();
  CHECK(lovrOpenVRPanelCreate(&panel, &api, "lunette.panel.17", "panel fixture", &c).status == OPENVR_PANEL_OK);
  reset();
  fake.destroyFail = true;
  CHECK(lovrOpenVRPanelDestroy(&panel).error == EVROverlayError_VROverlayError_PermissionDenied);
  CHECK(panel.handle == 73 && !panel.configured);
  reset();
  CHECK(lovrOpenVRPanelDestroy(&panel).status == OPENVR_PANEL_OK && !panel.handle);
  return true;
}

static bool unsupported(void) {
  for (unsigned int i = 0; i < 13; i++) {
    reset();
    OpenVRPanel panel = { 0 };
    OpenVRPanelConfig c = config();
    OpenVRPanelStatus expected = OPENVR_PANEL_INVALID;
    switch (i) {
      case 0: c.textureLayout = OPENVR_PANEL_TEXTURE_ARRAY; expected = OPENVR_PANEL_UNSUPPORTED; break;
      case 1: c.filter = OPENVR_PANEL_FILTER_NEAREST; expected = OPENVR_PANEL_UNSUPPORTED; break;
      case 2: c.filter = OPENVR_PANEL_FILTER_LINEAR; expected = OPENVR_PANEL_UNSUPPORTED; break;
      case 3: c.curve = -1.f; break;
      case 4: c.width = NAN; break;
      case 5: c.height = 0; break;
      case 6: c.viewport[0] = -1; break;
      case 7: c.viewport[2] = 801; break;
      case 8: c.color[3] = 1.1f; break;
      case 9: c.colorSpace = EColorSpace_ColorSpace_Auto; break;
      case 10: c.pose.m[0][3] = INFINITY; break;
      case 11: c.origin = (ETrackingUniverseOrigin) 77; break;
      case 12: c.transform = OPENVR_PANEL_DEVICE_RELATIVE; c.device = k_unTrackedDeviceIndexInvalid; break;
    }
    CHECK(lovrOpenVRPanelCreate(&panel, &api, "lunette.panel.17", "panel fixture", &c).status == expected);
    CHECK(!fake.calls && !panel.handle);
  }
  struct VR_IVROverlay_FnTable incomplete = api;
  incomplete.SetOverlayTexelAspect = NULL;
  OpenVRPanel panel = { 0 };
  OpenVRPanelConfig c = config();
  CHECK(lovrOpenVRPanelCreate(&panel, &incomplete, "lunette.panel.17", "panel fixture", &c).status == OPENVR_PANEL_UNSUPPORTED);
  CHECK(!fake.calls);
  return true;
}

int main(int argc, char** argv) {
  const NativeTest tests[] = {
    { "panel_property_matrix", properties },
    { "panel_stereo_curve_mapping", stereoCurve },
    { "panel_failure_cleanup_retry", failures },
    { "panel_capability_contract", unsupported }
  };
  return nativeRunTests(argc, argv, tests, sizeof(tests) / sizeof(tests[0]));
}
