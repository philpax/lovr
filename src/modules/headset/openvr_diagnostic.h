#ifndef LOVR_OPENVR_DIAGNOSTIC_H
#define LOVR_OPENVR_DIAGNOSTIC_H

#ifndef LOVR_ENABLE_OPENVR_DIAGNOSTIC
#error LOVR_ENABLE_OPENVR_DIAGNOSTIC is required
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  bool connected;
  bool cleanupPending;
  uint32_t generation;
  uint32_t currentScenePID;
  uint32_t liveOverlayHandles;
  uint64_t retiredOverlayHandles;
  uint64_t setOverlayTextureCalls;
  uint64_t setOverlayTextureSuccesses;
  int32_t lastSetOverlayTextureError;
} OpenVRDiagnosticObservation;

bool lovrOpenVRDiagnosticObserve(OpenVRDiagnosticObservation* observation);

#endif
