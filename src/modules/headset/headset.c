#include "headset/headset_ops.h"

static const HeadsetOps* const ops = &lovrHeadsetOpenXROps;

bool lovrHeadsetInit(HeadsetConfig* config) {
  return ops->HeadsetInit(config);
}

void lovrHeadsetDestroy(void) {
  ops->HeadsetDestroy();
}

bool lovrHeadsetConnect(void) {
  return ops->HeadsetConnect();
}

bool lovrHeadsetIsConnected(void) {
  return ops->HeadsetIsConnected();
}

const char* lovrHeadsetGetName(void) {
  return ops->HeadsetGetName();
}

const char* lovrHeadsetGetDriver(void) {
  return ops->HeadsetGetDriver();
}

void lovrHeadsetGetFeatures(HeadsetFeatures* features) {
  ops->HeadsetGetFeatures(features);
}

uint32_t lovrHeadsetGetLayerLimit(void) {
  return ops->HeadsetGetLayerLimit();
}

bool lovrHeadsetIsSeated(void) {
  return ops->HeadsetIsSeated();
}

bool lovrHeadsetStart(void) {
  return ops->HeadsetStart();
}

void lovrHeadsetStop(void) {
  ops->HeadsetStop();
}

bool lovrHeadsetIsActive(void) {
  return ops->HeadsetIsActive();
}

uint32_t lovrHeadsetGetSessionGeneration(void) {
  return ops->HeadsetGetSessionGeneration();
}

bool lovrHeadsetIsMainSessionVisible(void) {
  return ops->HeadsetIsMainSessionVisible();
}

bool lovrHeadsetIsVisible(bool* main) {
  return ops->HeadsetIsVisible(main);
}

bool lovrHeadsetIsFocused(void) {
  return ops->HeadsetIsFocused();
}

bool lovrHeadsetIsMounted(void) {
  return ops->HeadsetIsMounted();
}

bool lovrHeadsetPollEvents(void) {
  return ops->HeadsetPollEvents();
}

bool lovrHeadsetUpdate(void) {
  return ops->HeadsetUpdate();
}

void lovrHeadsetGetDisplayDimensions(uint32_t* width, uint32_t* height) {
  ops->HeadsetGetDisplayDimensions(width, height);
}

float* lovrHeadsetGetRefreshRates(uint32_t* count) {
  return ops->HeadsetGetRefreshRates(count);
}

float lovrHeadsetGetRefreshRate(void) {
  return ops->HeadsetGetRefreshRate();
}

bool lovrHeadsetSetRefreshRate(float refreshRate) {
  return ops->HeadsetSetRefreshRate(refreshRate);
}

void lovrHeadsetGetFoveation(FoveationLevel* level, bool* dynamic) {
  ops->HeadsetGetFoveation(level, dynamic);
}

bool lovrHeadsetSetFoveation(FoveationLevel level, bool dynamic) {
  return ops->HeadsetSetFoveation(level, dynamic);
}

bool lovrHeadsetIsHDR(void) {
  return ops->HeadsetIsHDR();
}

bool lovrHeadsetIsPassthroughSupported(PassthroughMode mode) {
  return ops->HeadsetIsPassthroughSupported(mode);
}

PassthroughMode lovrHeadsetGetPassthrough(void) {
  return ops->HeadsetGetPassthrough();
}

bool lovrHeadsetSetPassthrough(PassthroughMode mode) {
  return ops->HeadsetSetPassthrough(mode);
}

uint32_t lovrHeadsetGetViewCount(void) {
  return ops->HeadsetGetViewCount();
}

bool lovrHeadsetGetViewPose(uint32_t view, float* position, float* orientation) {
  return ops->HeadsetGetViewPose(view, position, orientation);
}

bool lovrHeadsetGetViewAngles(uint32_t view, float* left, float* right, float* up, float* down) {
  return ops->HeadsetGetViewAngles(view, left, right, up, down);
}

void lovrHeadsetGetClipDistance(float* clipNear, float* clipFar) {
  ops->HeadsetGetClipDistance(clipNear, clipFar);
}

void lovrHeadsetSetClipDistance(float clipNear, float clipFar) {
  ops->HeadsetSetClipDistance(clipNear, clipFar);
}

void lovrHeadsetGetBoundsDimensions(float* width, float* height) {
  ops->HeadsetGetBoundsDimensions(width, height);
}

bool lovrHeadsetGetHandPose(Side side, HandPose type, float* position, float* orientation) {
  return ops->HeadsetGetHandPose(side, type, position, orientation);
}

bool lovrHeadsetGetPose(Device device, float* position, float* orientation) {
  return ops->HeadsetGetPose(device, position, orientation);
}

bool lovrHeadsetGetVelocity(Device device, float* velocity, float* angularVelocity) {
  return ops->HeadsetGetVelocity(device, velocity, angularVelocity);
}

bool lovrHeadsetIsDown(Device device, DeviceButton button, bool* down, bool* changed) {
  return ops->HeadsetIsDown(device, button, down, changed);
}

bool lovrHeadsetIsTouched(Device device, DeviceButton button, bool* touched) {
  return ops->HeadsetIsTouched(device, button, touched);
}

bool lovrHeadsetGetAxis(Device device, DeviceAxis axis, float* value) {
  return ops->HeadsetGetAxis(device, axis, value);
}

bool lovrHeadsetGetSkeleton(Device device, float* poses, SkeletonSource* source) {
  return ops->HeadsetGetSkeleton(device, poses, source);
}

bool lovrHeadsetGetBattery(Device device, float* level, bool* charging) {
  return ops->HeadsetGetBattery(device, level, charging);
}

bool lovrHeadsetVibrate(Device device, float strength, float duration, float frequency) {
  return ops->HeadsetVibrate(device, strength, duration, frequency);
}

void lovrHeadsetStopVibration(Device device) {
  ops->HeadsetStopVibration(device);
}

uint64_t* lovrHeadsetGetModelKeys(uint32_t* count) {
  return ops->HeadsetGetModelKeys(count);
}

struct ModelData* lovrHeadsetNewModelData(uint64_t key) {
  return ops->HeadsetNewModelData(key);
}

bool lovrHeadsetGetModelPose(struct Model* model, float* position, float* orientation) {
  return ops->HeadsetGetModelPose(model, position, orientation);
}

bool lovrHeadsetAnimate(struct Model* model) {
  return ops->HeadsetAnimate(model);
}

struct Texture* lovrHeadsetSetBackground(uint32_t width, uint32_t height, uint32_t layers) {
  return ops->HeadsetSetBackground(width, height, layers);
}

Layer** lovrHeadsetGetLayers(uint32_t* count, bool* main) {
  return ops->HeadsetGetLayers(count, main);
}

bool lovrHeadsetSetLayers(Layer** layers, uint32_t count, bool main) {
  return ops->HeadsetSetLayers(layers, count, main);
}

bool lovrHeadsetGetTexture(struct Texture** texture) {
  return ops->HeadsetGetTexture(texture);
}

bool lovrHeadsetGetPass(struct Pass** pass) {
  return ops->HeadsetGetPass(pass);
}

bool lovrHeadsetSubmit(void) {
  return ops->HeadsetSubmit();
}

void lovrHeadsetSetPose(Device device, float* position, float* orientation) {
  ops->HeadsetSetPose(device, position, orientation);
}

void lovrHeadsetSetButton(Device device, DeviceButton button, bool down) {
  ops->HeadsetSetButton(device, button, down);
}

Layer* lovrLayerCreate(const LayerInfo* info) {
  return ops->LayerCreate(info);
}

void lovrLayerDestroy(void* ref) {
  ops->LayerDestroy(ref);
}

Device lovrLayerGetOrigin(Layer* layer) {
  return ops->LayerGetOrigin(layer);
}

void lovrLayerSetOrigin(Layer* layer, Device device) {
  ops->LayerSetOrigin(layer, device);
}

void lovrLayerGetPose(Layer* layer, float* position, float* orientation) {
  ops->LayerGetPose(layer, position, orientation);
}

void lovrLayerSetPose(Layer* layer, float* position, float* orientation) {
  ops->LayerSetPose(layer, position, orientation);
}

void lovrLayerGetDimensions(Layer* layer, float* width, float* height) {
  ops->LayerGetDimensions(layer, width, height);
}

void lovrLayerSetDimensions(Layer* layer, float width, float height) {
  ops->LayerSetDimensions(layer, width, height);
}

float lovrLayerGetCurve(Layer* layer) {
  return ops->LayerGetCurve(layer);
}

bool lovrLayerSetCurve(Layer* layer, float curve) {
  return ops->LayerSetCurve(layer, curve);
}

void lovrLayerGetColor(Layer* layer, float color[4]) {
  ops->LayerGetColor(layer, color);
}

void lovrLayerSetColor(Layer* layer, float color[4]) {
  ops->LayerSetColor(layer, color);
}

void lovrLayerGetViewport(Layer* layer, int32_t* viewport) {
  ops->LayerGetViewport(layer, viewport);
}

void lovrLayerSetViewport(Layer* layer, int32_t* viewport) {
  ops->LayerSetViewport(layer, viewport);
}

struct Texture* lovrLayerGetTexture(Layer* layer) {
  return ops->LayerGetTexture(layer);
}

struct Pass* lovrLayerGetPass(Layer* layer) {
  return ops->LayerGetPass(layer);
}

void lovrHeadsetGetVulkanPhysicalDevice(void* instance, uintptr_t physicalDevice) {
  ops->HeadsetGetVulkanPhysicalDevice(instance, physicalDevice);
}

uint32_t lovrHeadsetCreateVulkanInstance(void* instanceCreateInfo, void* allocator, uintptr_t instance, void* getInstanceProcAddr) {
  return ops->HeadsetCreateVulkanInstance(instanceCreateInfo, allocator, instance, getInstanceProcAddr);
}

uint32_t lovrHeadsetCreateVulkanDevice(void* instance, void* deviceCreateInfo, void* allocatoor, uintptr_t device, void* getInstanceProcAddr) {
  return ops->HeadsetCreateVulkanDevice(instance, deviceCreateInfo, allocatoor, device, getInstanceProcAddr);
}

double lovrHeadsetGetDisplayTime(void) {
  return ops->HeadsetGetDisplayTime();
}

double lovrHeadsetGetDisplayPeriod(void) {
  return ops->HeadsetGetDisplayPeriod();
}

double lovrHeadsetGetDeltaTime(void) {
  return ops->HeadsetGetDeltaTime();
}

bool lovrHeadsetGetDepthTexture(struct Texture** texture) {
  return ops->HeadsetGetDepthTexture(texture);
}
