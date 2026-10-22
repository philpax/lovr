#ifndef LOVR_OPENVR_FOVEATION_H
#define LOVR_OPENVR_FOVEATION_H

#include "headset.h"
#include <stdbool.h>
#include <stdint.h>

// Fragment density map texels are RG8: red scales horizontal density and green vertical density.
#define OPENVR_FOVEATION_TEXEL_BYTES 2
// Overlay texels are RGBA8.
#define OPENVR_FOVEATION_OVERLAY_BYTES 4

typedef struct OpenVRFoveationEye {
  // Raw runtime tangents in Y-down eye space: left, right, top, bottom.
  float tangents[4];
  // The gaze centre in the eye's normalized device coordinates, with Y pointing down as on the Steam Frame.
  float gaze[2];
  bool tracked;
} OpenVRFoveationEye;

bool lovrOpenVRFoveationMapSize(const uint32_t target[2], const uint32_t texel[2], uint32_t size[2]);
bool lovrOpenVRFoveationFill(FoveationLevel level, const OpenVRFoveationEye* eye, const uint32_t target[2],
  const uint32_t texel[2], const uint32_t size[2], uint8_t* texels);
bool lovrOpenVRFoveationOverlay(const OpenVRFoveationEye* eye, const uint32_t target[2], const uint32_t texel[2],
  const uint32_t size[2], const uint8_t* density, uint8_t* overlay);

#endif
