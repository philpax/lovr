#include "test.h"
#include "headset/openvr_assets.h"
#include <errno.h>

static struct {
  const char* source;
  const char* canonical;
  const char* failurePath;
  OpenVRAssetsStatus artifactStatus, canonicalStatus, inspectStatus;
  OpenVRAssetMetadata metadata;
  unsigned calls, canonicalCalls;
  int osError;
  bool shared, truncate;
} fake;

static OpenVRAssetsStatus artifact(void* context, bool shared, char* path, size_t capacity, int* osError) {
  (void) context;
  *osError = fake.osError;
  fake.shared = shared;
  if (fake.truncate) memset(path, 'x', capacity);
  else snprintf(path, capacity, "%s", fake.source);
  return fake.artifactStatus;
}

static OpenVRAssetsStatus canonicalize(void* context, const char* path, char* canonical, size_t capacity, int* osError) {
  (void) context;
  *osError = fake.osError;
  fake.canonicalCalls++;
  if (strcmp(path, fake.source)) return OPENVR_ASSETS_CANONICALIZATION_FAILED;
  snprintf(canonical, capacity, "%s", fake.canonical);
  return fake.canonicalStatus;
}

static OpenVRAssetsStatus inspect(void* context, const char* path, OpenVRAssetMetadata* metadata, int* osError) {
  (void) context;
  *osError = fake.osError;
  fake.calls++;
  if (fake.failurePath && !strcmp(path, fake.failurePath)) {
    *metadata = fake.metadata;
    return fake.inspectStatus;
  }
  *metadata = (OpenVRAssetMetadata) {
    .kind = strstr(path, ".json") ? OPENVR_ASSET_REGULAR : OPENVR_ASSET_DIRECTORY,
    .permissions = 0755, .trustedOwner = true, .readable = true
  };
  return OPENVR_ASSETS_OK_PATHS_NOT_PINNED;
}

static const OpenVRAssetsProvider provider = { NULL, artifact, canonicalize, inspect };

static void reset(void) {
  memset(&fake, 0, sizeof(fake));
  fake.source = "/loaded/liblovr.so";
  fake.canonical = "/prefix/lib/liblovr.so.1";
  fake.metadata = (OpenVRAssetMetadata) { OPENVR_ASSET_REGULAR, 0644, true, true };
}

static bool installedResolution(void) {
  reset();
  OpenVRAssetsResult result = lovrOpenVRAssetsResolve(true, &provider);
  CHECK(result.status == OPENVR_ASSETS_OK_PATHS_NOT_PINNED && fake.shared);
  CHECK(!strcmp(result.manifest, "/prefix/lib/lovr-openvr/actions.json"));
  CHECK(result.assetIndex == SIZE_MAX && fake.calls == 8);
  CHECK(lovrOpenVRAssetName(OPENVR_ASSET_COUNT) == NULL);
  reset();
  fake.source = "/proc/self/exe";
  fake.canonical = "/prefix/bin/lovr";
  result = lovrOpenVRAssetsResolve(false, &provider);
  CHECK(result.status == OPENVR_ASSETS_OK_PATHS_NOT_PINNED && !fake.shared);
  CHECK(!strcmp(result.manifest, "/prefix/bin/lovr-openvr/actions.json"));
  return true;
}

static bool artifactFailures(void) {
  for (unsigned shared = 0; shared < 2; shared++) {
    reset();
    fake.artifactStatus = OPENVR_ASSETS_ARTIFACT_UNAVAILABLE;
    CHECK(lovrOpenVRAssetsResolve(shared, &provider).status == OPENVR_ASSETS_ARTIFACT_UNAVAILABLE);
    reset();
    fake.truncate = true;
    CHECK(lovrOpenVRAssetsResolve(shared, &provider).status == OPENVR_ASSETS_PATH_TOO_LONG);
    reset();
    fake.canonicalStatus = OPENVR_ASSETS_CANONICALIZATION_FAILED;
    CHECK(lovrOpenVRAssetsResolve(shared, &provider).status == OPENVR_ASSETS_CANONICALIZATION_FAILED);
    reset();
    fake.canonical = "relative/lovr";
    CHECK(lovrOpenVRAssetsResolve(shared, &provider).status == OPENVR_ASSETS_INVALID_PATH);
    reset();
    fake.canonical = "/prefix/../lovr";
    CHECK(lovrOpenVRAssetsResolve(shared, &provider).status == OPENVR_ASSETS_INVALID_PATH);
    fake.canonical = "/prefix/..";
    CHECK(lovrOpenVRAssetsResolve(shared, &provider).status == OPENVR_ASSETS_INVALID_PATH);
  }
  char longPath[OPENVR_ASSET_PATH_CAPACITY];
  memset(longPath, 'x', sizeof(longPath));
  longPath[0] = '/';
  longPath[sizeof(longPath) - 3] = '/';
  longPath[sizeof(longPath) - 1] = 0;
  reset();
  fake.canonical = longPath;
  CHECK(lovrOpenVRAssetsResolve(true, &provider).status == OPENVR_ASSETS_PATH_TOO_LONG);
  return true;
}

static bool fileFailures(void) {
  char path[256];
  for (size_t i = 0; i < OPENVR_ASSET_COUNT; i++) {
    reset();
    snprintf(path, sizeof(path), "/prefix/lib/lovr-openvr/%s", lovrOpenVRAssetName(i));
    fake.failurePath = path;
    fake.inspectStatus = OPENVR_ASSETS_MISSING;
    OpenVRAssetsResult result = lovrOpenVRAssetsResolve(true, &provider);
    CHECK(result.status == OPENVR_ASSETS_MISSING && result.assetIndex == i && !result.manifest[0]);
    fake.inspectStatus = OPENVR_ASSETS_OK_PATHS_NOT_PINNED;
    fake.metadata.kind = OPENVR_ASSET_LINK;
    CHECK(lovrOpenVRAssetsResolve(true, &provider).status == OPENVR_ASSETS_SYMLINK);
    fake.metadata.kind = OPENVR_ASSET_OTHER;
    CHECK(lovrOpenVRAssetsResolve(true, &provider).status == OPENVR_ASSETS_NOT_REGULAR);
    fake.metadata.kind = OPENVR_ASSET_REGULAR;
    fake.metadata.readable = false;
    CHECK(lovrOpenVRAssetsResolve(true, &provider).status == OPENVR_ASSETS_NOT_READABLE);
    fake.metadata.readable = true;
    fake.metadata.permissions = 0664;
    CHECK(lovrOpenVRAssetsResolve(true, &provider).status == OPENVR_ASSETS_UNSAFE_PERMISSIONS);
    fake.metadata.permissions = 0666;
    CHECK(lovrOpenVRAssetsResolve(true, &provider).status == OPENVR_ASSETS_UNSAFE_PERMISSIONS);
    fake.metadata.permissions = 04644;
    CHECK(lovrOpenVRAssetsResolve(true, &provider).status == OPENVR_ASSETS_UNSAFE_PERMISSIONS);
    fake.metadata.permissions = 0644;
    fake.metadata.trustedOwner = false;
    CHECK(lovrOpenVRAssetsResolve(true, &provider).status == OPENVR_ASSETS_UNSAFE_PERMISSIONS);
  }
  return true;
}

static bool directoryFailures(void) {
  const char* paths[] = { "/", "/prefix", "/prefix/lib", "/prefix/lib/lovr-openvr" };
  for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
    reset();
    fake.failurePath = paths[i];
    fake.metadata.kind = OPENVR_ASSET_LINK;
    CHECK(lovrOpenVRAssetsResolve(true, &provider).status == OPENVR_ASSETS_SYMLINK);
    fake.metadata.kind = OPENVR_ASSET_REGULAR;
    CHECK(lovrOpenVRAssetsResolve(true, &provider).status == OPENVR_ASSETS_NOT_DIRECTORY);
    fake.metadata.kind = OPENVR_ASSET_DIRECTORY;
    fake.metadata.permissions = 0777;
    CHECK(lovrOpenVRAssetsResolve(true, &provider).status == OPENVR_ASSETS_UNSAFE_PERMISSIONS);
  }
  return true;
}

static bool relativeArtifactRejected(void) {
  const char* destinations[] = { "/original/liblovr.so", "/changed/liblovr.so" };
  for (size_t i = 0; i < 2; i++) {
    reset();
    fake.source = "relative/liblovr.so";
    fake.canonical = destinations[i];
    OpenVRAssetsResult result = lovrOpenVRAssetsResolve(true, &provider);
    CHECK(result.status == OPENVR_ASSETS_RELATIVE_ARTIFACT_UNSUPPORTED);
    CHECK(fake.canonicalCalls == 0 && fake.calls == 0);
    CHECK(!strcmp(result.failingPath, fake.source));
  }
  return true;
}

static bool errorContext(void) {
  const int causes[] = { ENOENT, EACCES, EIO };
  const OpenVRAssetsStatus statuses[] = { OPENVR_ASSETS_MISSING, OPENVR_ASSETS_ACCESS_DENIED, OPENVR_ASSETS_IO_ERROR };
  for (size_t i = 0; i < 3; i++) {
    reset();
    fake.failurePath = "/prefix/lib/lovr-openvr/bindings_knuckles.json";
    fake.inspectStatus = statuses[i];
    fake.osError = causes[i];
    OpenVRAssetsResult result = lovrOpenVRAssetsResolve(true, &provider);
    CHECK(result.status == statuses[i] && result.osError == causes[i]);
    CHECK(result.assetIndex == 2 && !strcmp(result.failingPath, fake.failurePath));
    CHECK(!result.manifest[0]);
  }
  reset();
  fake.canonicalStatus = OPENVR_ASSETS_CANONICALIZATION_FAILED;
  fake.osError = EIO;
  OpenVRAssetsResult result = lovrOpenVRAssetsResolve(true, &provider);
  CHECK(result.osError == EIO && !strcmp(result.failingPath, fake.source));
  return true;
}

int main(int argc, char** argv) {
  const NativeTest tests[] = {
    { "installed_action_asset_resolution", installedResolution },
    { "artifact_path_failures", artifactFailures },
    { "asset_file_failures", fileFailures },
    { "asset_directory_failures", directoryFailures },
    { "relative_artifact_rejected_after_cwd_change", relativeArtifactRejected },
    { "asset_error_context", errorContext }
  };
  return nativeRunTests(argc, argv, tests, sizeof(tests) / sizeof(tests[0]));
}
