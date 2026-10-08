#define _GNU_SOURCE
#include "test.h"
#include "headset/openvr_assets.h"
#include <dlfcn.h>
#include <stdlib.h>
#include <unistd.h>

static const char* names[] = {
  "actions.json", "bindings_vive_controller.json", "bindings_knuckles.json", "bindings_oculus_touch.json",
  "bindings_frame_controller.json"
};

static bool sameFile(const char* staged, const char* source) {
  FILE* a = fopen(staged, "rb");
  FILE* b = fopen(source, "rb");
  if (!a || !b) {
    if (a) fclose(a);
    if (b) fclose(b);
    return false;
  }
  bool equal = true;
  int x, y;
  do {
    x = fgetc(a);
    y = fgetc(b);
    if (x != y) equal = false;
  } while (equal && x != EOF && y != EOF);
  equal &= !ferror(a) && !ferror(b);
  int closeA = fclose(a);
  int closeB = fclose(b);
  return equal && !closeA && !closeB;
}

static bool checkResolved(OpenVRAssetsResult result) {
  if (result.status != OPENVR_ASSETS_OK_PATHS_NOT_PINNED) {
    fprintf(stderr, "asset resolver: status=%d path=%s errno=%d\n",
      result.status, result.failingPath, result.osError);
  }
  CHECK(result.status == OPENVR_ASSETS_OK_PATHS_NOT_PINNED);
  CHECK(result.osError == 0 && result.assetIndex == SIZE_MAX && !result.failingPath[0]);
  char directory[OPENVR_ASSET_PATH_CAPACITY];
  CHECK(realpath(STAGED_ASSET_DIRECTORY, directory) != NULL);
  char staged[OPENVR_ASSET_PATH_CAPACITY];
  char source[OPENVR_ASSET_PATH_CAPACITY];
  CHECK(sizeof(names) / sizeof(names[0]) == OPENVR_ASSET_COUNT);
  for (size_t i = 0; i < OPENVR_ASSET_COUNT; i++) {
    int length = snprintf(staged, sizeof(staged), "%s/%s", directory, names[i]);
    CHECK(length > 0 && (size_t) length < sizeof(staged));
    length = snprintf(source, sizeof(source), "%s/%s", SOURCE_ASSET_DIRECTORY, names[i]);
    CHECK(length > 0 && (size_t) length < sizeof(source));
    CHECK(sameFile(staged, source));
    if (i == 0) CHECK(!strcmp(result.manifest, staged));
  }
  return true;
}

#ifdef STAGED_SHARED
static bool resolveLibrary(const char* path, bool relative) {
  void* library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
  if (!library) fprintf(stderr, "dlopen: %s\n", dlerror());
  CHECK(library != NULL);
  OpenVRAssetsResult (*resolve)(void) = NULL;
  void* symbol = dlsym(library, "stagedOpenVRAssetsResolve");
  _Static_assert(sizeof(resolve) == sizeof(symbol), "Linux dlsym function pointer size");
  memcpy(&resolve, &symbol, sizeof(resolve));
  bool passed = resolve != NULL;
  if (!passed) fprintf(stderr, "dlsym: %s\n", dlerror());
  if (passed) passed = chdir(STAGED_UNRELATED_DIRECTORY) == 0;
  if (passed) {
    OpenVRAssetsResult result = resolve();
    if (relative) {
      passed = result.status == OPENVR_ASSETS_RELATIVE_ARTIFACT_UNSUPPORTED &&
        !strcmp(result.failingPath, path) && !result.manifest[0] && result.assetIndex == SIZE_MAX;
      if (!passed) fprintf(stderr, "relative asset resolver: status=%d path=%s\n", result.status, result.failingPath);
    } else {
      passed = checkResolved(result);
    }
  }
  int closed = dlclose(library);
  CHECK(passed && closed == 0);
  return true;
}

static bool stagedShared(void) {
  CHECK(STAGED_LIBRARY[0] == '/');
  CHECK(chdir(STAGED_UNRELATED_DIRECTORY) == 0);
  return resolveLibrary(STAGED_LIBRARY, false);
}

static bool stagedRelative(void) {
  CHECK(chdir(STAGED_LIBRARY_DIRECTORY) == 0);
  return resolveLibrary("./" STAGED_LIBRARY_NAME, true);
}
#else
static bool stagedStandalone(void) {
  CHECK(chdir(STAGED_UNRELATED_DIRECTORY) == 0);
  return checkResolved(lovrOpenVRAssetsResolve(false, NULL));
}
#endif

int main(int argc, char** argv) {
  const NativeTest tests[] = {
#ifdef STAGED_SHARED
    { "openvr.assets.staged.shared", stagedShared },
    { "openvr.assets.staged.relative-rejected", stagedRelative }
#else
    { "openvr.assets.staged.standalone", stagedStandalone }
#endif
  };
  return nativeRunTests(argc, argv, tests, sizeof(tests) / sizeof(tests[0]));
}
