#include "test.h"
#include "headset/openvr_runtime.h"
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>

static struct {
  unsigned int inits;
  unsigned int shutdowns;
  unsigned int validations;
  unsigned int interfaces;
  unsigned int sdkCalls;
  EVRInitError initError;
  EVRInitError sdkError;
  EVRInitError interfaceError;
  unsigned int failInterface;
  bool validVersion;
  bool validCalls;
  bool active;
  struct VR_IVRSystem_FnTable system;
  struct VR_IVROverlay_FnTable overlay;
  struct VR_IVRInput_FnTable input;
  struct VR_IVRCompositor_FnTable compositor;
  struct VR_IVRApplications_FnTable applications;
} fake;

static uint32_t LOVR_OPENVR_CALLTYPE fakeInit(EVRInitError* error, EVRApplicationType type, const char* startupInfo) {
  fake.validCalls &= !fake.active && type == EVRApplicationType_VRApplication_Overlay && startupInfo == NULL;
  fake.inits++;
  fake.active = true;
  *error = fake.initError;
  return 73;
}

static void LOVR_OPENVR_CALLTYPE fakeShutdown(void) {
  fake.validCalls &= fake.active;
  fake.active = false;
  fake.shutdowns++;
}

static bool LOVR_OPENVR_CALLTYPE fakeValidate(const char* version) {
  fake.validCalls &= fake.active && fake.interfaces == 0 && fake.sdkCalls == 0;
  fake.validCalls &= strcmp(version, IVRSystem_Version) == 0;
  fake.validations++;
  return fake.validVersion;
}

static EVRInitError OPENVR_FNTABLE_CALLTYPE fakeSDK(uint32_t major, uint32_t minor, uint32_t build) {
  fake.validCalls &= fake.active && fake.interfaces == 1 && fake.validations == 1;
  fake.validCalls &= major == k_nSteamVRVersionMajor && minor == k_nSteamVRVersionMinor && build == k_nSteamVRVersionBuild;
  fake.sdkCalls++;
  return fake.sdkError;
}

static void* LOVR_OPENVR_CALLTYPE fakeInterface(const char* version, EVRInitError* error) {
  const char* versions[] = { IVRSystem_Version, IVROverlay_Version, IVRInput_Version, IVRCompositor_Version, IVRApplications_Version };
  void* tables[] = { &fake.system, &fake.overlay, &fake.input, &fake.compositor, &fake.applications };
  unsigned int index = fake.interfaces++;
  fake.validCalls &= fake.active && fake.validations == 1 && index < 5;
  if (index >= 5) {
    *error = EVRInitError_VRInitError_Init_InvalidInterface;
    return NULL;
  }
  char expected[128];
  snprintf(expected, sizeof(expected), "FnTable:%s", versions[index]);
  fake.validCalls &= strcmp(version, expected) == 0;
  fake.validCalls &= index == 0 ? fake.sdkCalls == 0 : fake.sdkCalls == 1;
  if (fake.failInterface == index + 1) {
    *error = fake.interfaceError;
    return NULL;
  }
  *error = EVRInitError_VRInitError_None;
  return tables[index];
}

static const OpenVRLoader loader = {
  .init = fakeInit,
  .shutdown = fakeShutdown,
  .getInterface = fakeInterface,
  .isInterfaceVersionValid = fakeValidate
};

static void resetFake(void) {
  memset(&fake, 0, sizeof(fake));
  fake.validVersion = true;
  fake.validCalls = true;
  fake.system.SetSDKVersion = fakeSDK;
}

static bool cleared(const OpenVRRuntime* runtime) {
  return !runtime->initialized && !runtime->system && !runtime->overlay && !runtime->input && !runtime->compositor && !runtime->applications &&
    !runtime->loader.init && !runtime->loader.shutdown && !runtime->loader.getInterface && !runtime->loader.isInterfaceVersionValid;
}

static bool connectsOverlay(void) {
  resetFake();
  OpenVRRuntime runtime = { 0 };
  EVRInitError error = lovrOpenVRConnect(&runtime, &loader);
  bool connected = error == EVRInitError_VRInitError_None && runtime.initialized && runtime.system == &fake.system &&
    runtime.overlay == &fake.overlay && runtime.input == &fake.input && runtime.compositor == &fake.compositor && runtime.applications == &fake.applications;
  bool calls = fake.inits == 1 && fake.shutdowns == 0 && fake.validations == 1 && fake.interfaces == 5 && fake.sdkCalls == 1;
  lovrOpenVRDisconnect(&runtime);
  CHECK(connected);
  CHECK(calls);
  CHECK(fake.validCalls);
  CHECK(fake.shutdowns == 1 && !fake.active);
  CHECK(cleared(&runtime));
  return true;
}

static bool idempotentLifecycle(void) {
  resetFake();
  OpenVRRuntime runtime = { 0 };
  lovrOpenVRDisconnect(&runtime);
  CHECK(fake.shutdowns == 0);
  EVRInitError first = lovrOpenVRConnect(&runtime, &loader);
  OpenVRLoader invalid = { 0 };
  EVRInitError second = lovrOpenVRConnect(&runtime, &invalid);
  EVRInitError third = lovrOpenVRConnect(&runtime, NULL);
  bool calls = fake.inits == 1 && fake.interfaces == 5 && fake.sdkCalls == 1 && fake.shutdowns == 0;
  lovrOpenVRDisconnect(&runtime);
  lovrOpenVRDisconnect(&runtime);
  CHECK(first == EVRInitError_VRInitError_None && second == first && third == first);
  CHECK(calls && fake.validCalls);
  CHECK(fake.shutdowns == 1 && cleared(&runtime));
  resetFake();
  EVRInitError reconnected = lovrOpenVRConnect(&runtime, &loader);
  lovrOpenVRDisconnect(&runtime);
  CHECK(reconnected == EVRInitError_VRInitError_None);
  CHECK(fake.inits == 1 && fake.shutdowns == 1 && fake.validCalls);
  return true;
}

static bool exclusiveOwner(void) {
  resetFake();
  OpenVRRuntime ownerRuntime = { 0 };
  OpenVRRuntime secondRuntime = { 0 };
  EVRInitError first = lovrOpenVRConnect(&ownerRuntime, &loader);
  OpenVRRuntime copied = ownerRuntime;
  EVRInitError second = lovrOpenVRConnect(&secondRuntime, &loader);
  EVRInitError copyError = lovrOpenVRConnect(&copied, &loader);
  lovrOpenVRDisconnect(&secondRuntime);
  lovrOpenVRDisconnect(&copied);
  bool retained = ownerRuntime.initialized && fake.active && fake.shutdowns == 0 && fake.inits == 1 &&
    fake.interfaces == 5 && fake.sdkCalls == 1 && cleared(&secondRuntime) && cleared(&copied);
  EVRInitError stillConnected = lovrOpenVRConnect(&ownerRuntime, &loader);
  lovrOpenVRDisconnect(&ownerRuntime);
  CHECK(first == EVRInitError_VRInitError_None && stillConnected == first);
  CHECK(second == EVRInitError_VRInitError_Init_AlreadyRunning && copyError == second);
  CHECK(retained && fake.shutdowns == 1 && fake.validCalls && cleared(&ownerRuntime));
  resetFake();
  EVRInitError replacement = lovrOpenVRConnect(&secondRuntime, &loader);
  lovrOpenVRDisconnect(&secondRuntime);
  CHECK(replacement == EVRInitError_VRInitError_None);
  CHECK(fake.inits == 1 && fake.shutdowns == 1 && fake.validCalls);
  return true;
}

static bool initFailure(void) {
  resetFake();
  fake.initError = EVRInitError_VRInitError_Init_InstallationNotFound;
  OpenVRRuntime runtime = { 0 };
  EVRInitError error = lovrOpenVRConnect(&runtime, &loader);
  lovrOpenVRDisconnect(&runtime);
  CHECK(error == fake.initError);
  CHECK(fake.inits == 1 && fake.shutdowns == 1 && !fake.active);
  CHECK(fake.validations == 0 && fake.interfaces == 0 && fake.sdkCalls == 0);
  CHECK(fake.validCalls && cleared(&runtime));
  resetFake();
  EVRInitError retried = lovrOpenVRConnect(&runtime, &loader);
  lovrOpenVRDisconnect(&runtime);
  CHECK(retried == EVRInitError_VRInitError_None && fake.validCalls);
  CHECK(fake.inits == 1 && fake.shutdowns == 1);
  return true;
}

static bool versionFailure(void) {
  resetFake();
  fake.validVersion = false;
  OpenVRRuntime runtime = { 0 };
  EVRInitError error = lovrOpenVRConnect(&runtime, &loader);
  lovrOpenVRDisconnect(&runtime);
  CHECK(error == EVRInitError_VRInitError_Init_InterfaceNotFound);
  CHECK(fake.inits == 1 && fake.shutdowns == 1 && fake.validations == 1);
  CHECK(fake.interfaces == 0 && fake.sdkCalls == 0 && fake.validCalls);
  CHECK(cleared(&runtime));
  return true;
}

static bool interfaceFailures(void) {
  for (unsigned int i = 1; i <= 5; i++) {
    for (unsigned int explicitError = 0; explicitError < 2; explicitError++) {
      resetFake();
      fake.failInterface = i;
      fake.interfaceError = explicitError ? EVRInitError_VRInitError_Init_InvalidInterface : EVRInitError_VRInitError_None;
      OpenVRRuntime runtime = { 0 };
      EVRInitError error = lovrOpenVRConnect(&runtime, &loader);
      lovrOpenVRDisconnect(&runtime);
      CHECK(error == (explicitError ? EVRInitError_VRInitError_Init_InvalidInterface : EVRInitError_VRInitError_Init_InterfaceNotFound));
      CHECK(fake.inits == 1 && fake.shutdowns == 1 && fake.interfaces == i);
      CHECK(fake.sdkCalls == (i > 1 ? 1u : 0u));
      CHECK(fake.validCalls && !fake.active && cleared(&runtime));
    }
  }
  return true;
}

static bool sdkFailures(void) {
  for (unsigned int missing = 0; missing < 2; missing++) {
    resetFake();
    fake.sdkError = EVRInitError_VRInitError_Init_InvalidApplicationType;
    if (missing) fake.system.SetSDKVersion = NULL;
    OpenVRRuntime runtime = { 0 };
    EVRInitError error = lovrOpenVRConnect(&runtime, &loader);
    lovrOpenVRDisconnect(&runtime);
    CHECK(error == (missing ? EVRInitError_VRInitError_Init_InvalidInterface : fake.sdkError));
    CHECK(fake.inits == 1 && fake.shutdowns == 1 && fake.interfaces == 1);
    CHECK(fake.sdkCalls == (missing ? 0u : 1u));
    CHECK(fake.validCalls && cleared(&runtime));
  }
  return true;
}

static bool incompleteLoader(void) {
  for (unsigned int missing = 0; missing < 4; missing++) {
    resetFake();
    OpenVRLoader invalid = loader;
    if (missing == 0) invalid.init = NULL;
    if (missing == 1) invalid.shutdown = NULL;
    if (missing == 2) invalid.getInterface = NULL;
    if (missing == 3) invalid.isInterfaceVersionValid = NULL;
    OpenVRRuntime runtime = { 0 };
    CHECK(lovrOpenVRConnect(&runtime, &invalid) == EVRInitError_VRInitError_Init_InvalidInterface);
    lovrOpenVRDisconnect(&runtime);
    CHECK(fake.inits == 0 && fake.shutdowns == 0 && fake.interfaces == 0 && fake.sdkCalls == 0);
    CHECK(cleared(&runtime));
  }
  CHECK(lovrOpenVRConnect(NULL, &loader) == EVRInitError_VRInitError_Init_InvalidInterface);
  lovrOpenVRDisconnect(NULL);
  return true;
}

static int missingRuntime(void) {
  const char* names[] = { "VR_OVERRIDE", "VR_PATHREG_OVERRIDE", "HOME", "XDG_CONFIG_HOME", "XDG_DATA_HOME" };
  const char* root = getenv(names[0]);
  struct stat info;
  if (!root || root[0] != '/' || stat(root, &info) == 0 || errno != ENOENT) {
    fprintf(stderr, "without-runtime mode requires an absolute, nonexistent VR_OVERRIDE path\n");
    return 2;
  }
  for (size_t i = 1; i < sizeof(names) / sizeof(names[0]); i++) {
    const char* value = getenv(names[i]);
    if (!value || strcmp(value, root) != 0) {
      fprintf(stderr, "without-runtime mode requires %s to equal VR_OVERRIDE\n", names[i]);
      return 2;
    }
  }
  OpenVRRuntime runtime = { 0 };
  EVRInitError error = lovrOpenVRConnect(&runtime, NULL);
  lovrOpenVRDisconnect(&runtime);
  lovrOpenVRDisconnect(&runtime);
  if (error == EVRInitError_VRInitError_None || !cleared(&runtime)) {
    fprintf(stderr, "without-runtime mode did not fail cleanly: %d\n", (int) error);
    return 1;
  }
  printf("PASS openvr.without-runtime (%d)\n", (int) error);
  return 0;
}

int main(int argc, char** argv) {
  if (argc == 2 && strcmp(argv[1], "--without-runtime") == 0) return missingRuntime();
  const NativeTest tests[] = {
    { "openvr.overlay-sdk-fntables", connectsOverlay },
    { "openvr.idempotent-lifecycle", idempotentLifecycle },
    { "openvr.exclusive-owner", exclusiveOwner },
    { "openvr.init-failure-cleanup-retry", initFailure },
    { "openvr.version-failure-cleanup", versionFailure },
    { "openvr.interface-failure-cleanup", interfaceFailures },
    { "openvr.sdk-failure-cleanup", sdkFailures },
    { "openvr.incomplete-loader", incompleteLoader }
  };
  return nativeRunTests(argc, argv, tests, sizeof(tests) / sizeof(tests[0]));
}
