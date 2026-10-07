#include "headset/openvr_runtime.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#define OPENVR_LOADER_API __declspec(dllimport)
#else
#define OPENVR_LOADER_API extern
#endif

OPENVR_LOADER_API uint32_t LOVR_OPENVR_CALLTYPE VR_InitInternal2(EVRInitError* error, EVRApplicationType type, const char* startupInfo);
OPENVR_LOADER_API void LOVR_OPENVR_CALLTYPE VR_ShutdownInternal(void);
OPENVR_LOADER_API void* LOVR_OPENVR_CALLTYPE VR_GetGenericInterface(const char* version, EVRInitError* error);
OPENVR_LOADER_API bool LOVR_OPENVR_CALLTYPE VR_IsInterfaceVersionValid(const char* version);

static const OpenVRLoader defaultLoader = {
  .init = VR_InitInternal2,
  .shutdown = VR_ShutdownInternal,
  .getInterface = VR_GetGenericInterface,
  .isInterfaceVersionValid = VR_IsInterfaceVersionValid
};

static atomic_flag lock = ATOMIC_FLAG_INIT;
static OpenVRRuntime* owner;

static EVRInitError connect(OpenVRRuntime* runtime, const OpenVRLoader* loader);
static void disconnect(OpenVRRuntime* runtime);
static void* getInterface(OpenVRRuntime* runtime, const char* version, EVRInitError* error);

EVRInitError lovrOpenVRConnect(OpenVRRuntime* runtime, const OpenVRLoader* loader) {
  if (!runtime) return EVRInitError_VRInitError_Init_InvalidInterface;
  while (atomic_flag_test_and_set_explicit(&lock, memory_order_acquire)) {}
  EVRInitError error = connect(runtime, loader);
  atomic_flag_clear_explicit(&lock, memory_order_release);
  return error;
}

void lovrOpenVRDisconnect(OpenVRRuntime* runtime) {
  if (!runtime) return;
  while (atomic_flag_test_and_set_explicit(&lock, memory_order_acquire)) {}
  disconnect(runtime);
  atomic_flag_clear_explicit(&lock, memory_order_release);
}

static EVRInitError connect(OpenVRRuntime* runtime, const OpenVRLoader* loader) {
  if (owner == runtime) return EVRInitError_VRInitError_None;
  if (owner) return EVRInitError_VRInitError_Init_AlreadyRunning;

  OpenVRLoader ops = loader ? *loader : defaultLoader;
  memset(runtime, 0, sizeof(*runtime));
  if (!ops.init || !ops.shutdown || !ops.getInterface || !ops.isInterfaceVersionValid) {
    return EVRInitError_VRInitError_Init_InvalidInterface;
  }

  owner = runtime;
  runtime->loader = ops;
  runtime->initialized = true;
  EVRInitError error = EVRInitError_VRInitError_None;
  ops.init(&error, EVRApplicationType_VRApplication_Overlay, NULL);
  if (error != EVRInitError_VRInitError_None) goto fail;

  if (!ops.isInterfaceVersionValid(IVRSystem_Version)) {
    error = EVRInitError_VRInitError_Init_InterfaceNotFound;
    goto fail;
  }

  runtime->system = getInterface(runtime, IVRSystem_Version, &error);
  if (error != EVRInitError_VRInitError_None) goto fail;
  if (!runtime->system->SetSDKVersion) {
    error = EVRInitError_VRInitError_Init_InvalidInterface;
    goto fail;
  }

  error = runtime->system->SetSDKVersion(k_nSteamVRVersionMajor, k_nSteamVRVersionMinor, k_nSteamVRVersionBuild);
  if (error != EVRInitError_VRInitError_None) goto fail;

  runtime->overlay = getInterface(runtime, IVROverlay_Version, &error);
  if (error != EVRInitError_VRInitError_None) goto fail;
  runtime->input = getInterface(runtime, IVRInput_Version, &error);
  if (error != EVRInitError_VRInitError_None) goto fail;
  runtime->compositor = getInterface(runtime, IVRCompositor_Version, &error);
  if (error != EVRInitError_VRInitError_None) goto fail;
  runtime->applications = getInterface(runtime, IVRApplications_Version, &error);
  if (error != EVRInitError_VRInitError_None) goto fail;

  return EVRInitError_VRInitError_None;

fail:
  disconnect(runtime);
  return error;
}

static void disconnect(OpenVRRuntime* runtime) {
  if (owner != runtime) {
    memset(runtime, 0, sizeof(*runtime));
    return;
  }

  OpenVRLoader loader = runtime->loader;
  memset(runtime, 0, sizeof(*runtime));
  loader.shutdown();
  owner = NULL;
}

static void* getInterface(OpenVRRuntime* runtime, const char* version, EVRInitError* error) {
  char name[128];
  int length = snprintf(name, sizeof(name), "FnTable:%s", version);
  if (length < 0 || (size_t) length >= sizeof(name)) {
    *error = EVRInitError_VRInitError_Init_InvalidInterface;
    return NULL;
  }

  *error = EVRInitError_VRInitError_None;
  void* interface = runtime->loader.getInterface(name, error);
  if (!interface && *error == EVRInitError_VRInitError_None) {
    *error = EVRInitError_VRInitError_Init_InterfaceNotFound;
  }
  return *error == EVRInitError_VRInitError_None ? interface : NULL;
}
