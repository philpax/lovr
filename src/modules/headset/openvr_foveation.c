#include "openvr_foveation.h"
#include <math.h>

typedef struct {
  float inner;
  float outer;
  float floor;
} Profile;

static bool validTangents(const float* tangents);
static bool validExtent(const uint32_t extent[2]);
static float density(const Profile* profile, float degrees);
static void gazeTangent(const OpenVRFoveationEye* eye, float* x, float* y);

// Each level keeps full density within the inner cone around the gaze and falls to the floor at the
// outer cone. The angles are in degrees from the gaze direction.
static const Profile profiles[] = {
  [FOVEATION_LOW] = { 20.f, 35.f, .5f },
  [FOVEATION_MEDIUM] = { 15.f, 30.f, .35f },
  [FOVEATION_HIGH] = { 10.f, 25.f, .25f }
};

bool lovrOpenVRFoveationMapSize(const uint32_t target[2], const uint32_t texel[2], uint32_t size[2]) {
  if (!target || !texel || !size || !validExtent(target) || !validExtent(texel)) return false;
  size[0] = target[0] / texel[0] + (target[0] % texel[0] != 0);
  size[1] = target[1] / texel[1] + (target[1] % texel[1] != 0);
  return true;
}

bool lovrOpenVRFoveationFill(FoveationLevel level, const OpenVRFoveationEye* eye, const uint32_t target[2],
    const uint32_t texel[2], const uint32_t size[2], uint8_t* texels) {
  uint32_t expected[2];
  if (!eye || !texels || !validTangents(eye->tangents) || !lovrOpenVRFoveationMapSize(target, texel, expected) ||
      !size || size[0] != expected[0] || size[1] != expected[1]) return false;
  if (level != FOVEATION_LOW && level != FOVEATION_MEDIUM && level != FOVEATION_HIGH) return false;

  float left = eye->tangents[0], right = eye->tangents[1], top = eye->tangents[2], bottom = eye->tangents[3];
  float gazeX, gazeY;
  gazeTangent(eye, &gazeX, &gazeY);
  float gazeLength = sqrtf(gazeX * gazeX + gazeY * gazeY + 1.f);

  const Profile* profile = &profiles[level];
  for (uint32_t y = 0; y < size[1]; y++) {
    float v = ((y + .5f) * texel[1]) / target[1];
    float tangentY = top + v * (bottom - top);
    for (uint32_t x = 0; x < size[0]; x++) {
      float u = ((x + .5f) * texel[0]) / target[0];
      float tangentX = left + u * (right - left);
      float length = sqrtf(tangentX * tangentX + tangentY * tangentY + 1.f);
      float cosine = (tangentX * gazeX + tangentY * gazeY + 1.f) / (length * gazeLength);
      float degrees = acosf(fminf(fmaxf(cosine, -1.f), 1.f)) * 180.f / (float) M_PI;
      uint8_t value = (uint8_t) lroundf(density(profile, degrees) * 255.f);
      uint8_t* out = texels + ((size_t) y * size[0] + x) * OPENVR_FOVEATION_TEXEL_BYTES;
      out[0] = value;
      out[1] = value;
    }
  }
  return true;
}

// Leaves full density transparent and tints reduced density from yellow toward red, more opaque as it
// falls, so the overlay shows where the periphery is cheaper. A cyan cross marks the gaze.
bool lovrOpenVRFoveationOverlay(const OpenVRFoveationEye* eye, const uint32_t target[2], const uint32_t texel[2],
    const uint32_t size[2], const uint8_t* density, uint8_t* overlay) {
  uint32_t expected[2];
  if (!eye || !density || !overlay || !validTangents(eye->tangents) ||
      !lovrOpenVRFoveationMapSize(target, texel, expected) || !size || size[0] != expected[0] ||
      size[1] != expected[1]) return false;
  for (size_t i = 0; i < (size_t) size[0] * size[1]; i++) {
    uint8_t* out = overlay + i * OPENVR_FOVEATION_OVERLAY_BYTES;
    float value = density[i * OPENVR_FOVEATION_TEXEL_BYTES] / 255.f;
    if (value >= 1.f) {
      out[0] = out[1] = out[2] = out[3] = 0;
      continue;
    }
    float t = fminf((1.f - value) / .75f, 1.f);
    out[0] = 255;
    out[1] = (uint8_t) lroundf(220.f - 180.f * t);
    out[2] = 0;
    out[3] = (uint8_t) lroundf(48.f + 112.f * t);
  }
  float gazeX, gazeY;
  gazeTangent(eye, &gazeX, &gazeY);
  float left = eye->tangents[0], right = eye->tangents[1], top = eye->tangents[2], bottom = eye->tangents[3];
  float x = (gazeX - left) / (right - left) * target[0] / texel[0];
  float y = (gazeY - top) / (bottom - top) * target[1] / texel[1];
  if (!(x >= 0.f && y >= 0.f && x < size[0] && y < size[1])) return true;
  int32_t cx = (int32_t) x, cy = (int32_t) y;
  for (int32_t d = -1; d <= 1; d++) {
    int32_t points[2][2] = { { cx + d, cy }, { cx, cy + d } };
    for (unsigned p = 0; p < 2; p++) {
      if (points[p][0] < 0 || points[p][1] < 0 || points[p][0] >= (int32_t) size[0] || points[p][1] >= (int32_t) size[1]) continue;
      uint8_t* out = overlay + ((size_t) points[p][1] * size[0] + points[p][0]) * OPENVR_FOVEATION_OVERLAY_BYTES;
      out[0] = 0;
      out[1] = out[2] = 255;
      out[3] = 220;
    }
  }
  return true;
}

// The gaze direction as Y-down eye-space tangents, or the optical axis when the gaze is unknown.
static void gazeTangent(const OpenVRFoveationEye* eye, float* x, float* y) {
  *x = *y = 0.f;
  if (!eye->tracked || !isfinite(eye->gaze[0]) || !isfinite(eye->gaze[1])) return;
  float left = eye->tangents[0], right = eye->tangents[1], top = eye->tangents[2], bottom = eye->tangents[3];
  *x = left + (eye->gaze[0] + 1.f) * .5f * (right - left);
  *y = top + (eye->gaze[1] + 1.f) * .5f * (bottom - top);
}

static bool validTangents(const float* tangents) {
  for (unsigned i = 0; i < 4; i++) if (!isfinite(tangents[i])) return false;
  return tangents[0] < tangents[1] && tangents[2] < tangents[3];
}

static bool validExtent(const uint32_t extent[2]) {
  return extent[0] > 0 && extent[1] > 0;
}

static float density(const Profile* profile, float degrees) {
  float t = (degrees - profile->inner) / (profile->outer - profile->inner);
  t = fminf(fmaxf(t, 0.f), 1.f);
  float eased = t * t * (3.f - 2.f * t);
  return 1.f + (profile->floor - 1.f) * eased;
}
