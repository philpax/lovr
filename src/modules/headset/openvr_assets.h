#ifndef LOVR_OPENVR_ASSETS_H
#define LOVR_OPENVR_ASSETS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OPENVR_ASSET_PATH_CAPACITY 4096
#define OPENVR_ASSET_COUNT 4

typedef enum {
  OPENVR_ASSETS_OK_PATHS_NOT_PINNED,
  OPENVR_ASSETS_ARTIFACT_UNAVAILABLE,
  OPENVR_ASSETS_RELATIVE_ARTIFACT_UNSUPPORTED,
  OPENVR_ASSETS_ACCESS_DENIED,
  OPENVR_ASSETS_IO_ERROR,
  OPENVR_ASSETS_PATH_TOO_LONG,
  OPENVR_ASSETS_CANONICALIZATION_FAILED,
  OPENVR_ASSETS_INVALID_PATH,
  OPENVR_ASSETS_MISSING,
  OPENVR_ASSETS_SYMLINK,
  OPENVR_ASSETS_UNSAFE_PERMISSIONS,
  OPENVR_ASSETS_NOT_DIRECTORY,
  OPENVR_ASSETS_NOT_REGULAR,
  OPENVR_ASSETS_NOT_READABLE
} OpenVRAssetsStatus;

typedef enum {
  OPENVR_ASSET_DIRECTORY,
  OPENVR_ASSET_REGULAR,
  OPENVR_ASSET_LINK,
  OPENVR_ASSET_OTHER
} OpenVRAssetKind;

typedef struct {
  OpenVRAssetKind kind;
  uint32_t permissions;
  bool trustedOwner;
  bool readable;
} OpenVRAssetMetadata;

typedef struct {
  void* context;
  OpenVRAssetsStatus (*artifact)(void* context, bool shared, char* path, size_t capacity, int* osError);
  OpenVRAssetsStatus (*canonicalize)(void* context, const char* path, char* canonical, size_t capacity, int* osError);
  OpenVRAssetsStatus (*inspect)(void* context, const char* path, OpenVRAssetMetadata* metadata, int* osError);
} OpenVRAssetsProvider;

typedef struct {
  OpenVRAssetsStatus status;
  size_t assetIndex;
  int osError;
  char failingPath[OPENVR_ASSET_PATH_CAPACITY];
  char manifest[OPENVR_ASSET_PATH_CAPACITY];
} OpenVRAssetsResult;

const char* lovrOpenVRAssetName(size_t index);
OpenVRAssetsResult lovrOpenVRAssetsResolve(bool shared, const OpenVRAssetsProvider* provider);

#endif
