#ifndef LOVR_MONADO_RECENTER_H
#define LOVR_MONADO_RECENTER_H

#include "headset/headset.h"
// Official BSL-1.0 header: Monado 3acbda6b7dacbe014749020e8543fded54ef076b, API 1.8.0.
// Source: src/xrt/targets/libmonado/monado.h; license: deps/monado/LICENSE.
#include <monado.h>

// The loader receives only the explicit application path; no discovery or environment lookup occurs.
typedef struct MonadoRecenterLoader {
  void* context;
  void* (*open)(void* context, const char* path);
  void* (*symbol)(void* context, void* library, const char* name);
  void (*close)(void* context, void* library);
} MonadoRecenterLoader;

typedef struct MonadoRecenter {
  char* path;
  MonadoRecenterLoader loader;
  void* library;
  mnd_root_t* root;
  bool unsupported;
  // Cleared at each reset entry, set before the setter, and retained even on setter error.
  bool writeAttempted;
  void (*getVersion)(uint32_t*, uint32_t*, uint32_t*);
  mnd_result_t (*create)(mnd_root_t**);
  void (*destroy)(mnd_root_t**);
  mnd_result_t (*getOffset)(mnd_root_t*, mnd_reference_space_type_t, mnd_pose_t*);
  mnd_result_t (*setOffset)(mnd_root_t*, mnd_reference_space_type_t, const mnd_pose_t*);
} MonadoRecenter;

// Init requires a fresh or destroyed adapter, copies the path, and defers loading and connection.
bool lovrMonadoRecenterInit(MonadoRecenter* adapter, const char* path);
bool lovrMonadoRecenterInitWithLoader(MonadoRecenter* adapter, const char* path, const MonadoRecenterLoader* loader);
void lovrMonadoRecenterDestroy(MonadoRecenter* adapter);

// The tracked head pose is relative to the selected underlying space; orientation is x, y, z, w.
// Status probes the selected offset over IPC and does not modify it.
// libmonado addresses shared semantic spaces, not necessarily a client's per-application spaces.
// LOCAL_FLOOR status and reset are unsupported without a write attempt; reads remain available.
// LOCAL writes also update shared LOCAL_FLOOR X/Z and yaw.
// A caller targeting that family supplies a LOCAL-relative head pose and selects LOCAL explicitly.
// Runtime setter errors can occur after mutation; false does not guarantee an unchanged runtime.
HeadsetRecenterStatus lovrMonadoGetRecenterStatus(MonadoRecenter* adapter, mnd_reference_space_type_t space,
  const float headPosition[3], const float headOrientation[4], bool tracked);
bool lovrMonadoRecenter(MonadoRecenter* adapter, mnd_reference_space_type_t space,
  const float headPosition[3], const float headOrientation[4], bool tracked);

// Read before sampling the head, then use FromOffset to reject a different final offset.
// This cannot detect changes that revert before the final read (ABA).
// No conditional setter exists: this cannot exclude changes between the final read and write.
bool lovrMonadoReadRecenterOffset(MonadoRecenter* adapter, mnd_reference_space_type_t space, mnd_pose_t* offset);
bool lovrMonadoRecenterFromOffset(MonadoRecenter* adapter, mnd_reference_space_type_t space,
  const mnd_pose_t* expected, const float headPosition[3], const float headOrientation[4], bool tracked);

#endif
