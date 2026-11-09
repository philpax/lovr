// Changes the runtime's shared tracking origin without changing its floor plane.
#include "headset/openvr_recenter.h"
#include "util.h"
#include <math.h>
#include <stdlib.h>

static HeadsetRecenterStatus inspect(const OpenVRRuntime* runtime, ETrackingUniverseOrigin origin, TrackedDevicePose_t* head, HmdMatrix34_t* zero);
static bool liveZeroPose(const OpenVRRuntime* runtime, ETrackingUniverseOrigin origin, HmdMatrix34_t* zero);
static bool sameMatrix(const HmdMatrix34_t* a, const HmdMatrix34_t* b);
static bool validMatrix(const HmdMatrix34_t* matrix);
static bool level(const HmdMatrix34_t* matrix);
static bool calibrated(const OpenVRRuntime* runtime, ETrackingUniverseOrigin origin);
static bool fail(struct VR_IVRChaperoneSetup_FnTable* setup, const char* reason);

HeadsetRecenterStatus lovrOpenVRGetRecenterStatus(const OpenVRRuntime* runtime, ETrackingUniverseOrigin origin) {
  TrackedDevicePose_t head = { 0 };
  HmdMatrix34_t zero;
  return inspect(runtime, origin, &head, &zero);
}

bool lovrOpenVRRecenter(const OpenVRRuntime* runtime, ETrackingUniverseOrigin origin) {
  TrackedDevicePose_t head = { 0 };
  HmdMatrix34_t snapshot;
  HeadsetRecenterStatus status = inspect(runtime, origin, &head, &snapshot);
  if (status != HEADSET_RECENTER_AVAILABLE) {
    lovrSetError("openvr: recenter: reset is not available (status %d)", status);
    return false;
  }

  struct VR_IVRChaperoneSetup_FnTable* setup = runtime->chaperoneSetup;
  // ResetZeroPose resets position and yaw, but does not guarantee the old vertical origin.
  // Editing the selected live zero pose preserves height in seated and standing spaces.
  setup->RevertWorkingCopy();
  HmdMatrix34_t before;
  bool seated = origin == ETrackingUniverseOrigin_TrackingUniverseSeated;
  bool read = seated ? setup->GetWorkingSeatedZeroPoseToRawTrackingPose(&before) :
    setup->GetWorkingStandingZeroPoseToRawTrackingPose(&before);
  if (!read || !validMatrix(&before) || !level(&before)) return fail(setup, "could not read a valid level zero pose");
  if (!sameMatrix(&before, &snapshot)) return fail(setup, "zero pose changed while sampling");

  const HmdMatrix34_t* pose = &head.mDeviceToAbsoluteTracking;
  float length = hypotf(pose->m[0][2], pose->m[2][2]);
  if (length < 1e-4f) return fail(setup, "head direction has no horizontal heading");
  float c = pose->m[2][2] / length, s = pose->m[0][2] / length;
  float x = pose->m[0][3], z = pose->m[2][3];
  HmdMatrix34_t after = before;
  for (unsigned i = 0; i < 3; i++) {
    after.m[i][0] = before.m[i][0] * c - before.m[i][2] * s;
    after.m[i][2] = before.m[i][0] * s + before.m[i][2] * c;
    after.m[i][3] = before.m[i][3] + before.m[i][0] * x + before.m[i][2] * z;
  }
  after.m[1][3] = before.m[1][3];
  if (!validMatrix(&after)) return fail(setup, "new zero pose is invalid");

  HmdQuad_t* bounds = NULL;
  uint32_t count = 0;
  if (!seated) {
    // Collision walls stay at their physical locations when the standing origin moves.
    if (!setup->GetWorkingCollisionBoundsInfo(NULL, &count)) return fail(setup, "could not read collision bounds size");
    if (count) {
      size_t bytes = (size_t) count * sizeof(*bounds);
      if (bytes / count != sizeof(*bounds) || !(bounds = malloc(bytes))) return fail(setup, "could not allocate collision bounds");
      uint32_t capacity = count;
      if (!setup->GetWorkingCollisionBoundsInfo(bounds, &count) || count > capacity) {
        free(bounds);
        return fail(setup, "could not read collision bounds");
      }
      for (uint32_t i = 0; i < count; i++) {
        for (unsigned j = 0; j < 4; j++) {
          float* point = bounds[i].vCorners[j].v;
          float dx = point[0] - x, dz = point[2] - z;
          point[0] = c * dx - s * dz;
          point[2] = s * dx + c * dz;
          if (!isfinite(point[0]) || !isfinite(point[1]) || !isfinite(point[2])) {
            free(bounds);
            return fail(setup, "collision bounds are invalid");
          }
        }
      }
    }
  }

  if (!calibrated(runtime, origin)) {
    free(bounds);
    return fail(setup, "calibration is no longer available");
  }
  if (seated) {
    setup->SetWorkingSeatedZeroPoseToRawTrackingPose(&after);
  } else {
    setup->SetWorkingStandingZeroPoseToRawTrackingPose(&after);
    setup->SetWorkingCollisionBoundsInfo(bounds, count);
  }
  free(bounds);
  HmdMatrix34_t current;
  if (!calibrated(runtime, origin) || !liveZeroPose(runtime, origin, &current) || !sameMatrix(&current, &snapshot)) {
    return fail(setup, "calibration changed before commit");
  }
  if (!setup->CommitWorkingCopy(EChaperoneConfigFile_Live)) return fail(setup, "live calibration commit failed");
  return true;
}

static HeadsetRecenterStatus inspect(const OpenVRRuntime* runtime, ETrackingUniverseOrigin origin, TrackedDevicePose_t* head, HmdMatrix34_t* zero) {
  if (!runtime || !runtime->initialized || !runtime->system) return HEADSET_RECENTER_UNAVAILABLE;
  bool seated = origin == ETrackingUniverseOrigin_TrackingUniverseSeated;
  if (!seated && origin != ETrackingUniverseOrigin_TrackingUniverseStanding) return HEADSET_RECENTER_UNSUPPORTED;
  struct VR_IVRChaperoneSetup_FnTable* setup = runtime->chaperoneSetup;
  if (!runtime->chaperone || !runtime->chaperone->GetCalibrationState || !setup ||
      !setup->RevertWorkingCopy || !setup->CommitWorkingCopy || !runtime->system->GetDeviceToAbsoluteTrackingPose ||
      (seated && (!setup->GetLiveSeatedZeroPoseToRawTrackingPose || !setup->GetWorkingSeatedZeroPoseToRawTrackingPose ||
        !setup->SetWorkingSeatedZeroPoseToRawTrackingPose)) ||
      (!seated && (!runtime->system->GetRawZeroPoseToStandingAbsoluteTrackingPose ||
        !setup->GetWorkingStandingZeroPoseToRawTrackingPose || !setup->SetWorkingStandingZeroPoseToRawTrackingPose ||
        !setup->GetWorkingCollisionBoundsInfo || !setup->SetWorkingCollisionBoundsInfo))) return HEADSET_RECENTER_UNSUPPORTED;
  if (!calibrated(runtime, origin) || !liveZeroPose(runtime, origin, zero)) return HEADSET_RECENTER_UNAVAILABLE;
  runtime->system->GetDeviceToAbsoluteTrackingPose(origin, 0.f, head, 1);
  HmdMatrix34_t current;
  if (!calibrated(runtime, origin) || !liveZeroPose(runtime, origin, &current) || !sameMatrix(zero, &current)) {
    return HEADSET_RECENTER_UNAVAILABLE;
  }
  if (!head->bDeviceIsConnected || !head->bPoseIsValid || head->eTrackingResult != ETrackingResult_TrackingResult_Running_OK ||
      !validMatrix(&head->mDeviceToAbsoluteTracking)) return HEADSET_RECENTER_UNTRACKED;
  if (hypotf(head->mDeviceToAbsoluteTracking.m[0][2], head->mDeviceToAbsoluteTracking.m[2][2]) < 1e-4f) return HEADSET_RECENTER_UNTRACKED;
  return HEADSET_RECENTER_AVAILABLE;
}

static bool liveZeroPose(const OpenVRRuntime* runtime, ETrackingUniverseOrigin origin, HmdMatrix34_t* zero) {
  if (origin == ETrackingUniverseOrigin_TrackingUniverseSeated) {
    if (!runtime->chaperoneSetup->GetLiveSeatedZeroPoseToRawTrackingPose(zero)) return false;
  } else {
    // The SDK exposes live raw-to-standing, not live standing-to-raw.
    HmdMatrix34_t raw = runtime->system->GetRawZeroPoseToStandingAbsoluteTrackingPose();
    if (!validMatrix(&raw)) return false;
    for (unsigned i = 0; i < 3; i++) {
      zero->m[i][3] = 0.f;
      for (unsigned j = 0; j < 3; j++) {
        zero->m[i][j] = raw.m[j][i];
        zero->m[i][3] -= raw.m[j][i] * raw.m[j][3];
      }
    }
  }
  return validMatrix(zero) && level(zero);
}

static bool sameMatrix(const HmdMatrix34_t* a, const HmdMatrix34_t* b) {
  for (unsigned i = 0; i < 3; i++) {
    for (unsigned j = 0; j < 4; j++) if (fabsf(a->m[i][j] - b->m[i][j]) > 1e-5f) return false;
  }
  return true;
}

static bool level(const HmdMatrix34_t* matrix) {
  // A tilted origin cannot preserve the floor under a horizontal translation.
  return fabsf(matrix->m[1][0]) <= 1e-5f && fabsf(matrix->m[1][2]) <= 1e-5f && fabsf(matrix->m[1][1] - 1.f) <= 1e-5f;
}

static bool validMatrix(const HmdMatrix34_t* matrix) {
  for (unsigned i = 0; i < 3; i++) {
    for (unsigned j = 0; j < 4; j++) if (!isfinite(matrix->m[i][j])) return false;
    for (unsigned j = i; j < 3; j++) {
      float dot = 0.f;
      for (unsigned k = 0; k < 3; k++) dot += matrix->m[k][i] * matrix->m[k][j];
      if (fabsf(dot - (i == j ? 1.f : 0.f)) > 1e-3f) return false;
    }
  }
  float determinant = matrix->m[0][0] * (matrix->m[1][1] * matrix->m[2][2] - matrix->m[1][2] * matrix->m[2][1])
    - matrix->m[0][1] * (matrix->m[1][0] * matrix->m[2][2] - matrix->m[1][2] * matrix->m[2][0])
    + matrix->m[0][2] * (matrix->m[1][0] * matrix->m[2][1] - matrix->m[1][1] * matrix->m[2][0]);
  return fabsf(determinant - 1.f) < 1e-3f;
}

static bool calibrated(const OpenVRRuntime* runtime, ETrackingUniverseOrigin origin) {
  ChaperoneCalibrationState calibration = runtime->chaperone->GetCalibrationState();
  return calibration >= ChaperoneCalibrationState_OK && calibration < ChaperoneCalibrationState_Error &&
    !(origin == ETrackingUniverseOrigin_TrackingUniverseSeated && calibration == ChaperoneCalibrationState_Warning_SeatedBoundsInvalid);
}

static bool fail(struct VR_IVRChaperoneSetup_FnTable* setup, const char* reason) {
  setup->RevertWorkingCopy();
  lovrSetError("openvr: recenter: %s", reason);
  return false;
}
