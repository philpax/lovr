#define _GNU_SOURCE
#include "openvr_assets.h"
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char artifactAnchor = 0;
static const char* assets[OPENVR_ASSET_COUNT] = {
  "actions.json", "bindings_vive_controller.json", "bindings_knuckles.json", "bindings_oculus_touch.json"
};

const char* lovrOpenVRAssetName(size_t index) {
  return index < OPENVR_ASSET_COUNT ? assets[index] : NULL;
}

static OpenVRAssetsStatus artifact(void* context, bool shared, char* path, size_t capacity, int* osError) {
  (void) context;
  *osError = 0;
  if (shared) {
    Dl_info info;
    if (!dladdr(&artifactAnchor, &info) || !info.dli_fname) return OPENVR_ASSETS_ARTIFACT_UNAVAILABLE;
    size_t length = strlen(info.dli_fname);
    if (length >= capacity) return OPENVR_ASSETS_PATH_TOO_LONG;
    memcpy(path, info.dli_fname, length + 1);
  } else {
    ssize_t length = readlink("/proc/self/exe", path, capacity);
    if (length < 0) { *osError = errno; return OPENVR_ASSETS_ARTIFACT_UNAVAILABLE; }
    if ((size_t) length >= capacity) return OPENVR_ASSETS_PATH_TOO_LONG;
    path[length] = '\0';
  }
  return OPENVR_ASSETS_OK_PATHS_NOT_PINNED;
}

static OpenVRAssetsStatus canonicalize(void* context, const char* path, char* canonical, size_t capacity, int* osError) {
  (void) context;
  *osError = 0;
  if (capacity < OPENVR_ASSET_PATH_CAPACITY) return OPENVR_ASSETS_PATH_TOO_LONG;
  if (!realpath(path, canonical)) {
    *osError = errno;
    return errno == ENAMETOOLONG ? OPENVR_ASSETS_PATH_TOO_LONG : OPENVR_ASSETS_CANONICALIZATION_FAILED;
  }
  return OPENVR_ASSETS_OK_PATHS_NOT_PINNED;
}

static OpenVRAssetsStatus inspect(void* context, const char* path, OpenVRAssetMetadata* metadata, int* osError) {
  (void) context;
  *osError = 0;
  struct stat st;
  if (lstat(path, &st)) {
    *osError = errno;
    return errno == ENOENT || errno == ENOTDIR ? OPENVR_ASSETS_MISSING :
      errno == EACCES || errno == EPERM ? OPENVR_ASSETS_ACCESS_DENIED : OPENVR_ASSETS_IO_ERROR;
  }
  metadata->kind = S_ISLNK(st.st_mode) ? OPENVR_ASSET_LINK : S_ISDIR(st.st_mode) ? OPENVR_ASSET_DIRECTORY :
    S_ISREG(st.st_mode) ? OPENVR_ASSET_REGULAR : OPENVR_ASSET_OTHER;
  metadata->permissions = st.st_mode & 07777;
  metadata->trustedOwner = st.st_uid == 0 || st.st_uid == geteuid();
  metadata->readable = false;
  if (metadata->kind == OPENVR_ASSET_REGULAR) {
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0) {
      *osError = errno;
      return errno == EACCES || errno == EPERM ? OPENVR_ASSETS_ACCESS_DENIED :
        errno == ENOENT || errno == ENOTDIR ? OPENVR_ASSETS_MISSING :
        errno == ELOOP ? OPENVR_ASSETS_SYMLINK : OPENVR_ASSETS_IO_ERROR;
    }
    struct stat opened;
    if (fstat(fd, &opened)) {
      *osError = errno;
      close(fd);
      return OPENVR_ASSETS_IO_ERROR;
    }
    metadata->readable = S_ISREG(opened.st_mode) && opened.st_dev == st.st_dev && opened.st_ino == st.st_ino &&
      opened.st_mode == st.st_mode && opened.st_uid == st.st_uid;
    if (close(fd)) { *osError = errno; return OPENVR_ASSETS_IO_ERROR; }
  }
  return OPENVR_ASSETS_OK_PATHS_NOT_PINNED;
}

static OpenVRAssetsStatus validate(const OpenVRAssetsProvider* provider, const char* path, bool directory, OpenVRAssetsResult* result) {
  OpenVRAssetMetadata metadata;
  strcpy(result->failingPath, path);
  result->manifest[0] = 0;
  result->osError = 0;
  OpenVRAssetsStatus status = provider->inspect(provider->context, path, &metadata, &result->osError);
  if (status != OPENVR_ASSETS_OK_PATHS_NOT_PINNED) return status;
  if (metadata.kind == OPENVR_ASSET_LINK) return OPENVR_ASSETS_SYMLINK;
  if (!metadata.trustedOwner || (metadata.permissions & 07022)) return OPENVR_ASSETS_UNSAFE_PERMISSIONS;
  if (directory) return metadata.kind == OPENVR_ASSET_DIRECTORY ? status : OPENVR_ASSETS_NOT_DIRECTORY;
  if (metadata.kind != OPENVR_ASSET_REGULAR) return OPENVR_ASSETS_NOT_REGULAR;
  return metadata.readable ? status : OPENVR_ASSETS_NOT_READABLE;
}

OpenVRAssetsResult lovrOpenVRAssetsResolve(bool shared, const OpenVRAssetsProvider* provider) {
  const OpenVRAssetsProvider native = { NULL, artifact, canonicalize, inspect };
  if (!provider) provider = &native;
  OpenVRAssetsResult result = { .status = OPENVR_ASSETS_INVALID_PATH, .assetIndex = SIZE_MAX };
  if (!provider->artifact || !provider->canonicalize || !provider->inspect) return result;
  char source[OPENVR_ASSET_PATH_CAPACITY] = { 0 };
  char path[OPENVR_ASSET_PATH_CAPACITY] = { 0 };
  strcpy(result.failingPath, shared ? "loaded LÖVR artifact" : "/proc/self/exe");
  result.status = provider->artifact(provider->context, shared, source, sizeof(source), &result.osError);
  if (result.status != OPENVR_ASSETS_OK_PATHS_NOT_PINNED) return result;
  if (!memchr(source, 0, sizeof(source))) { result.status = OPENVR_ASSETS_PATH_TOO_LONG; return result; }
  strcpy(result.failingPath, source);
  if (source[0] != '/') {
    result.status = OPENVR_ASSETS_RELATIVE_ARTIFACT_UNSUPPORTED;
    return result;
  }
  result.status = provider->canonicalize(provider->context, source, path, sizeof(path), &result.osError);
  if (result.status != OPENVR_ASSETS_OK_PATHS_NOT_PINNED) return result;
  if (!memchr(path, 0, sizeof(path))) { result.status = OPENVR_ASSETS_PATH_TOO_LONG; return result; }
  if (path[0] != '/' || strstr(path, "/../") || strstr(path, "/./") || strstr(path, "//")) {
    result.status = OPENVR_ASSETS_INVALID_PATH;
    return result;
  }
  char* slash = strrchr(path, '/');
  if (!slash || !slash[1] || !strcmp(slash + 1, ".") || !strcmp(slash + 1, "..")) {
    result.status = OPENVR_ASSETS_INVALID_PATH;
    return result;
  }
  slash[1] = 0;
  size_t rootLength = strlen(path);
  const char suffix[] = "lovr-openvr/";
  size_t longest = 0;
  for (size_t i = 0; i < OPENVR_ASSET_COUNT; i++) {
    size_t length = strlen(assets[i]);
    if (length > longest) longest = length;
  }
  if (rootLength + sizeof(suffix) + longest > sizeof(path)) {
    result.status = OPENVR_ASSETS_PATH_TOO_LONG;
    return result;
  }
  memcpy(path + rootLength, suffix, sizeof(suffix));
  size_t baseLength = strlen(path);
  for (size_t i = 1; i < baseLength; i++) {
    if (path[i] != '/') continue;
    path[i] = 0;
    result.status = validate(provider, path, true, &result);
    path[i] = '/';
    if (result.status != OPENVR_ASSETS_OK_PATHS_NOT_PINNED) return result;
  }
  result.status = validate(provider, "/", true, &result);
  if (result.status != OPENVR_ASSETS_OK_PATHS_NOT_PINNED) return result;
  for (size_t i = 0; i < OPENVR_ASSET_COUNT; i++) {
    result.assetIndex = i;
    strcpy(path + baseLength, assets[i]);
    result.status = validate(provider, path, false, &result);
    if (result.status != OPENVR_ASSETS_OK_PATHS_NOT_PINNED) { result.manifest[0] = 0; return result; }

  }
  strcpy(path + baseLength, assets[0]);
  strcpy(result.manifest, path);
  result.failingPath[0] = 0;
  result.osError = 0;
  result.assetIndex = SIZE_MAX;
  return result;
}
