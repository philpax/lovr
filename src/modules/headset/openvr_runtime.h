#ifndef LOVR_OPENVR_RUNTIME_H
#define LOVR_OPENVR_RUNTIME_H

#include <openvr_capi.h>

#if defined(_WIN32)
#define LOVR_OPENVR_CALLTYPE __cdecl
#else
#define LOVR_OPENVR_CALLTYPE
#endif

typedef struct OpenVRLoader {
  uint32_t (LOVR_OPENVR_CALLTYPE *init)(EVRInitError* error, EVRApplicationType type, const char* startupInfo);
  void (LOVR_OPENVR_CALLTYPE *shutdown)(void);
  void* (LOVR_OPENVR_CALLTYPE *getInterface)(const char* version, EVRInitError* error);
  bool (LOVR_OPENVR_CALLTYPE *isInterfaceVersionValid)(const char* version);
} OpenVRLoader;

typedef struct OpenVRRuntime {
  struct VR_IVRSystem_FnTable* system;
  struct VR_IVROverlay_FnTable* overlay;
  struct VR_IVRInput_FnTable* input;
  struct VR_IVRCompositor_FnTable* compositor;
  struct VR_IVRApplications_FnTable* applications;
  OpenVRLoader loader;
  bool initialized;
} OpenVRRuntime;

EVRInitError lovrOpenVRConnect(OpenVRRuntime* runtime, const OpenVRLoader* loader);
void lovrOpenVRDisconnect(OpenVRRuntime* runtime);

#endif
