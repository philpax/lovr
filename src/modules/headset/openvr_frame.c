#include "openvr_frame.h"
#include "core/maf.h"
#include <math.h>
#include <string.h>

static bool validMatrix(const HmdMatrix34_t* matrix);
static bool makePose(OpenVRFramePose* pose, const HmdMatrix34_t* matrix);
static HmdMatrix34_t compose(const HmdMatrix34_t* head, const HmdMatrix34_t* eye);
static bool validTangents(const float* tangents);
static void updateClock(OpenVRFrame* frame, OpenVRFrameSnapshot* snapshot);

void lovrOpenVRFrameInit(OpenVRFrame* frame, struct VR_IVRSystem_FnTable* system,
    struct VR_IVROverlay_FnTable* overlay) {
  if (!frame) return;
  memset(frame, 0, sizeof(*frame));
  frame->system = system;
  frame->overlay = overlay;
}

OpenVRFrameResult lovrOpenVRFrameUpdate(OpenVRFrame* frame, ETrackingUniverseOrigin origin,
    OpenVRFrameClock clock, void* context) {
  OpenVRFrameResult result = { OPENVR_FRAME_INVALID, EVROverlayError_VROverlayError_None };
  if (!frame) return result;
  struct VR_IVRSystem_FnTable* system = frame->system;
  if (!system || !frame->overlay || !frame->overlay->WaitFrameSync ||
      !system->GetFloatTrackedDeviceProperty || !system->GetTimeSinceLastVsync ||
      !system->GetRecommendedRenderTargetSize || !system->GetDeviceToAbsoluteTrackingPose ||
      !system->GetEyeToHeadTransform || !system->GetProjectionRaw || !clock ||
      (origin != ETrackingUniverseOrigin_TrackingUniverseSeated &&
       origin != ETrackingUniverseOrigin_TrackingUniverseStanding &&
       origin != ETrackingUniverseOrigin_TrackingUniverseRawAndUncalibrated)) {
    memset(&frame->snapshot, 0, sizeof(frame->snapshot));
    return result;
  }

  if (frame->serial == UINT64_MAX) {
    memset(&frame->snapshot, 0, sizeof(frame->snapshot));
    return result;
  }
  OpenVRFrameSnapshot snapshot = { .origin = origin, .serial = ++frame->serial };
  ETrackedPropertyError error = ETrackedPropertyError_TrackedProp_Success;
  float frequency = system->GetFloatTrackedDeviceProperty(k_unTrackedDeviceIndex_Hmd,
    ETrackedDeviceProperty_Prop_DisplayFrequency_Float, &error);
  bool frequencyValid = error == ETrackedPropertyError_TrackedProp_Success &&
    isfinite(frequency) && frequency >= 1.f && frequency <= 1000.f;
  uint32_t timeout = frequencyValid ? (uint32_t) ceilf(2000.f / frequency) : 20;
  if (timeout > 100) timeout = 100;
  if (timeout < 1) timeout = 1;
  result.error = frame->overlay->WaitFrameSync(timeout);
  double now = clock(context);
  double clockResolution = nextafter(now, INFINITY) - now;
  if (!isfinite(now) || now < 0. || !isfinite(clockResolution) || clockResolution > 1e-6 ||
      (frame->updated && now < frame->lastNow)) {
    memset(&frame->snapshot, 0, sizeof(frame->snapshot));
    if (result.error != EVROverlayError_VROverlayError_None && result.error != EVROverlayError_VROverlayError_TimedOut) {
      result.status = OPENVR_FRAME_RUNTIME_ERROR;
    }
    return result;
  }
  snapshot.time = now;
  snapshot.displayPeriod = frequencyValid ? 1. / frequency : frame->lastDisplayPeriod;
  double previousNow = frame->lastNow;
  bool previouslyUpdated = frame->updated;
  frame->lastNow = now;
  frame->updated = true;
  if (result.error != EVROverlayError_VROverlayError_None && result.error != EVROverlayError_VROverlayError_TimedOut) {
    updateClock(frame, &snapshot);
    frame->snapshot = snapshot;
    result.status = OPENVR_FRAME_RUNTIME_ERROR;
    return result;
  }

  float photons = system->GetFloatTrackedDeviceProperty(k_unTrackedDeviceIndex_Hmd,
    ETrackedDeviceProperty_Prop_SecondsFromVsyncToPhotons_Float, &error);
  float vsync = 0.f;
  uint64_t counter = 0;
  bool timing = system->GetTimeSinceLastVsync(&vsync, &counter);
  system->GetRecommendedRenderTargetSize(&snapshot.width, &snapshot.height);
  if (!frequencyValid || error != ETrackedPropertyError_TrackedProp_Success || !timing ||
      !isfinite(photons) || photons < 0.f || photons > 1.f || !isfinite(vsync) || vsync < 0.f ||
      snapshot.width == 0 || snapshot.height == 0) {
    snapshot.width = snapshot.height = 0;
    updateClock(frame, &snapshot);
    frame->snapshot = snapshot;
    return result;
  }
  snapshot.frequency = frequency;
  float period = 1.f / frequency;
  snapshot.prediction = period - fmodf(vsync, period) + photons;
  snapshot.time = now + snapshot.prediction;
  if (!isfinite(snapshot.time) || snapshot.time <= now || snapshot.time - snapshot.displayPeriod == snapshot.time) {
    frame->lastNow = previousNow;
    frame->updated = previouslyUpdated;
    memset(&frame->snapshot, 0, sizeof(frame->snapshot));
    return result;
  }
  snapshot.displayTime = frame->hasEpoch ? snapshot.time - frame->epoch : snapshot.displayPeriod;
  if (frame->hasEpoch && snapshot.displayTime < frame->lastDisplayTime) {
    snapshot.displayTime = 0.;
    frame->snapshot = snapshot;
    return result;
  }
  TrackedDevicePose_t head = { 0 };
  system->GetDeviceToAbsoluteTrackingPose(origin, snapshot.prediction, &head, 1);
  if (head.bPoseIsValid && head.bDeviceIsConnected) {
    makePose(&snapshot.head, &head.mDeviceToAbsoluteTracking);
  }
  if (snapshot.head.valid) {
    snapshot.velocityValid = true;
    for (unsigned i = 0; i < 3; i++) {
      snapshot.velocityValid &= isfinite(head.vVelocity.v[i]) && isfinite(head.vAngularVelocity.v[i]);
    }
    if (snapshot.velocityValid) {
      memcpy(snapshot.velocity, head.vVelocity.v, sizeof(snapshot.velocity));
      memcpy(snapshot.angularVelocity, head.vAngularVelocity.v, sizeof(snapshot.angularVelocity));
    }
  }
  for (unsigned i = 0; i < 2; i++) {
    OpenVRFrameEye* eye = &snapshot.eyes[i];
    system->GetProjectionRaw((EVREye) i, &eye->tangents[0], &eye->tangents[1], &eye->tangents[2], &eye->tangents[3]);
    HmdMatrix34_t offset = system->GetEyeToHeadTransform((EVREye) i);
    if (snapshot.head.valid && validMatrix(&offset) && validTangents(eye->tangents)) {
      HmdMatrix34_t transform = compose(&head.mDeviceToAbsoluteTracking, &offset);
      if (makePose(&eye->pose, &transform)) eye->transform = transform;
    }
  }
  updateClock(frame, &snapshot);
  frame->snapshot = snapshot;
  result.status = result.error == EVROverlayError_VROverlayError_TimedOut ? OPENVR_FRAME_TIMEOUT : OPENVR_FRAME_OK;
  return result;
}

bool lovrOpenVRFrameProjection(const float tangents[4], float near, float far, float matrix[16]) {
  if (!matrix || !tangents || !validTangents(tangents) || !isfinite(near) || near <= 0.f ||
      !isfinite(far) || (far != 0.f && far <= near)) return false;
  double left = tangents[0], right = tangents[1], top = tangents[2], bottom = tangents[3];
  float projection[16] = { 0 };
  projection[0] = 2. / (right - left);
  projection[5] = 2. / (top - bottom);
  projection[8] = (right + left) / (right - left);
  projection[9] = (top + bottom) / (top - bottom);
  projection[10] = far == 0.f ? 0.f : (double) near / (far - (double) near);
  projection[11] = -1.f;
  projection[14] = far == 0.f ? near : (double) near * far / (far - (double) near);
  for (unsigned i = 0; i < 16; i++) if (!isfinite(projection[i])) return false;
  if (projection[0] <= 0.f || projection[5] >= 0.f || projection[14] <= 0.f ||
      (far != 0.f && projection[10] <= 0.f)) return false;
  memcpy(matrix, projection, sizeof(projection));
  return true;
}

static void updateClock(OpenVRFrame* frame, OpenVRFrameSnapshot* snapshot) {
  if (snapshot->prediction <= 0.f) return;
  if (!frame->hasEpoch) {
    frame->epoch = snapshot->time - snapshot->displayPeriod;
    frame->hasEpoch = true;
  }
  snapshot->displayTime = snapshot->time - frame->epoch;
  snapshot->delta = fmax(0., snapshot->displayTime - frame->lastDisplayTime);
  frame->lastDisplayTime = snapshot->displayTime;
  frame->lastDisplayPeriod = snapshot->displayPeriod;
}

static bool validMatrix(const HmdMatrix34_t* matrix) {
  const float (*m)[4] = matrix->m;
  for (unsigned row = 0; row < 3; row++) {
    for (unsigned col = 0; col < 4; col++) if (!isfinite(m[row][col])) return false;
    for (unsigned other = 0; other < 3; other++) {
      float dot = 0.f;
      for (unsigned col = 0; col < 3; col++) dot += m[row][col] * m[other][col];
      if (!isfinite(dot) || fabsf(dot - (row == other ? 1.f : 0.f)) > 1e-3f) return false;
    }
  }
  float determinant = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
    m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
    m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
  return isfinite(determinant) && fabsf(determinant - 1.f) <= 1e-3f;
}

static bool makePose(OpenVRFramePose* pose, const HmdMatrix34_t* matrix) {
  if (!validMatrix(matrix)) return false;
  for (unsigned row = 0; row < 3; row++) {
    for (unsigned col = 0; col < 4; col++) pose->matrix[col * 4 + row] = matrix->m[row][col];
  }
  pose->matrix[15] = 1.f;
  memcpy(pose->position, pose->matrix + 12, sizeof(pose->position));
  quat_fromMat4(pose->orientation, pose->matrix);
  pose->valid = true;
  return true;
}

static HmdMatrix34_t compose(const HmdMatrix34_t* head, const HmdMatrix34_t* eye) {
  HmdMatrix34_t result = { 0 };
  for (unsigned row = 0; row < 3; row++) {
    for (unsigned col = 0; col < 4; col++) {
      double value = col == 3 ? head->m[row][3] : 0.;
      for (unsigned k = 0; k < 3; k++) value += (double) head->m[row][k] * eye->m[k][col];
      result.m[row][col] = value;
    }
  }
  return result;
}

static bool validTangents(const float* tangents) {
  for (unsigned i = 0; i < 4; i++) if (!isfinite(tangents[i])) return false;
  return tangents[0] < tangents[1] && tangents[2] < tangents[3];
}
