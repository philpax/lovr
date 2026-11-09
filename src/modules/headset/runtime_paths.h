#ifndef LOVR_HEADSET_RUNTIME_PATHS_H
#define LOVR_HEADSET_RUNTIME_PATHS_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
  const char* manifest;
  const char* library;
  const void* snapshot;
  size_t snapshotLength;
  bool monadoStageUnmapped;
} HeadsetRuntimePaths;

// Sequential reads return false on error and zero bytes at EOF.
typedef bool (*HeadsetRuntimeManifestReader)(void* context, void* buffer, size_t capacity, size_t* length);

bool lovrHeadsetSetRuntimePaths(const char* manifest, const char* library);
bool lovrHeadsetSetRuntimeManifestSnapshot(const void* bytes, size_t length);
bool lovrHeadsetSetMonadoStageUnmapped(bool unmapped);
bool lovrHeadsetValidateRuntimeManifest(void);
bool lovrHeadsetValidateRuntimeManifestWithReader(HeadsetRuntimeManifestReader reader, void* context);
// Borrowed paths remain valid until the next successful configuration change.
HeadsetRuntimePaths lovrHeadsetGetRuntimePaths(void);
void lovrHeadsetRuntimePathsLock(bool inUse);

#endif
