#define _GNU_SOURCE
#include "openvr_assets.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#else
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static const char artifactAnchor = 0;
static const char* assets[OPENVR_ASSET_COUNT] = {
  "actions.json", "bindings_vive_controller.json", "bindings_knuckles.json", "bindings_oculus_touch.json",
  "bindings_frame_controller.json", "bindings_vd_hand_controller.json"
};

const char* lovrOpenVRAssetName(size_t index) {
  return index < OPENVR_ASSET_COUNT ? assets[index] : NULL;
}

#ifdef _WIN32
static void pathSlashes(char* path, char from, char to) {
  for (; *path; path++) {
    if (*path == from) *path = to;
  }
}

static OpenVRAssetsStatus fromWide(const wchar_t* wide, char* path, size_t capacity, int* osError) {
  if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, path, (int) capacity, NULL, NULL)) {
    *osError = (int) GetLastError();
    return *osError == ERROR_INSUFFICIENT_BUFFER ? OPENVR_ASSETS_PATH_TOO_LONG : OPENVR_ASSETS_INVALID_PATH;
  }
  pathSlashes(path, '\\', '/');
  return OPENVR_ASSETS_OK_PATHS_NOT_PINNED;
}

static OpenVRAssetsStatus artifact(void* context, bool shared, char* path, size_t capacity, int* osError) {
  (void) context;
  HMODULE module = NULL;
  if (shared && !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
    (LPCWSTR) &artifactAnchor, &module)) {
    *osError = (int) GetLastError();
    return OPENVR_ASSETS_ARTIFACT_UNAVAILABLE;
  }
  wchar_t wide[OPENVR_ASSET_PATH_CAPACITY];
  DWORD length = GetModuleFileNameW(module, wide, OPENVR_ASSET_PATH_CAPACITY);
  if (!length) {
    *osError = (int) GetLastError();
    return OPENVR_ASSETS_ARTIFACT_UNAVAILABLE;
  }
  if (length >= OPENVR_ASSET_PATH_CAPACITY) return OPENVR_ASSETS_PATH_TOO_LONG;
  return fromWide(wide, path, capacity, osError);
}

static OpenVRAssetsStatus canonicalize(void* context, const char* path, char* canonical, size_t capacity, int* osError) {
  (void) context;
  wchar_t wide[OPENVR_ASSET_PATH_CAPACITY], full[OPENVR_ASSET_PATH_CAPACITY];
  if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, OPENVR_ASSET_PATH_CAPACITY)) {
    *osError = (int) GetLastError();
    return OPENVR_ASSETS_INVALID_PATH;
  }
  // Resolving junctions here would let an unsafe ancestor disappear from the checks.
  DWORD length = GetFullPathNameW(wide, OPENVR_ASSET_PATH_CAPACITY, full, NULL);
  if (!length) {
    *osError = (int) GetLastError();
    return OPENVR_ASSETS_CANONICALIZATION_FAILED;
  }
  if (length >= OPENVR_ASSET_PATH_CAPACITY) return OPENVR_ASSETS_PATH_TOO_LONG;
  return fromWide(full, canonical, capacity, osError);
}

static bool trustedSid(PSID sid, PSID user, PSID installer) {
  return sid && (EqualSid(sid, user) || EqualSid(sid, installer) ||
    IsWellKnownSid(sid, WinLocalSystemSid) || IsWellKnownSid(sid, WinBuiltinAdministratorsSid));
}

static bool trustedAcl(PACL acl, PSID user, PSID installer, bool directory) {
  if (!acl) return false;
  // Adding an unrelated child is harmless. Replacing existing entries requires DELETE or FILE_DELETE_CHILD.
  DWORD unsafe = DELETE | WRITE_DAC | WRITE_OWNER | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA |
    (directory ? FILE_DELETE_CHILD : FILE_WRITE_DATA | FILE_APPEND_DATA);
  GENERIC_MAPPING mapping = { FILE_GENERIC_READ, FILE_GENERIC_WRITE, FILE_GENERIC_EXECUTE, FILE_ALL_ACCESS };
  for (DWORD i = 0; i < acl->AceCount; i++) {
    void* entry;
    if (!GetAce(acl, i, &entry)) return false;
    const ACE_HEADER* header = entry;
    if (header->AceFlags & INHERIT_ONLY_ACE) continue;
    if (header->AceType == ACCESS_DENIED_ACE_TYPE || header->AceType == ACCESS_DENIED_OBJECT_ACE_TYPE ||
        header->AceType == ACCESS_DENIED_CALLBACK_ACE_TYPE || header->AceType == ACCESS_DENIED_CALLBACK_OBJECT_ACE_TYPE) continue;
    if (header->AceType != ACCESS_ALLOWED_ACE_TYPE) return false;
    ACCESS_ALLOWED_ACE* allow = entry;
    DWORD mask = allow->Mask;
    MapGenericMask(&mask, &mapping);
    if ((mask & unsafe) && !trustedSid(&allow->SidStart, user, installer)) return false;
  }
  return true;
}

static OpenVRAssetsStatus inspect(void* context, const char* path, OpenVRAssetMetadata* metadata, int* osError) {
  (void) context;
  wchar_t wide[OPENVR_ASSET_PATH_CAPACITY];
  if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, OPENVR_ASSET_PATH_CAPACITY)) {
    *osError = (int) GetLastError();
    return OPENVR_ASSETS_INVALID_PATH;
  }
  HANDLE file = CreateFileW(wide, READ_CONTROL | FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
    NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (file == INVALID_HANDLE_VALUE) {
    *osError = (int) GetLastError();
    return *osError == ERROR_FILE_NOT_FOUND || *osError == ERROR_PATH_NOT_FOUND ? OPENVR_ASSETS_MISSING :
      *osError == ERROR_ACCESS_DENIED ? OPENVR_ASSETS_ACCESS_DENIED : OPENVR_ASSETS_IO_ERROR;
  }

  OpenVRAssetsStatus status = OPENVR_ASSETS_IO_ERROR;
  FILE_ATTRIBUTE_TAG_INFO info;
  HANDLE token = NULL;
  PSID owner = NULL, installer = NULL;
  PACL acl = NULL;
  PSECURITY_DESCRIPTOR descriptor = NULL;
  union { TOKEN_USER user; BYTE bytes[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE]; } tokenData;
  DWORD size;
  if (!GetFileInformationByHandleEx(file, FileAttributeTagInfo, &info, sizeof(info))) goto cleanup;
  if (info.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
    status = OPENVR_ASSETS_SYMLINK;
    goto cleanup;
  }
  bool directory = (info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) ||
      !GetTokenInformation(token, TokenUser, &tokenData, sizeof(tokenData), &size) ||
      !ConvertStringSidToSidW(L"S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464", &installer)) goto cleanup;
  DWORD error = GetSecurityInfo(file, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
    &owner, NULL, &acl, NULL, &descriptor);
  if (error != ERROR_SUCCESS) {
    SetLastError(error);
    goto cleanup;
  }
  metadata->kind = directory ? OPENVR_ASSET_DIRECTORY : GetFileType(file) == FILE_TYPE_DISK ? OPENVR_ASSET_REGULAR : OPENVR_ASSET_OTHER;
  metadata->trustedOwner = trustedSid(owner, tokenData.user.User.Sid, installer);
  metadata->permissions = trustedAcl(acl, tokenData.user.User.Sid, installer, directory) ? 0 : 0022;
  metadata->readable = false;
  if (metadata->kind == OPENVR_ASSET_REGULAR) {
    HANDLE readable = ReOpenFile(file, FILE_READ_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0);
    metadata->readable = readable != INVALID_HANDLE_VALUE;
    if (metadata->readable) CloseHandle(readable);
  }
  status = OPENVR_ASSETS_OK_PATHS_NOT_PINNED;

cleanup:
  if (status == OPENVR_ASSETS_IO_ERROR) *osError = (int) GetLastError();
  if (descriptor) LocalFree(descriptor);
  if (installer) LocalFree(installer);
  if (token) CloseHandle(token);
  CloseHandle(file);
  return status;
}
#else
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
#endif

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
  bool useNative = !provider;
  if (!provider) provider = &native;
  OpenVRAssetsResult result = { .status = OPENVR_ASSETS_INVALID_PATH, .assetIndex = SIZE_MAX };
  if (!provider->artifact || !provider->canonicalize || !provider->inspect) return result;
  char source[OPENVR_ASSET_PATH_CAPACITY] = { 0 };
  char path[OPENVR_ASSET_PATH_CAPACITY] = { 0 };
  strcpy(result.failingPath, shared ? "loaded LÖVR artifact" : "running executable");
  result.status = provider->artifact(provider->context, shared, source, sizeof(source), &result.osError);
  if (result.status != OPENVR_ASSETS_OK_PATHS_NOT_PINNED) return result;
  if (!memchr(source, 0, sizeof(source))) { result.status = OPENVR_ASSETS_PATH_TOO_LONG; return result; }
  strcpy(result.failingPath, source);
  if (!lovrOpenVRAssetPathRootLength(source)) {
    result.status = OPENVR_ASSETS_RELATIVE_ARTIFACT_UNSUPPORTED;
    return result;
  }
  result.status = provider->canonicalize(provider->context, source, path, sizeof(path), &result.osError);
  if (result.status != OPENVR_ASSETS_OK_PATHS_NOT_PINNED) return result;
  if (!memchr(path, 0, sizeof(path))) { result.status = OPENVR_ASSETS_PATH_TOO_LONG; return result; }
  size_t root = lovrOpenVRAssetPathRootLength(path);
  if (!root || strstr(path, "/../") || strstr(path, "/./") || strstr(path, "//")) {
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
  for (size_t i = root; i < baseLength; i++) {
    if (path[i] != '/') continue;
    path[i] = 0;
    result.status = validate(provider, path, true, &result);
    path[i] = '/';
    if (result.status != OPENVR_ASSETS_OK_PATHS_NOT_PINNED) return result;
  }
  char first = path[root];
  path[root] = 0;
  result.status = validate(provider, path, true, &result);
  path[root] = first;
  if (result.status != OPENVR_ASSETS_OK_PATHS_NOT_PINNED) return result;
  for (size_t i = 0; i < OPENVR_ASSET_COUNT; i++) {
    result.assetIndex = i;
    strcpy(path + baseLength, assets[i]);
    result.status = validate(provider, path, false, &result);
    if (result.status != OPENVR_ASSETS_OK_PATHS_NOT_PINNED) { result.manifest[0] = 0; return result; }

  }
  strcpy(path + baseLength, assets[0]);
  strcpy(result.manifest, path);
#ifdef _WIN32
  if (useNative) pathSlashes(result.manifest, '/', '\\');
#else
  (void) useNative;
#endif
  result.failingPath[0] = 0;
  result.osError = 0;
  result.assetIndex = SIZE_MAX;
  return result;
}
