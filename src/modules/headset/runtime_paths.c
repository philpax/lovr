#include "headset/runtime_paths.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

static struct {
  char* manifest;
  char* library;
  unsigned char* snapshot;
  size_t snapshotLength;
  bool inUse;
  bool monadoStageUnmapped;
} paths;

bool lovrHeadsetSetRuntimePaths(const char* manifest, const char* library) {
  lovrAssert(!paths.inUse, "openxr: runtime paths: an instance is already connected");
  lovrAssert((manifest && library) || (!manifest && !library), "openxr: runtime paths: manifest and library are required together");
  lovrAssert(!manifest || (manifest[0] == '/' && library[0] == '/'), "openxr: runtime paths: paths must be absolute");
  char* manifestCopy = NULL;
  char* libraryCopy = NULL;
  if (manifest) {
    manifestCopy = lovrMalloc(strlen(manifest) + 1);
    libraryCopy = lovrMalloc(strlen(library) + 1);
    strcpy(manifestCopy, manifest);
    strcpy(libraryCopy, library);
  }
  lovrFree(paths.manifest);
  lovrFree(paths.library);
  paths.manifest = manifestCopy;
  paths.library = libraryCopy;
  lovrFree(paths.snapshot);
  paths.snapshot = NULL;
  paths.snapshotLength = 0;
  paths.monadoStageUnmapped = false;
  return true;
}

bool lovrHeadsetSetRuntimeManifestSnapshot(const void* bytes, size_t length) {
  lovrAssert(!paths.inUse, "openxr: runtime paths: an instance is already connected");
  lovrAssert(bytes || !length, "openxr: runtime paths: snapshot bytes are required");
  lovrAssert(!bytes || paths.manifest, "openxr: runtime paths: a manifest is required for a snapshot");
  unsigned char* copy = NULL;
  if (bytes) {
    copy = lovrMalloc(length ? length : 1);
    if (length) memcpy(copy, bytes, length);
  }
  lovrFree(paths.snapshot);
  paths.snapshot = copy;
  paths.snapshotLength = length;
  return true;
}

bool lovrHeadsetSetMonadoStageUnmapped(bool unmapped) {
  lovrAssert(!paths.inUse, "openxr: runtime paths: an instance is already connected");
  paths.monadoStageUnmapped = unmapped;
  return true;
}

static bool readManifest(void* context, void* buffer, size_t capacity, size_t* length) {
  FILE* file = context;
  *length = fread(buffer, 1, capacity, file);
  return !ferror(file);
}

bool lovrHeadsetValidateRuntimeManifest(void) {
  lovrAssert(paths.manifest && paths.snapshot, "openxr: runtime paths: a manifest snapshot is required");
  FILE* file = fopen(paths.manifest, "rb");
  lovrAssert(file, "openxr: runtime paths: could not open manifest %s", paths.manifest);
  bool matches = lovrHeadsetValidateRuntimeManifestWithReader(readManifest, file);
  int closed = fclose(file);
  lovrAssert(closed == 0, "openxr: runtime paths: could not close manifest %s", paths.manifest);
  return matches;
}

bool lovrHeadsetValidateRuntimeManifestWithReader(HeadsetRuntimeManifestReader reader, void* context) {
  lovrAssert(paths.manifest && paths.snapshot, "openxr: runtime paths: a manifest snapshot is required");
  lovrAssert(reader, "openxr: runtime paths: a manifest reader is required");
  unsigned char buffer[4096];
  size_t offset = 0;
  for (;;) {
    size_t length = 0;
    lovrAssert(reader(context, buffer, sizeof(buffer), &length), "openxr: runtime paths: could not read manifest %s", paths.manifest);
    lovrAssert(length <= sizeof(buffer), "openxr: runtime paths: manifest reader exceeded its buffer");
    if (!length) {
      lovrAssert(offset == paths.snapshotLength, "openxr: runtime paths: manifest differs from its snapshot");
      return true;
    }
    lovrAssert(length <= paths.snapshotLength - offset, "openxr: runtime paths: manifest differs from its snapshot");
    lovrAssert(!memcmp(buffer, paths.snapshot + offset, length), "openxr: runtime paths: manifest differs from its snapshot");
    offset += length;
  }
}

HeadsetRuntimePaths lovrHeadsetGetRuntimePaths(void) {
  return (HeadsetRuntimePaths) { paths.manifest, paths.library, paths.snapshot, paths.snapshotLength, paths.monadoStageUnmapped };
}

void lovrHeadsetRuntimePathsLock(bool inUse) {
  paths.inUse = inUse;
}
