#ifndef LOVR_OPENVR_RECENTER_H
#define LOVR_OPENVR_RECENTER_H

#include "headset/headset.h"
#include "headset/openvr_runtime.h"

HeadsetRecenterStatus lovrOpenVRGetRecenterStatus(const OpenVRRuntime* runtime, ETrackingUniverseOrigin origin);
bool lovrOpenVRRecenter(const OpenVRRuntime* runtime, ETrackingUniverseOrigin origin);

#endif
