#include "test.h"
#define lovrHeadsetValidateRuntimeManifest mockValidateRuntimeManifest
#include "../../src/modules/headset/headset_openxr.c"
#undef lovrHeadsetValidateRuntimeManifest

static unsigned manifestValidations, manifestFailureAt;
bool mockValidateRuntimeManifest(void) {
  return ++manifestValidations != manifestFailureAt;
}

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
static unsigned recenterEvents;
void lovrEventPush(Event event) {
  if (event.type != EVENT_RECENTER) abort();
  recenterEvents++;
}
void lovrModelResetNodeTransforms(Model* model) { abort(); }
void lovrModelSetNodeTransform(Model* model, uint32_t node, float* position, float* scale, float* rotation, float alpha) { abort(); }
void lovrModelSetNodeVisible(Model* model, uint32_t node, bool visible) { abort(); }
const TextureInfo* lovrTextureGetInfo(Texture* texture) { abort(); }
float lovrMathLinearToGamma(float x) { abort(); }
float lovrMathGammaToLinear(float x) { abort(); }
bool lovrGraphicsIsInitialized(void) { return false; }
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
bool lovrGraphicsRegisterSessionTexture(Texture* texture) { return texture != NULL; }
bool lovrGraphicsRegisterSessionPass(Pass* pass) { return pass != NULL; }
uint32_t lovrHeadsetNextSessionGeneration(void) { return 1; }

static struct {
  unsigned opens, closes, creates, destroys, gets, sets, heads, waits, syncs, views;
  XrSpaceLocationFlags flags;
  XrSpace sampledSpace, sampledBase;
  XrTime sampledTime;
  XrResult locateResult, waitResult;
  mnd_result_t getResult, setResult;
  bool offsetChangesDuringSample, missingSymbol, wrongVersion, failConnect;
  unsigned referenceCreates, referenceDestroys, stageCreates, failStageAt;
  XrReferenceSpaceType spaceTypes[16];
  XrPosef spacePoses[16];
  XrReferenceSpaceType pendingChange;
  mnd_pose_t offset, written;
  char calls[32];
  unsigned callCount;
} recenter;
static char recenterRoot, recenterLibrary;

static void recenterCall(char call) {
  if (recenter.callCount + 1 >= sizeof(recenter.calls)) abort();
  recenter.calls[recenter.callCount++] = call;
  recenter.calls[recenter.callCount] = '\0';
}
static void mockMonadoVersion(uint32_t* major, uint32_t* minor, uint32_t* patch) {
  *major = recenter.wrongVersion ? 2 : 1; *minor = 8; *patch = 0;
}
static mnd_result_t mockMonadoCreate(mnd_root_t** root) {
  if (recenter.failConnect) return MND_ERROR_OPERATION_FAILED;
  recenter.creates++;
  *root = (mnd_root_t*) &recenterRoot;
  return MND_SUCCESS;
}
static void mockMonadoDestroy(mnd_root_t** root) {
  if (*root != (mnd_root_t*) &recenterRoot) abort();
  recenter.destroys++;
  *root = NULL;
}
static mnd_result_t mockMonadoGet(mnd_root_t* root, mnd_reference_space_type_t space, mnd_pose_t* offset) {
  if (root != (mnd_root_t*) &recenterRoot || space != MND_SPACE_REFERENCE_TYPE_STAGE) abort();
  recenterCall('G');
  recenter.gets++;
  *offset = recenter.offset;
  return recenter.getResult;
}
static mnd_result_t mockMonadoSet(mnd_root_t* root, mnd_reference_space_type_t space, const mnd_pose_t* offset) {
  if (root != (mnd_root_t*) &recenterRoot || space != MND_SPACE_REFERENCE_TYPE_STAGE) abort();
  recenterCall('S');
  recenter.sets++;
  recenter.written = recenter.offset = *offset;
  // A runtime setter can mutate the origin before reporting an IPC failure.
  return recenter.setResult;
}
static void* mockMonadoOpen(void* context, const char* path) {
  if (context != &recenter || strcmp(path, "/runtime/libmonado.so")) abort();
  recenter.opens++;
  return &recenterLibrary;
}
static void* mockMonadoSymbol(void* context, void* library, const char* name) {
  if (context != &recenter || library != &recenterLibrary) abort();
  if (recenter.missingSymbol) return NULL;
  if (!strcmp(name, "mnd_api_get_version")) return (void*) mockMonadoVersion;
  if (!strcmp(name, "mnd_root_create")) return (void*) mockMonadoCreate;
  if (!strcmp(name, "mnd_root_destroy")) return (void*) mockMonadoDestroy;
  if (!strcmp(name, "mnd_root_get_reference_space_offset")) return (void*) mockMonadoGet;
  if (!strcmp(name, "mnd_root_set_reference_space_offset")) return (void*) mockMonadoSet;
  abort();
}
static void mockMonadoClose(void* context, void* library) {
  if (context != &recenter || library != &recenterLibrary || recenter.creates != recenter.destroys) abort();
  recenter.closes++;
}
static XrResult XRAPI_CALL mockLocateSpace(XrSpace space, XrSpace base, XrTime time, XrSpaceLocation* location) {
  recenterCall('H');
  recenter.heads++;
  recenter.sampledSpace = space;
  recenter.sampledBase = base;
  recenter.sampledTime = time;
  location->locationFlags = recenter.flags;
  location->pose = (XrPosef) { .orientation = { 0.f, sinf(.25f), 0.f, cosf(.25f) }, .position = { 2.f, 1.7f, -3.f } };
  if (location->next) {
    XrSpaceVelocity* velocity = location->next;
    velocity->velocityFlags = XR_SPACE_VELOCITY_LINEAR_VALID_BIT | XR_SPACE_VELOCITY_ANGULAR_VALID_BIT;
  }
  if (recenter.offsetChangesDuringSample && recenter.heads == 2) recenter.offset.position.x += 1.f;
  return recenter.locateResult;
}
static XrResult XRAPI_CALL mockWaitFrame(XrSession session, const XrFrameWaitInfo* info, XrFrameState* frame) {
  if (session != state.session || !state.trackingInvalidated) abort();
  recenter.waits++;
  if (XR_FAILED(recenter.waitResult)) return recenter.waitResult;
  frame->predictedDisplayTime += 100;
  frame->predictedDisplayPeriod = 100;
  return XR_SUCCESS;
}
static XrResult XRAPI_CALL mockSyncActions(XrSession session, const XrActionsSyncInfo* info) {
  if (session != state.session || state.trackingInvalidated) abort();
  recenter.syncs++;
  return XR_SUCCESS;
}
static XrResult XRAPI_CALL mockLocateViews(XrSession session, const XrViewLocateInfo* info,
    XrViewState* viewState, uint32_t capacity, uint32_t* count, XrView* views) {
  if (session != state.session || info->space != state.referenceSpace || !capacity) abort();
  recenter.views++;
  *count = 1;
  viewState->viewStateFlags = XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT;
  views[0].pose.orientation.w = 1.f;
  return XR_SUCCESS;
}

static bool loaderAvailable, loaderFailure;
static unsigned loaderInitializations;
static char selectedManifest[256];
static XrResult XRAPI_CALL mockInitializeLoader(const XrLoaderInitInfoBaseHeaderKHR* info) {
  if (info->type != XR_TYPE_LOADER_INIT_INFO_PROPERTIES_EXT) abort();
  const XrLoaderInitInfoPropertiesEXT* properties = (const XrLoaderInitInfoPropertiesEXT*) info;
  if (properties->propertyValueCount != 1 || strcmp(properties->propertyValues[0].name, "XR_RUNTIME_JSON")) abort();
  snprintf(selectedManifest, sizeof(selectedManifest), "%s", properties->propertyValues[0].value);
  loaderInitializations++;
  return loaderFailure ? XR_ERROR_INITIALIZATION_FAILED : XR_SUCCESS;
}

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
  if (loaderAvailable) LOAD(xrInitializeLoaderKHR, mockInitializeLoader);
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
  manifestValidations = manifestFailureAt = 0;
  loaderAvailable = loaderFailure = false;
  loaderInitializations = 0;
  selectedManifest[0] = '\0';
  lovrHeadsetSetRuntimePaths(NULL, NULL);
  state.config.supersample = 1.f;
  state.config.connect = true;
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
  CHECK(!lovrHeadsetSetRuntimePaths(NULL, NULL));
  failDestroy = failSystem = false;
  CHECK(lovrOpenXRConnect(OPENXR_CONNECT_REQUIRE_OVERLAY) == OPENXR_CONNECT_SELECTED);
  CHECK(instances == 2 && destructions == 1 && !state.cleanupPending);
  failActionDestroy = true;
  CHECK(!openxrDisconnect() && state.actionSet && state.instance && state.blendModes);
  CHECK(lovrOpenXRConnect(OPENXR_CONNECT_REQUIRE_OVERLAY) == OPENXR_CONNECT_CLEANUP_PENDING);
  CHECK(instances == 2);
  failActionDestroy = false;
  CHECK(openxrDisconnect());
  CHECK(lovrHeadsetSetRuntimePaths(NULL, NULL));
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
static bool matchingManifest(void) {
  reset();
  CHECK(lovrHeadsetSetRuntimePaths("/runtime/active.json", "/runtime/libmonado.so"));
  loaderAvailable = true;
  CHECK(openxrHeadsetConnect());
  CHECK(loaderInitializations == 1 && !strcmp(selectedManifest, "/runtime/active.json"));
  CHECK(state.runtimeManifestSelected);
  CHECK(!lovrHeadsetSetRuntimePaths(NULL, NULL));
  CHECK(openxrDisconnect());
  CHECK(lovrHeadsetSetRuntimePaths(NULL, NULL));

  reset();
  CHECK(lovrHeadsetSetRuntimePaths("/runtime/active.json", "/runtime/libmonado.so"));
  CHECK(openxrHeadsetConnect());
  CHECK(!state.runtimeManifestSelected && loaderInitializations == 0);
  CHECK(openxrDisconnect());

  reset(); loaderAvailable = loaderFailure = true;
  CHECK(lovrHeadsetSetRuntimePaths("/runtime/active.json", "/runtime/libmonado.so"));
  CHECK(openxrHeadsetConnect());
  CHECK(!state.runtimeManifestSelected && loaderInitializations == 1);
  CHECK(openxrDisconnect());
  CHECK(lovrHeadsetSetRuntimePaths(NULL, NULL));

  for (unsigned failure = 1; failure <= 2; failure++) {
    reset(); loaderAvailable = true;
    manifestFailureAt = failure;
    CHECK(lovrHeadsetSetRuntimePaths("/runtime/active.json", "/runtime/libmonado.so"));
    CHECK(openxrHeadsetConnect());
    CHECK(!state.runtimeManifestSelected && !state.recenter.path);
    CHECK(loaderInitializations == (failure == 1 ? 0u : 1u));
    CHECK(openxrDisconnect());
  }
  CHECK(lovrHeadsetSetRuntimePaths(NULL, NULL));
  return true;
}

static bool recenterFixture(void) {
  reset();
  memset(&recenter, 0, sizeof(recenter));
  recenterEvents = 0;
  const MonadoRecenterLoader loader = { &recenter, mockMonadoOpen, mockMonadoSymbol, mockMonadoClose };
  CHECK(lovrMonadoRecenterInitWithLoader(&state.recenter, "/runtime/libmonado.so", &loader));
  state.runtimeManifestSelected = true;
  state.monadoStage = true;
  state.session = (XrSession) (uintptr_t) 1;
  state.sessionState = XR_SESSION_STATE_FOCUSED;
  state.spaces[DEVICE_HEAD] = (XrSpace) (uintptr_t) 2;
  // Rendering and reset use distinct identity STAGE handles.
  state.recenterSpace = (XrSpace) (uintptr_t) 3;
  state.referenceSpace = (XrSpace) (uintptr_t) 4;
  state.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
  state.handSpaces[SIDE_LEFT][HAND_GRIP] = (XrSpace) (uintptr_t) 5;
  state.frameState.predictedDisplayTime = 100;
  state.viewCount = 1;
  recenter.flags = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT |
    XR_SPACE_LOCATION_POSITION_TRACKED_BIT | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
  recenter.offset = (mnd_pose_t) { .orientation = { 0.f, 0.f, 0.f, 1.f }, .position = { 10.f, -1.25f, 20.f } };
  xrLocateSpace = mockLocateSpace;
  xrWaitFrame = mockWaitFrame;
  xrSyncActions = mockSyncActions;
  xrLocateViews = mockLocateViews;
  return true;
}

static XrResult XRAPI_CALL mockStartSession(XrInstance instance, const XrSessionCreateInfo* info, XrSession* session) {
  *session = (XrSession) (uintptr_t) 1;
  return XR_SUCCESS;
}
static XrResult XRAPI_CALL mockAttach(XrSession session, const XrSessionActionSetsAttachInfo* info) { return XR_SUCCESS; }
static XrResult XRAPI_CALL mockDestroySession(XrSession session) { return XR_SUCCESS; }
static XrResult XRAPI_CALL mockTime(XrInstance instance, const struct timespec* time, XrTime* result) {
  *result = 100;
  return XR_SUCCESS;
}
static XrResult XRAPI_CALL mockReferenceSpace(XrSession session, const XrReferenceSpaceCreateInfo* info, XrSpace* space) {
  if (info->referenceSpaceType == XR_REFERENCE_SPACE_TYPE_STAGE && ++recenter.stageCreates == recenter.failStageAt) {
    return XR_ERROR_REFERENCE_SPACE_UNSUPPORTED;
  }
  unsigned index = ++recenter.referenceCreates;
  if (index >= COUNTOF(recenter.spaceTypes)) abort();
  recenter.spaceTypes[index] = info->referenceSpaceType;
  recenter.spacePoses[index] = info->poseInReferenceSpace;
  *space = (XrSpace) (uintptr_t) index;
  return XR_SUCCESS;
}
static XrResult XRAPI_CALL mockDestroySpace(XrSpace space) {
  recenter.referenceDestroys++;
  return XR_SUCCESS;
}
static XrResult XRAPI_CALL mockPollEvent(XrInstance instance, XrEventDataBuffer* buffer) {
  if (!recenter.pendingChange) return XR_EVENT_UNAVAILABLE;
  XrEventDataReferenceSpaceChangePending* event = (void*) buffer;
  event->type = XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING;
  event->referenceSpaceType = recenter.pendingChange;
  event->changeTime = 100;
  recenter.pendingChange = 0;
  return XR_SUCCESS;
}
static bool stageStartFixture(void) {
  CHECK(recenterFixture());
  state.session = XR_NULL_HANDLE;
  state.referenceSpace = state.recenterSpace = XR_NULL_HANDLE;
  state.referenceSpaceType = 0;
  state.monadoStage = false;
  memset(state.spaces, 0, sizeof(state.spaces));
  memset(state.handSpaces, 0, sizeof(state.handSpaces));
  CHECK(lovrHeadsetSetRuntimePaths("/runtime/active.json", "/runtime/libmonado.so"));
  CHECK(lovrHeadsetSetMonadoStageUnmapped(true));
  state.connected = true;
  state.extensions.headless = true;
  state.extensions.localFloor = true;
  xrCreateSession = mockStartSession;
  xrDestroySession = mockDestroySession;
  xrAttachSessionActionSets = mockAttach;
  xrCreateReferenceSpace = mockReferenceSpace;
  xrDestroySpace = mockDestroySpace;
  xrConvertTimespecTimeToTimeKHR = mockTime;
  xrPollEvent = mockPollEvent;
  return true;
}

static bool recenterCleanup(void) {
  lovrMonadoRecenterDestroy(&state.recenter);
  CHECK(recenter.opens == recenter.closes && recenter.creates == recenter.destroys);
  CHECK(!state.recenter.path && !state.recenter.root && !state.recenter.library);
  state.session = XR_NULL_HANDLE;
  return true;
}

static bool recenterStatus(void) {
  reset();
  CHECK(openxrHeadsetGetRecenterStatus() == HEADSET_RECENTER_UNSUPPORTED);
  CHECK(!openxrHeadsetGetRecenterSpace());
  state.runtimeManifestSelected = true;
  CHECK(openxrHeadsetGetRecenterStatus() == HEADSET_RECENTER_UNSUPPORTED);
  CHECK(!openxrHeadsetGetRecenterSpace());
  CHECK(recenterFixture());
  CHECK(!strcmp(openxrHeadsetGetRecenterSpace(), "stage"));
  state.runtimeManifestSelected = false;
  CHECK(openxrHeadsetGetRecenterStatus() == HEADSET_RECENTER_UNSUPPORTED);
  CHECK(!openxrHeadsetGetRecenterSpace());
  state.runtimeManifestSelected = true;
  state.session = XR_NULL_HANDLE;
  CHECK(openxrHeadsetGetRecenterStatus() == HEADSET_RECENTER_UNAVAILABLE);
  state.session = (XrSession) (uintptr_t) 1;
  state.sessionState = XR_SESSION_STATE_IDLE;
  CHECK(openxrHeadsetGetRecenterStatus() == HEADSET_RECENTER_UNAVAILABLE);
  state.sessionState = XR_SESSION_STATE_FOCUSED;
  XrSpace stage = state.recenterSpace;
  state.recenterSpace = XR_NULL_HANDLE;
  CHECK(openxrHeadsetGetRecenterStatus() == HEADSET_RECENTER_UNAVAILABLE);
  state.recenterSpace = stage;
  state.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT;
  CHECK(openxrHeadsetGetRecenterStatus() == HEADSET_RECENTER_UNSUPPORTED);
  CHECK(!openxrHeadsetGetRecenterSpace());
  CHECK(!openxrHeadsetRecenter() && recenter.sets == 0);
  state.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
  CHECK(openxrHeadsetGetRecenterStatus() == HEADSET_RECENTER_UNSUPPORTED);
  CHECK(!openxrHeadsetGetRecenterSpace());
  CHECK(!openxrHeadsetRecenter() && recenter.sets == 0);
  state.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
  CHECK(!recenter.heads && !recenter.opens && !recenter.gets && !recenter.sets);
  XrSpaceLocationFlags required = recenter.flags;
  for (unsigned bit = 0; bit < 4; bit++) {
    recenter.flags = required & ~((XrSpaceLocationFlags) 1 << bit);
    CHECK(openxrHeadsetGetRecenterStatus() == HEADSET_RECENTER_UNTRACKED);
  }
  recenter.flags = required;
  recenter.locateResult = XR_ERROR_RUNTIME_FAILURE;
  CHECK(openxrHeadsetGetRecenterStatus() == HEADSET_RECENTER_UNTRACKED);
  recenter.locateResult = XR_SUCCESS;
  state.frameState.predictedDisplayTime = 0;
  CHECK(openxrHeadsetGetRecenterStatus() == HEADSET_RECENTER_UNTRACKED);
  state.frameState.predictedDisplayTime = 100;
  CHECK(openxrHeadsetGetRecenterStatus() == HEADSET_RECENTER_AVAILABLE);
  CHECK(recenter.sampledSpace == state.spaces[DEVICE_HEAD]);
  CHECK(recenter.sampledBase == stage && recenter.sampledBase != state.referenceSpace);
  CHECK(recenter.sampledTime == state.frameState.predictedDisplayTime);
  CHECK(recenter.gets == 1 && !recenter.sets && !recenterEvents);
  return recenterCleanup();
}

static bool recenterStage(void) {
  CHECK(recenterFixture());
  XrSpace floor = state.referenceSpace;
  CHECK(openxrHeadsetRecenter());
  // Status samples once, then the reset brackets its own head sample with offset reads.
  CHECK(!strcmp(recenter.calls, "HGGHGS"));
  CHECK(recenter.sets == 1 && recenterEvents == 1);
  CHECK(recenter.sampledBase == state.recenterSpace && recenter.sampledBase != floor);
  CHECK(recenter.written.position.x == 12.f && recenter.written.position.z == 17.f);
  CHECK(recenter.written.position.y == -1.25f);
  CHECK(fabsf(recenter.written.orientation.y - sinf(.25f)) < 1e-5f);
  CHECK(fabsf(recenter.written.orientation.w - cosf(.25f)) < 1e-5f);
  CHECK(state.referenceSpace == floor && state.referenceSpaceType == XR_REFERENCE_SPACE_TYPE_STAGE);
  CHECK(state.frameState.predictedDisplayTime == 100 && state.trackingInvalidated);
  CHECK(openxrHeadsetGetRecenterStatus() == HEADSET_RECENTER_UNAVAILABLE);
  CHECK(!openxrHeadsetRecenter() && recenter.sets == 1 && recenterEvents == 1);
  return recenterCleanup();
}

static bool recenterInvalidation(void) {
  CHECK(recenterFixture());
  state.waited = true;
  CHECK(openxrHeadsetRecenter());
  float position[3], orientation[4], linear[3], angular[3];
  unsigned heads = recenter.heads;
  CHECK(!openxrHeadsetGetPose(DEVICE_HEAD, position, orientation));
  CHECK(!openxrHeadsetGetHandPose(SIDE_LEFT, HAND_GRIP, position, orientation));
  CHECK(!openxrHeadsetGetVelocity(DEVICE_HEAD, linear, angular));
  CHECK(!openxrHeadsetGetViewPose(0, position, orientation));
  CHECK(recenter.heads == heads && !recenter.views);
  CHECK(openxrHeadsetUpdate());
  CHECK(state.trackingInvalidated && !recenter.waits && !recenter.syncs);
  state.waited = false;
  recenter.waitResult = XR_ERROR_RUNTIME_FAILURE;
  CHECK(!openxrHeadsetUpdate());
  CHECK(state.trackingInvalidated && !state.waited && recenter.waits == 1 && !recenter.syncs);
  recenter.waitResult = XR_SUCCESS;
  CHECK(openxrHeadsetUpdate());
  CHECK(!state.trackingInvalidated && state.waited && recenter.waits == 2 && recenter.syncs == 1);
  CHECK(state.frameState.predictedDisplayTime == 200);
  CHECK(openxrHeadsetGetPose(DEVICE_HEAD, position, orientation));
  CHECK(openxrHeadsetGetHandPose(SIDE_LEFT, HAND_GRIP, position, orientation));
  CHECK(openxrHeadsetGetVelocity(DEVICE_HEAD, linear, angular));
  CHECK(openxrHeadsetGetViewPose(0, position, orientation));
  CHECK(recenter.heads == heads + 3 && recenter.views == 1 && recenterEvents == 1);
  return recenterCleanup();
}

static bool recenterFailures(void) {
  CHECK(recenterFixture());
  recenter.getResult = MND_ERROR_INVALID_OPERATION;
  CHECK(!openxrHeadsetRecenter());
  CHECK(!recenter.sets && !recenterEvents);
  CHECK(recenterCleanup());

  CHECK(recenterFixture());
  recenter.offsetChangesDuringSample = true;
  CHECK(!openxrHeadsetRecenter());
  CHECK(strstr(lovrGetError(), "offset changed during head sampling"));
  CHECK(!recenter.sets && !recenterEvents);
  CHECK(recenterCleanup());

  CHECK(recenterFixture());
  recenter.setResult = MND_ERROR_OPERATION_FAILED;
  CHECK(!openxrHeadsetRecenter());
  CHECK(recenter.sets == 1 && !recenterEvents && recenter.destroys == 1);
  CHECK(recenter.offset.position.x == 12.f && recenter.offset.position.z == 17.f);
  CHECK(state.trackingInvalidated);
  CHECK(strstr(lovrGetError(), "set offset"));
  return recenterCleanup();
}

static bool stageSelection(void) {
  for (unsigned seated = 0; seated < 2; seated++) {
    CHECK(stageStartFixture());
    state.config.seated = seated;
    CHECK(openxrHeadsetStart());
    CHECK(state.monadoStage && state.referenceSpaceType == XR_REFERENCE_SPACE_TYPE_STAGE);
    CHECK(!openxrHeadsetIsSeated());
    CHECK(recenter.opens == 1 && recenter.creates == 1 && recenter.gets == 1);
    CHECK(!recenter.heads && !recenter.sets && !strcmp(recenter.calls, "G"));
    CHECK(state.recenterSpace != state.referenceSpace);
    unsigned rendering = (unsigned) (uintptr_t) state.referenceSpace;
    unsigned sampling = (unsigned) (uintptr_t) state.recenterSpace;
    CHECK(recenter.spaceTypes[rendering] == XR_REFERENCE_SPACE_TYPE_STAGE);
    CHECK(recenter.spaceTypes[sampling] == XR_REFERENCE_SPACE_TYPE_STAGE);
    const XrPosef identity = { .orientation.w = 1.f };
    CHECK(!memcmp(&recenter.spacePoses[rendering], &identity, sizeof(identity)));
    CHECK(!memcmp(&recenter.spacePoses[sampling], &identity, sizeof(identity)));
    unsigned spaces = recenter.referenceCreates;
    XrSpace original = state.referenceSpace;
    recenter.pendingChange = XR_REFERENCE_SPACE_TYPE_LOCAL;
    CHECK(openxrHeadsetPollEvents());
    CHECK(!recenterEvents && state.referenceSpace == original && recenter.referenceCreates == spaces);
    recenter.pendingChange = XR_REFERENCE_SPACE_TYPE_STAGE;
    CHECK(openxrHeadsetPollEvents());
    CHECK(recenterEvents == 1 && state.referenceSpace == original && recenter.referenceCreates == spaces);
    CHECK(recenter.gets == 1 && !recenter.heads && !recenter.sets);
    CHECK(openxrStopSession());
    CHECK(!state.monadoStage && !state.recenterSpace && !state.referenceSpaceType);
    CHECK(recenter.referenceCreates == recenter.referenceDestroys);
    CHECK(recenterCleanup());
  }
  return true;
}

static bool stageFallback(void) {
  for (unsigned failure = 0; failure < 10; failure++) {
    CHECK(stageStartFixture());
    switch (failure) {
      case 0: CHECK(lovrHeadsetSetMonadoStageUnmapped(false)); break;
      case 1: state.runtimeManifestSelected = false; break;
      case 2: lovrMonadoRecenterDestroy(&state.recenter); break;
      case 3: recenter.missingSymbol = true; break;
      case 4: recenter.wrongVersion = true; break;
      case 5: recenter.failConnect = true; break;
      case 6: recenter.getResult = MND_ERROR_OPERATION_FAILED; break;
      case 7: recenter.failStageAt = 1; break;
      case 8: recenter.failStageAt = 3; break;
      case 9: manifestFailureAt = 1; break;
    }
    CHECK(openxrHeadsetStart());
    CHECK(!state.monadoStage && !state.recenterSpace);
    CHECK(state.referenceSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT);
    CHECK(!recenter.heads && !recenter.sets);
    unsigned gets = recenter.gets, spaces = recenter.referenceCreates;
    XrSpace original = state.referenceSpace;
    recenter.missingSymbol = recenter.wrongVersion = recenter.failConnect = false;
    recenter.getResult = MND_SUCCESS;
    recenter.failStageAt = manifestFailureAt = 0;
    CHECK(openxrHeadsetGetRecenterStatus() == HEADSET_RECENTER_UNSUPPORTED);
    CHECK(!openxrHeadsetGetRecenterSpace());
    CHECK(!openxrHeadsetRecenter());
    recenter.pendingChange = XR_REFERENCE_SPACE_TYPE_LOCAL;
    CHECK(openxrHeadsetPollEvents());
    CHECK(!state.monadoStage && state.referenceSpace == original);
    CHECK(recenter.referenceCreates == spaces && recenter.gets == gets && !recenter.heads && !recenter.sets);
    CHECK(openxrStopSession());
    CHECK(recenter.referenceCreates == recenter.referenceDestroys);
    CHECK(recenterCleanup());
  }
  CHECK(stageStartFixture());
  recenter.getResult = MND_ERROR_OPERATION_FAILED;
  CHECK(openxrHeadsetStart() && !state.monadoStage);
  CHECK(openxrStopSession());
  recenter.getResult = MND_SUCCESS;
  CHECK(openxrHeadsetStart() && state.monadoStage);
  CHECK(openxrStopSession());
  return recenterCleanup();
}

int main(int argc, char** argv) {
  NativeTest tests[] = {
    { "openxr.connect.no-overlay", noOverlay },
    { "openxr.connect.probe", probe },
    { "openxr.connect.cleanup-retry", cleanupRetry },
    { "openxr.connect.failures", failures },
    { "openxr.connect.bindings-retry", bindingsRetry },
    { "openxr.connect.matching-manifest", matchingManifest },
    { "openxr.connect.stage-selection", stageSelection },
    { "openxr.connect.stage-fallback", stageFallback },
    { "openxr.connect.recenter-status", recenterStatus },
    { "openxr.connect.recenter-stage", recenterStage },
    { "openxr.connect.recenter-invalidation", recenterInvalidation },
    { "openxr.connect.recenter-failures", recenterFailures }
  };
  return nativeRunTests(argc, argv, tests, COUNTOF(tests));
}
