#include "headset/openvr_assets.h"

__attribute__((visibility("default"))) OpenVRAssetsResult stagedOpenVRAssetsResolve(void) {
  return lovrOpenVRAssetsResolve(true, NULL);
}
