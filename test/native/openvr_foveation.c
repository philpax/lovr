#include "test.h"
#include "headset/openvr_foveation.h"
#include <math.h>
#include <string.h>

#define SIDE 10u
#define TEXELS (SIDE * SIDE * OPENVR_FOVEATION_TEXEL_BYTES)

static const uint32_t target[2] = { 320, 320 };
static const uint32_t texel[2] = { 32, 32 };
static const uint32_t size[2] = { SIDE, SIDE };

static uint8_t at(const uint8_t* texels, uint32_t x, uint32_t y) {
  return texels[(y * SIDE + x) * OPENVR_FOVEATION_TEXEL_BYTES];
}

static OpenVRFoveationEye eye(float gazeX, float gazeY, bool tracked) {
  return (OpenVRFoveationEye) { .tangents = { -1.f, 1.f, -1.f, 1.f }, .gaze = { gazeX, gazeY }, .tracked = tracked };
}

static bool mapSize(void) {
  uint32_t result[2];
  CHECK(lovrOpenVRFoveationMapSize((uint32_t[2]) { 3456, 3456 }, texel, result) && result[0] == 108 && result[1] == 108);
  CHECK(lovrOpenVRFoveationMapSize((uint32_t[2]) { 1729, 1000 }, texel, result) && result[0] == 55 && result[1] == 32);
  CHECK(lovrOpenVRFoveationMapSize((uint32_t[2]) { 1, 1 }, texel, result) && result[0] == 1 && result[1] == 1);
  CHECK(!lovrOpenVRFoveationMapSize((uint32_t[2]) { 0, 32 }, texel, result));
  CHECK(!lovrOpenVRFoveationMapSize(target, (uint32_t[2]) { 32, 0 }, result));
  return true;
}

static bool fixedCentre(void) {
  uint8_t texels[TEXELS];
  OpenVRFoveationEye untracked = eye(.9f, .9f, false);
  CHECK(lovrOpenVRFoveationFill(FOVEATION_HIGH, &untracked, target, texel, size, texels));
  CHECK(at(texels, 4, 4) == 255 && at(texels, 5, 5) == 255);
  CHECK(at(texels, 0, 0) == 64 && at(texels, 9, 9) == 64);
  for (uint32_t i = 0; i < SIDE * SIDE; i++) CHECK(texels[2 * i] == texels[2 * i + 1]);
  return true;
}

static bool gazeHorizontal(void) {
  uint8_t texels[TEXELS];
  OpenVRFoveationEye left = eye(-.9f, 0.f, true);
  CHECK(lovrOpenVRFoveationFill(FOVEATION_HIGH, &left, target, texel, size, texels));
  CHECK(at(texels, 0, 4) == 255 && at(texels, 0, 5) == 255);
  CHECK(at(texels, 9, 4) == 64);
  return true;
}

// The runtime reports gaze with Y down, so a negative Y looks toward the map's first row.
static bool gazeVertical(void) {
  uint8_t texels[TEXELS];
  OpenVRFoveationEye up = eye(0.f, -.9f, true);
  CHECK(lovrOpenVRFoveationFill(FOVEATION_HIGH, &up, target, texel, size, texels));
  CHECK(at(texels, 4, 0) == 255 && at(texels, 5, 0) == 255);
  CHECK(at(texels, 4, 9) == 64);
  return true;
}

static bool levelOrdering(void) {
  uint8_t low[TEXELS], medium[TEXELS], high[TEXELS];
  for (int gy = -4; gy <= 4; gy++) {
    for (int gx = -4; gx <= 4; gx++) {
      OpenVRFoveationEye tracked = eye(gx * .25f, gy * .25f, true);
      CHECK(lovrOpenVRFoveationFill(FOVEATION_LOW, &tracked, target, texel, size, low));
      CHECK(lovrOpenVRFoveationFill(FOVEATION_MEDIUM, &tracked, target, texel, size, medium));
      CHECK(lovrOpenVRFoveationFill(FOVEATION_HIGH, &tracked, target, texel, size, high));
      for (uint32_t i = 0; i < TEXELS; i++) {
        CHECK(low[i] >= medium[i] && medium[i] >= high[i]);
        CHECK(low[i] >= 128 && high[i] >= 64);
      }
    }
  }
  return true;
}

static bool invalidInput(void) {
  uint8_t texels[TEXELS], reference[TEXELS];
  OpenVRFoveationEye valid = eye(0.f, 0.f, true);
  CHECK(!lovrOpenVRFoveationFill(FOVEATION_NONE, &valid, target, texel, size, texels));
  CHECK(!lovrOpenVRFoveationFill((FoveationLevel) 7, &valid, target, texel, size, texels));
  CHECK(!lovrOpenVRFoveationFill(FOVEATION_LOW, &valid, target, texel, (uint32_t[2]) { SIDE, SIDE - 1 }, texels));
  CHECK(!lovrOpenVRFoveationFill(FOVEATION_LOW, NULL, target, texel, size, texels));
  CHECK(!lovrOpenVRFoveationFill(FOVEATION_LOW, &valid, target, texel, size, NULL));
  OpenVRFoveationEye inverted = valid;
  inverted.tangents[2] = 1.f;
  inverted.tangents[3] = -1.f;
  CHECK(!lovrOpenVRFoveationFill(FOVEATION_LOW, &inverted, target, texel, size, texels));
  OpenVRFoveationEye nonfinite = eye(NAN, 0.f, true);
  OpenVRFoveationEye untracked = eye(0.f, 0.f, false);
  CHECK(lovrOpenVRFoveationFill(FOVEATION_MEDIUM, &nonfinite, target, texel, size, texels));
  CHECK(lovrOpenVRFoveationFill(FOVEATION_MEDIUM, &untracked, target, texel, size, reference));
  CHECK(memcmp(texels, reference, TEXELS) == 0);
  return true;
}

static bool overlayTint(void) {
  uint8_t density[TEXELS], overlay[SIDE * SIDE * OPENVR_FOVEATION_OVERLAY_BYTES];
  OpenVRFoveationEye left = eye(-.9f, 0.f, true);
  CHECK(lovrOpenVRFoveationFill(FOVEATION_HIGH, &left, target, texel, size, density));
  CHECK(lovrOpenVRFoveationOverlay(&left, target, texel, size, density, overlay));
  const uint8_t* gaze = overlay + (5 * SIDE + 0) * OPENVR_FOVEATION_OVERLAY_BYTES;
  CHECK(gaze[0] == 0 && gaze[1] == 255 && gaze[2] == 255 && gaze[3] == 220);
  const uint8_t* full = overlay + (4 * SIDE + 1) * OPENVR_FOVEATION_OVERLAY_BYTES;
  CHECK(at(density, 1, 4) == 255 && full[3] == 0);
  // The lowest density is the reddest and most opaque.
  const uint8_t* floor = overlay + (5 * SIDE + 9) * OPENVR_FOVEATION_OVERLAY_BYTES;
  CHECK(at(density, 9, 5) == 64 && floor[0] == 255 && floor[1] == 40 && floor[3] == 160);
  for (uint32_t i = 0; i < SIDE * SIDE; i++) {
    const uint8_t* texel = overlay + i * OPENVR_FOVEATION_OVERLAY_BYTES;
    bool marked = texel[0] == 0 && texel[3] == 220;
    CHECK(marked || (density[2 * i] == 255) == (texel[3] == 0));
  }
  CHECK(!lovrOpenVRFoveationOverlay(&left, target, texel, (uint32_t[2]) { SIDE, SIDE + 1 }, density, overlay));
  CHECK(!lovrOpenVRFoveationOverlay(&left, target, texel, size, NULL, overlay));
  return true;
}

int main(int argc, char** argv) {
  const NativeTest tests[] = {
    { "foveation_map_size", mapSize },
    { "foveation_fixed_centre", fixedCentre },
    { "foveation_gaze_horizontal", gazeHorizontal },
    { "foveation_gaze_vertical", gazeVertical },
    { "foveation_level_ordering", levelOrdering },
    { "foveation_invalid_input", invalidInput },
    { "foveation_overlay_tint", overlayTint }
  };
  return nativeRunTests(argc, argv, tests, sizeof(tests) / sizeof(tests[0]));
}
