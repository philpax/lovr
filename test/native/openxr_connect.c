#include "test.h"
#include "../../src/modules/headset/headset_openxr.c"

struct ModelMetadata* lovrModelGetMetadata(Model* model) { abort(); }
uintptr_t gpu_vk_get_physical_device(void) { abort(); }
void os_sleep(double seconds) { abort(); }
const DataField* lovrMeshGetVertexFormat(Mesh* mesh) { abort(); }
Mesh* lovrMeshCreate(const MeshInfo* info, void** data) { abort(); }
void* lovrMeshSetVertices(Mesh* mesh, uint32_t index, uint32_t count) { abort(); }
void* lovrMeshSetIndices(Mesh* mesh, uint32_t count, DataType type) { abort(); }
bool lovrMeshSetDrawRange(Mesh* mesh, uint32_t start, uint32_t count) { abort(); }
void lovrSystemGetWindowSize(uint32_t* width, uint32_t* height) { abort(); }
void lovrModelDataAllocate(ModelData* model) { abort(); }
bool lovrModelDataFinalize(ModelData* model) { abort(); }
Blob* lovrBlobCreate(void* data, size_t size, const char* name) { abort(); }
ModelData* lovrModelDataCreate(struct Blob* blob, ModelDataIO* io) { abort(); }
void lovrBlobDestroy(void* ref) { abort(); }
void lovrEventPush(Event event) { abort(); }
void lovrModelResetNodeTransforms(Model* model) { abort(); }
void lovrModelSetNodeTransform(Model* model, uint32_t node, float* position, float* scale, float* rotation, float alpha) { abort(); }
void lovrModelSetNodeVisible(Model* model, uint32_t node, bool visible) { abort(); }
const TextureInfo* lovrTextureGetInfo(Texture* texture) { abort(); }
float lovrMathLinearToGamma(float x) { abort(); }
float lovrMathGammaToLinear(float x) { abort(); }
bool lovrGraphicsIsInitialized(void) { abort(); }
uintptr_t gpu_vk_get_instance(void) { abort(); }
uintptr_t gpu_vk_get_device(void) { abort(); }
uintptr_t gpu_vk_get_queue(uint32_t* queueFamilyIndex, uint32_t* queueIndex) { abort(); }
uint32_t lovrGraphicsGetFormatSupport(uint32_t format, uint32_t features) { abort(); }
void lovrGraphicsGetFeatures(GraphicsFeatures* features) { abort(); }
void lovrGraphicsGetBackgroundColor(float background[4]) { abort(); }
Shader* lovrGraphicsGetDefaultShader(DefaultShader type) { abort(); }
bool lovrPassPush(Pass* pass, StackType stack) { abort(); }
void lovrPassSetShader(Pass* pass, Shader* shader) { abort(); }
void lovrPassSetColor(Pass* pass, float color[4]) { abort(); }
bool lovrPassDrawMesh(Pass* pass, Mesh* mesh, float* transform, uint32_t instances) { abort(); }
bool lovrPassPop(Pass* pass, StackType stack) { abort(); }
void lovrPassSetViewport(Pass* pass, float viewport[6]) { abort(); }
bool lovrPassSetScissor(Pass* pass, uint32_t scissor[4]) { abort(); }
Texture* lovrTextureCreate(const TextureInfo* info) {
  (void) info;
  atomic_uint* texture = lovrCalloc(sizeof(*texture));
  atomic_init(texture, 1);
  return (Texture*) texture;
}

void lovrTextureDestroy(void* texture) { lovrFree(texture); }
void lovrPassDestroy(void* pass) { lovrFree(pass); }
Pass* lovrPassCreate(const char* label) {
  (void) label;
  atomic_uint* pass = lovrCalloc(sizeof(*pass));
  atomic_init(pass, 1);
  return (Pass*) pass;
}
bool lovrPassSetCanvas(Pass* pass, Canvas* canvas) { (void) pass; (void) canvas; return true; }
bool lovrPassSetClear(Pass* pass, LoadAction loads[4], float clears[4][4], LoadAction depthLoad, float depthClear) {
  (void) pass; (void) loads; (void) clears; (void) depthLoad; (void) depthClear;
  return true;
}
bool lovrPassSetViewMatrix(Pass* pass, uint32_t index, float matrix[16]) {
  (void) pass; (void) index; (void) matrix;
  return true;
}
bool lovrPassSetProjection(Pass* pass, uint32_t index, float matrix[16]) {
  (void) pass; (void) index; (void) matrix;
  return true;
}
void lovrMeshDestroy(void* mesh) { lovrFree(mesh); }
bool gpu_prepare_teardown(void) { return true; }
void gpu_flush_deferred_after_idle(void) {}
bool lovrGraphicsPrepareSessionTeardown(void) { return true; }
void lovrGraphicsInvalidateSessionTexture(Texture* texture) { abort(); }
void lovrGraphicsInvalidateSessionPass(Pass* pass) { abort(); }

static bool overlay, extended, failDestroy, failActionDestroy, failSystem, failCreate, failEnumeration, failBlend;
static unsigned instances, destructions, sessions, suggestions, generation;
static uint32_t handBindings;
static bool withoutMicrogestures, gaze, enabledOverlay, enabledGaze, populatedProperties;

static XrResult XRAPI_CALL mockDestroyInstance(XrInstance instance) {
  if (failDestroy) return XR_ERROR_RUNTIME_FAILURE;
  destructions++;
  return XR_SUCCESS;
}
static XrResult XRAPI_CALL mockInstanceProperties(XrInstance instance, XrInstanceProperties* properties) { return XR_SUCCESS; }
static XrResult XRAPI_CALL mockSystem(XrInstance instance, const XrSystemGetInfo* info, XrSystemId* system) {
  if (failSystem) return XR_ERROR_FORM_FACTOR_UNAVAILABLE;
  *system = 1;
  return XR_SUCCESS;
}
static XrResult XRAPI_CALL mockSystemProperties(XrInstance instance, XrSystemId system, XrSystemProperties* properties) {
  XrSystemEyeGazeInteractionPropertiesEXT* eyeGaze = properties->next;
  if (eyeGaze) {
    if (eyeGaze->type != XR_TYPE_SYSTEM_EYE_GAZE_INTERACTION_PROPERTIES_EXT) abort();
    eyeGaze->supportsEyeGazeInteraction = XR_TRUE;
    populatedProperties = true;
  }
  return XR_SUCCESS;
}
static XrResult XRAPI_CALL mockViews(XrInstance instance, XrSystemId system, uint32_t capacity, uint32_t* count, XrViewConfigurationType* views) {
  *count = 1;
  views[0] = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  return XR_SUCCESS;
}
static XrResult XRAPI_CALL mockViewSizes(XrInstance instance, XrSystemId system, XrViewConfigurationType type, uint32_t capacity, uint32_t* count, XrViewConfigurationView* views) {
  *count = 2;
  if (views) for (unsigned i = 0; i < 2; i++) {
    views[i].maxImageRectWidth = views[i].maxImageRectHeight = 1024;
    views[i].recommendedImageRectWidth = views[i].recommendedImageRectHeight = 512;
  }
  return XR_SUCCESS;
}
static XrResult XRAPI_CALL mockBlend(XrInstance instance, XrSystemId system, XrViewConfigurationType type, uint32_t capacity, uint32_t* count, XrEnvironmentBlendMode* modes) {
  *count = 1;
  if (modes) {
    if (failBlend) return XR_ERROR_RUNTIME_FAILURE;
    modes[0] = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  }
  return XR_SUCCESS;
}
static XrResult XRAPI_CALL mockActionSet(XrInstance instance, const XrActionSetCreateInfo* info, XrActionSet* set) { *set = (XrActionSet) (uintptr_t) 1; return XR_SUCCESS; }
static XrResult XRAPI_CALL mockDestroyActionSet(XrActionSet set) { return failActionDestroy ? XR_ERROR_RUNTIME_FAILURE : XR_SUCCESS; }
static XrResult XRAPI_CALL mockAction(XrActionSet set, const XrActionCreateInfo* info, XrAction* action) { *action = (XrAction) (uintptr_t) (++generation); return XR_SUCCESS; }
static XrResult XRAPI_CALL mockPath(XrInstance instance, const char* string, XrPath* path) {
  *path = !strcmp(string, lovrInteractionProfilePaths[PROFILE_HAND]) ? 123 : ++generation + 1000;
  return XR_SUCCESS;
}
static XrResult XRAPI_CALL mockSuggest(XrInstance instance, const XrInteractionProfileSuggestedBinding* info) {
  suggestions++;
  if (info->interactionProfile == 123) handBindings = info->countSuggestedBindings;
  return XR_SUCCESS;
}
static XrResult XRAPI_CALL mockSession(XrInstance instance, const XrSessionCreateInfo* info, XrSession* session) { sessions++; abort(); }
static XrResult XRAPI_CALL mockResult(XrInstance instance, XrResult result, char* text) { strcpy(text, "mock failure"); return XR_SUCCESS; }

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateInstanceExtensionProperties(const char* name, uint32_t capacity, uint32_t* count, XrExtensionProperties* properties) {
  const char* names[] = { "XR_EXTX_overlay", "XR_EXT_hand_interaction", "XR_EXT_palm_pose", "XR_META_hand_tracking_microgestures" };
  *count = overlay + (extended ? (withoutMicrogestures ? 2 : 3) : 0) + gaze;
  if (properties) {
    if (failEnumeration) return XR_ERROR_RUNTIME_FAILURE;
    unsigned j = 0;
    for (unsigned i = overlay ? 0 : 1; i < (extended ? (withoutMicrogestures ? 3u : 4u) : 1u); i++) strcpy(properties[j++].extensionName, names[i]);
    if (gaze) strcpy(properties[j++].extensionName, "XR_EXT_eye_gaze_interaction");
  }
  return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrCreateInstance(const XrInstanceCreateInfo* info, XrInstance* instance) {
  if (failCreate) return XR_ERROR_RUNTIME_FAILURE;
  enabledOverlay = enabledGaze = false;
  for (uint32_t i = 0; i < info->enabledExtensionCount; i++) {
    enabledOverlay |= !strcmp(info->enabledExtensionNames[i], "XR_EXTX_overlay");
    enabledGaze |= !strcmp(info->enabledExtensionNames[i], "XR_EXT_eye_gaze_interaction");
  }
  instances++;
  *instance = (XrInstance) (uintptr_t) instances;
  return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function) {
  *function = NULL;
#define LOAD(symbol, mock) if (!strcmp(name, #symbol)) *function = (PFN_xrVoidFunction) mock
  LOAD(xrDestroyInstance, mockDestroyInstance);
  LOAD(xrGetInstanceProperties, mockInstanceProperties);
  LOAD(xrGetSystem, mockSystem);
  LOAD(xrGetSystemProperties, mockSystemProperties);
  LOAD(xrEnumerateViewConfigurations, mockViews);
  LOAD(xrEnumerateViewConfigurationViews, mockViewSizes);
  LOAD(xrEnumerateEnvironmentBlendModes, mockBlend);
  LOAD(xrCreateActionSet, mockActionSet);
  LOAD(xrDestroyActionSet, mockDestroyActionSet);
  LOAD(xrCreateAction, mockAction);
  LOAD(xrStringToPath, mockPath);
  LOAD(xrSuggestInteractionProfileBindings, mockSuggest);
  LOAD(xrCreateSession, mockSession);
  LOAD(xrResultToString, mockResult);
#undef LOAD
  return *function ? XR_SUCCESS : XR_ERROR_FUNCTION_UNSUPPORTED;
}

static void reset(void) {
  memset(&state, 0, sizeof(state));
  state.config.supersample = 1.f;
  overlay = extended = failDestroy = failActionDestroy = failSystem = failCreate = failEnumeration = failBlend = false;
  instances = destructions = sessions = suggestions = generation = handBindings = 0;
  withoutMicrogestures = gaze = enabledOverlay = enabledGaze = populatedProperties = false;
}
static bool noOverlay(void) {
  reset();
  CHECK(lovrOpenXRConnect(OPENXR_CONNECT_REQUIRE_OVERLAY) == OPENXR_CONNECT_UNAVAILABLE_CLEANED);
  CHECK(!instances && !sessions && !state.instance && !state.system && !state.cleanupPending);
  CHECK(openxrHeadsetConnect());
  CHECK(instances == 1 && !sessions && !state.extensions.overlay && !enabledOverlay);
  CHECK(openxrDisconnect());
  return true;
}
static bool probe(void) {
  reset(); overlay = gaze = true;
  state.config.extensions = lovrMalloc(32);
  strcpy(state.config.extensions, "XR_EXT_eye_gaze_interaction");
  state.config.extensionCount = 1;
  state.config.overlayOrder = 7;
  state.simulator.initialized = true;
  state.simulator.poses[DEVICE_HEAD][1] = 2.f;
  state.clipNear = .02f;
  state.clipFar = 100.f;
  HeadsetConfig config = state.config;
  Simulator simulator = state.simulator;
  CHECK(lovrOpenXRConnect(OPENXR_CONNECT_REQUIRE_OVERLAY) == OPENXR_CONNECT_SELECTED);
  CHECK(state.instance && state.system && !state.session && !sessions && state.extensions.overlay);
  CHECK(!state.config.overlay && !state.systemProperties.next);
  CHECK(enabledOverlay && enabledGaze && populatedProperties && state.extensions.gaze);
  CHECK(lovrOpenXRConnect(OPENXR_CONNECT_REQUIRE_OVERLAY) == OPENXR_CONNECT_SELECTED);
  CHECK(instances == 1);
  CHECK(openxrDisconnect());
  CHECK(!state.actions[ACTION_GRIP_POSE] && !state.blendModes && !state.extensions.overlay && !state.viewConfiguration);
  CHECK(!memcmp(&config, &state.config, sizeof(config)));
  CHECK(!memcmp(&simulator, &state.simulator, sizeof(simulator)));
  CHECK(state.clipNear == .02f && state.clipFar == 100.f);
  CHECK(!strcmp(state.config.extensions, "XR_EXT_eye_gaze_interaction"));
  lovrFree(state.config.extensions);
  state.config.extensions = NULL;
  return true;
}
static bool cleanupRetry(void) {
  reset(); overlay = failSystem = failDestroy = true;
  CHECK(lovrOpenXRConnect(OPENXR_CONNECT_REQUIRE_OVERLAY) == OPENXR_CONNECT_CLEANUP_PENDING);
  CHECK(state.instance && state.cleanupPending && !openxrHeadsetIsConnected());
  CHECK(strstr(lovrGetError(), "xrGetSystem") && strstr(lovrGetError(), "xrDestroyInstance"));
  CHECK(lovrOpenXRConnect(OPENXR_CONNECT_REQUIRE_OVERLAY) == OPENXR_CONNECT_CLEANUP_PENDING);
  CHECK(instances == 1 && !sessions);
  CHECK(strstr(lovrGetError(), "xrGetSystem") && strstr(lovrGetError(), "xrDestroyInstance"));
  failDestroy = failSystem = false;
  CHECK(lovrOpenXRConnect(OPENXR_CONNECT_REQUIRE_OVERLAY) == OPENXR_CONNECT_SELECTED);
  CHECK(instances == 2 && destructions == 1 && !state.cleanupPending);
  failActionDestroy = true;
  CHECK(!openxrDisconnect() && state.actionSet && state.instance && state.blendModes);
  CHECK(lovrOpenXRConnect(OPENXR_CONNECT_REQUIRE_OVERLAY) == OPENXR_CONNECT_CLEANUP_PENDING);
  CHECK(instances == 2);
  failActionDestroy = false;
  CHECK(openxrDisconnect());
  return true;
}
static bool failures(void) {
  reset(); overlay = true;
  failEnumeration = true;
  CHECK(lovrOpenXRConnect(OPENXR_CONNECT_REQUIRE_OVERLAY) == OPENXR_CONNECT_UNAVAILABLE_CLEANED);
  failEnumeration = false; failCreate = true;
  CHECK(lovrOpenXRConnect(OPENXR_CONNECT_REQUIRE_OVERLAY) == OPENXR_CONNECT_UNAVAILABLE_CLEANED);
  CHECK(!instances);
  failCreate = false; failSystem = true;
  CHECK(!openxrHeadsetConnect() && !openxrHeadsetIsConnected());
  CHECK(!state.instance && !state.system && !state.cleanupPending);
  failSystem = false; failBlend = true;
  CHECK(lovrOpenXRConnect(OPENXR_CONNECT_REQUIRE_OVERLAY) == OPENXR_CONNECT_UNAVAILABLE_CLEANED);
  CHECK(instances == destructions && !state.blendModes);
  failBlend = false;
  CHECK(openxrHeadsetConnect());
  CHECK(openxrDisconnect());
  return true;
}
static bool bindingsRetry(void) {
  reset(); overlay = true;
  Binding original[64];
  size_t count = 0;
  while (lovrActionBindings[PROFILE_HAND][count].path) count++;
  CHECK(count <= COUNTOF(original));
  memcpy(original, lovrActionBindings[PROFILE_HAND], count * sizeof(Binding));
  CHECK(openxrHeadsetConnect());
  CHECK(!memcmp(original, lovrActionBindings[PROFILE_HAND], count * sizeof(Binding)));
  CHECK(openxrDisconnect());
  extended = withoutMicrogestures = true;
  CHECK(lovrOpenXRConnect(OPENXR_CONNECT_REQUIRE_OVERLAY) == OPENXR_CONNECT_SELECTED);
  CHECK(handBindings == count - 10);
  CHECK(openxrDisconnect());
  withoutMicrogestures = false;
  CHECK(lovrOpenXRConnect(OPENXR_CONNECT_REQUIRE_OVERLAY) == OPENXR_CONNECT_SELECTED);
  CHECK(handBindings == count);
  CHECK(!memcmp(original, lovrActionBindings[PROFILE_HAND], count * sizeof(Binding)));
  CHECK(openxrDisconnect());
  return true;
}
int main(int argc, char** argv) {
  NativeTest tests[] = {
    { "openxr.connect.no-overlay", noOverlay },
    { "openxr.connect.probe", probe },
    { "openxr.connect.cleanup-retry", cleanupRetry },
    { "openxr.connect.failures", failures },
    { "openxr.connect.bindings-retry", bindingsRetry }
  };
  return nativeRunTests(argc, argv, tests, COUNTOF(tests));
}
