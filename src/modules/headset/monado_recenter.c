// Linux-only: optional libmonado recentering through an explicit application-provided library.
#include "headset/monado_recenter.h"
#include "util.h"
#include <dlfcn.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static void* openLibrary(void* context, const char* path);
static void* loadSymbol(void* context, void* library, const char* name);
static void closeLibrary(void* context, void* library);
static HeadsetRecenterStatus connect(MonadoRecenter* adapter);
static HeadsetRecenterStatus inspect(MonadoRecenter* adapter, mnd_reference_space_type_t space,
  const float position[3], const float orientation[4], bool tracked, mnd_pose_t* offset, float* yaw);
static bool validPose(const float position[3], const float orientation[4]);
static bool horizontalYaw(const float orientation[4], float* yaw);
static bool applyOffset(MonadoRecenter* adapter, mnd_reference_space_type_t space,
  const mnd_pose_t* offset, const float position[3], float yaw);
static bool equalPose(const mnd_pose_t* a, const mnd_pose_t* b);

bool lovrMonadoRecenterInit(MonadoRecenter* adapter, const char* path) {
  const MonadoRecenterLoader loader = { NULL, openLibrary, loadSymbol, closeLibrary };
  return lovrMonadoRecenterInitWithLoader(adapter, path, &loader);
}

bool lovrMonadoRecenterInitWithLoader(MonadoRecenter* adapter, const char* path, const MonadoRecenterLoader* loader) {
  if (!adapter) return lovrSetError("monado: recenter: missing adapter");
  memset(adapter, 0, sizeof(*adapter));
  if (!loader || !loader->open || !loader->symbol || !loader->close) {
    return lovrSetError("monado: recenter: incomplete loader");
  }
  adapter->loader = *loader;
  if (!path || !path[0]) return true;
  // A bare soname would enable the dynamic linker's system search path.
  if (path[0] != '/') return lovrSetError("monado: recenter: library path must be absolute");
  size_t length = strlen(path) + 1;
  adapter->path = malloc(length);
  if (!adapter->path) return lovrSetError("monado: recenter: library path allocation failed");
  memcpy(adapter->path, path, length);
  return true;
}

void lovrMonadoRecenterDestroy(MonadoRecenter* adapter) {
  if (!adapter) return;
  if (adapter->root) adapter->destroy(&adapter->root);
  if (adapter->library) adapter->loader.close(adapter->loader.context, adapter->library);
  free(adapter->path);
  memset(adapter, 0, sizeof(*adapter));
}

HeadsetRecenterStatus lovrMonadoGetRecenterStatus(MonadoRecenter* adapter, mnd_reference_space_type_t space,
  const float position[3], const float orientation[4], bool tracked) {
  mnd_pose_t offset;
  float yaw;
  return inspect(adapter, space, position, orientation, tracked, &offset, &yaw);
}

bool lovrMonadoRecenter(MonadoRecenter* adapter, mnd_reference_space_type_t space,
  const float position[3], const float orientation[4], bool tracked) {
  if (adapter) adapter->writeAttempted = false;
  mnd_pose_t offset;
  float yaw;
  if (inspect(adapter, space, position, orientation, tracked, &offset, &yaw) != HEADSET_RECENTER_AVAILABLE) return false;
  return applyOffset(adapter, space, &offset, position, yaw);
}

bool lovrMonadoReadRecenterOffset(MonadoRecenter* adapter, mnd_reference_space_type_t space, mnd_pose_t* offset) {
  if (!offset) return lovrSetError("monado: recenter: missing offset output");
  if (space != MND_SPACE_REFERENCE_TYPE_LOCAL && space != MND_SPACE_REFERENCE_TYPE_LOCAL_FLOOR &&
      space != MND_SPACE_REFERENCE_TYPE_STAGE) return lovrSetError("monado: recenter: unsupported reference space %d", space);
  if (connect(adapter) != HEADSET_RECENTER_AVAILABLE) return false;
  memset(offset, 0, sizeof(*offset));
  mnd_result_t result = adapter->getOffset(adapter->root, space, offset);
  if (result < 0) {
    if (result == MND_ERROR_OPERATION_FAILED || result == MND_ERROR_CONNECTING_FAILED) adapter->destroy(&adapter->root);
    return lovrSetError("monado: recenter: get offset for space %d returned %d", space, result);
  }
  float p[3] = { offset->position.x, offset->position.y, offset->position.z };
  float q[4] = { offset->orientation.x, offset->orientation.y, offset->orientation.z, offset->orientation.w };
  if (!validPose(p, q) || fabsf(q[0]) > 1e-4f || fabsf(q[2]) > 1e-4f) {
    return lovrSetError("monado: recenter: reference space %d offset is invalid or not level", space);
  }
  return true;
}

bool lovrMonadoRecenterFromOffset(MonadoRecenter* adapter, mnd_reference_space_type_t space,
  const mnd_pose_t* expected, const float position[3], const float orientation[4], bool tracked) {
  if (adapter) adapter->writeAttempted = false;
  if (!expected) return lovrSetError("monado: recenter: missing expected offset");
  mnd_pose_t offset;
  float yaw;
  if (inspect(adapter, space, position, orientation, tracked, &offset, &yaw) != HEADSET_RECENTER_AVAILABLE) return false;
  if (!equalPose(expected, &offset)) return lovrSetError("monado: recenter: reference space %d offset changed during head sampling", space);
  return applyOffset(adapter, space, &offset, position, yaw);
}

static bool applyOffset(MonadoRecenter* adapter, mnd_reference_space_type_t space,
  const mnd_pose_t* previous, const float position[3], float yaw) {
  mnd_pose_t offset = *previous;
  float oldOrientation[4] = { offset.orientation.x, offset.orientation.y, offset.orientation.z, offset.orientation.w };
  float oldYaw;
  if (!horizontalYaw(oldOrientation, &oldYaw)) return lovrSetError("monado: recenter: offset yaw is singular");
  double c = cos((double) oldYaw), s = sin((double) oldYaw);
  double x = (double) offset.position.x + c * position[0] + s * position[2];
  double z = (double) offset.position.z - s * position[0] + c * position[2];
  if (!isfinite(x) || !isfinite(z) || fabs(x) > FLT_MAX || fabs(z) > FLT_MAX) {
    return lovrSetError("monado: recenter: composed offset is outside the float range");
  }
  mnd_pose_t composed = {
    .orientation = { 0.f, sinf((oldYaw + yaw) * .5f), 0.f, cosf((oldYaw + yaw) * .5f) },
    .position = { (float) x, offset.position.y, (float) z }
  };
  float p[3] = { composed.position.x, composed.position.y, composed.position.z };
  float q[4] = { composed.orientation.x, composed.orientation.y, composed.orientation.z, composed.orientation.w };
  if (!validPose(p, q)) return lovrSetError("monado: recenter: composed offset is invalid");
  // Only the requested space is written, even when the runtime rejects LOCAL_FLOOR.
  adapter->writeAttempted = true;
  mnd_result_t result = adapter->setOffset(adapter->root, space, &composed);
  if (result < 0) {
    if (result == MND_ERROR_OPERATION_FAILED || result == MND_ERROR_CONNECTING_FAILED) adapter->destroy(&adapter->root);
    return lovrSetError("monado: recenter: set offset for space %d returned %d", space, result);
  }
  return true;
}

static void* openLibrary(void* context, const char* path) {
  (void) context;
  return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

static void* loadSymbol(void* context, void* library, const char* name) {
  (void) context;
  return dlsym(library, name);
}

static void closeLibrary(void* context, void* library) {
  (void) context;
  dlclose(library);
}

static HeadsetRecenterStatus connect(MonadoRecenter* adapter) {
  if (!adapter || !adapter->path || adapter->unsupported) {
    lovrSetError("monado: recenter: adapter is unsupported or no library path is configured");
    return HEADSET_RECENTER_UNSUPPORTED;
  }
  if (!adapter->library) {
    adapter->library = adapter->loader.open(adapter->loader.context, adapter->path);
    if (!adapter->library) {
      lovrSetError("monado: recenter: could not load library at %s", adapter->path);
      return HEADSET_RECENTER_UNAVAILABLE;
    }
    const char* names[] = { "mnd_api_get_version", "mnd_root_create", "mnd_root_destroy",
      "mnd_root_get_reference_space_offset", "mnd_root_set_reference_space_offset" };
    void* symbols[5];
    for (unsigned i = 0; i < 5; i++) {
      symbols[i] = adapter->loader.symbol(adapter->loader.context, adapter->library, names[i]);
      if (!symbols[i]) {
        lovrSetError("monado: recenter: library at %s is missing symbol %s", adapter->path, names[i]);
        adapter->unsupported = true;
        adapter->loader.close(adapter->loader.context, adapter->library);
        adapter->library = NULL;
        return HEADSET_RECENTER_UNSUPPORTED;
      }
    }
    adapter->getVersion = (void (*)(uint32_t*, uint32_t*, uint32_t*)) symbols[0];
    adapter->create = (mnd_result_t (*)(mnd_root_t**)) symbols[1];
    adapter->destroy = (void (*)(mnd_root_t**)) symbols[2];
    adapter->getOffset = (mnd_result_t (*)(mnd_root_t*, mnd_reference_space_type_t, mnd_pose_t*)) symbols[3];
    adapter->setOffset = (mnd_result_t (*)(mnd_root_t*, mnd_reference_space_type_t, const mnd_pose_t*)) symbols[4];
    uint32_t major, minor, patch;
    adapter->getVersion(&major, &minor, &patch);
    if (major != MND_API_VERSION_MAJOR || minor < 3) {
      lovrSetError("monado: recenter: incompatible library API %u.%u.%u (requires 1.3 or newer 1.x)", major, minor, patch);
      adapter->unsupported = true;
      adapter->loader.close(adapter->loader.context, adapter->library);
      adapter->library = NULL;
      return HEADSET_RECENTER_UNSUPPORTED;
    }
  }
  if (!adapter->root) {
    mnd_result_t result = adapter->create(&adapter->root);
    if (result < 0 || !adapter->root) {
      if (adapter->root) adapter->destroy(&adapter->root);
      lovrSetError("monado: recenter: root connection returned %d%s", result, result >= 0 ? " without a root" : "");
      return HEADSET_RECENTER_UNAVAILABLE;
    }
  }
  return HEADSET_RECENTER_AVAILABLE;
}

static HeadsetRecenterStatus inspect(MonadoRecenter* adapter, mnd_reference_space_type_t space,
  const float position[3], const float orientation[4], bool tracked, mnd_pose_t* offset, float* yaw) {
  if (space != MND_SPACE_REFERENCE_TYPE_LOCAL && space != MND_SPACE_REFERENCE_TYPE_LOCAL_FLOOR &&
      space != MND_SPACE_REFERENCE_TYPE_STAGE) {
    lovrSetError("monado: recenter: unsupported reference space %d", space);
    return HEADSET_RECENTER_UNSUPPORTED;
  }
  if (space == MND_SPACE_REFERENCE_TYPE_LOCAL_FLOOR) {
    lovrSetError("monado: recenter: LOCAL_FLOOR setter is unsupported; select LOCAL with a LOCAL-relative head pose");
    return HEADSET_RECENTER_UNSUPPORTED;
  }
  HeadsetRecenterStatus status = connect(adapter);
  if (status != HEADSET_RECENTER_AVAILABLE) return status;
  if (!tracked || !validPose(position, orientation) || !horizontalYaw(orientation, yaw)) {
    lovrSetError("monado: recenter: head pose is untracked, invalid, or has singular yaw");
    return HEADSET_RECENTER_UNTRACKED;
  }
  if (!lovrMonadoReadRecenterOffset(adapter, space, offset)) return HEADSET_RECENTER_UNAVAILABLE;
  return HEADSET_RECENTER_AVAILABLE;
}

static bool equalPose(const mnd_pose_t* a, const mnd_pose_t* b) {
  return a->position.x == b->position.x && a->position.y == b->position.y && a->position.z == b->position.z &&
    a->orientation.x == b->orientation.x && a->orientation.y == b->orientation.y &&
    a->orientation.z == b->orientation.z && a->orientation.w == b->orientation.w;
}

static bool validPose(const float position[3], const float orientation[4]) {
  if (!position || !orientation) return false;
  for (unsigned i = 0; i < 3; i++) if (!isfinite(position[i])) return false;
  double norm = 0.;
  for (unsigned i = 0; i < 4; i++) {
    if (!isfinite(orientation[i])) return false;
    norm += (double) orientation[i] * orientation[i];
  }
  return fabs(norm - 1.) <= 1e-3;
}

static bool horizontalYaw(const float orientation[4], float* yaw) {
  float x = orientation[0], y = orientation[1], z = orientation[2], w = orientation[3];
  float norm = x * x + y * y + z * z + w * w;
  float forwardX = 2.f * (x * z + w * y) / norm;
  float forwardZ = 1.f - 2.f * (x * x + y * y) / norm;
  if (hypotf(forwardX, forwardZ) < 1e-4f) return false;
  *yaw = atan2f(forwardX, forwardZ);
  return true;
}
