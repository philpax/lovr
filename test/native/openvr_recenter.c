#include "test.h"
#include "headset/openvr_recenter.h"
#include "util.h"
#include <math.h>
#include <stdlib.h>

static struct {
  OpenVRRuntime runtime;
  struct VR_IVRSystem_FnTable system;
  struct VR_IVRChaperone_FnTable chaperone;
  struct VR_IVRChaperoneSetup_FnTable setup;
  TrackedDevicePose_t head;
  HmdMatrix34_t live[2], working[2];
  HmdQuad_t bounds, workingBounds;
  ChaperoneCalibrationState calibration;
  ETrackingUniverseOrigin origin;
  unsigned reverts, gets[2], sets[2], commits, queries;
  bool read, liveRead, commit, boundsRead;
  enum { CHANGE_NONE, CHANGE_SAMPLE, CHANGE_REVERT, CHANGE_WORKING_READ, CHANGE_SET } change;
} fake;

static HmdMatrix34_t transform(float yaw, float x, float y, float z) {
  float c = cosf(yaw), s = sinf(yaw);
  return (HmdMatrix34_t) { .m = { { c, 0, s, x }, { 0, 1, 0, y }, { -s, 0, c, z } } };
}

static void changeOrigin(unsigned phase) {
  if (fake.change == phase) {
    fake.live[fake.origin == ETrackingUniverseOrigin_TrackingUniverseSeated ? 0 : 1] = transform(-.8f, 7.f, .2f, 9.f);
    fake.change = CHANGE_NONE;
  }
}

static void OPENVR_FNTABLE_CALLTYPE poses(ETrackingUniverseOrigin origin, float prediction, TrackedDevicePose_t* head, uint32_t count) {
  if (origin != fake.origin || prediction != 0.f || count != 1) abort();
  fake.queries++;
  *head = fake.head;
  changeOrigin(CHANGE_SAMPLE);
}
static bool OPENVR_FNTABLE_CALLTYPE liveSeated(HmdMatrix34_t* matrix) {
  *matrix = fake.live[0];
  return fake.liveRead;
}
static HmdMatrix34_t OPENVR_FNTABLE_CALLTYPE rawToStanding(void) {
  const HmdMatrix34_t* live = &fake.live[1];
  HmdMatrix34_t inverse = { 0 };
  for (unsigned i = 0; i < 3; i++) {
    for (unsigned j = 0; j < 3; j++) {
      inverse.m[i][j] = live->m[j][i];
      inverse.m[i][3] -= live->m[j][i] * live->m[j][3];
    }
  }
  return inverse;
}
static ChaperoneCalibrationState OPENVR_FNTABLE_CALLTYPE calibration(void) { return fake.calibration; }
static void OPENVR_FNTABLE_CALLTYPE revert(void) {
  fake.reverts++;
  changeOrigin(CHANGE_REVERT);
  memcpy(fake.working, fake.live, sizeof(fake.live));
  fake.workingBounds = fake.bounds;
}
static bool OPENVR_FNTABLE_CALLTYPE getSeated(HmdMatrix34_t* matrix) {
  fake.gets[0]++; *matrix = fake.working[0]; changeOrigin(CHANGE_WORKING_READ); return fake.read;
}
static bool OPENVR_FNTABLE_CALLTYPE getStanding(HmdMatrix34_t* matrix) {
  fake.gets[1]++; *matrix = fake.working[1]; changeOrigin(CHANGE_WORKING_READ); return fake.read;
}
static void OPENVR_FNTABLE_CALLTYPE setSeated(HmdMatrix34_t* matrix) { fake.sets[0]++; fake.working[0] = *matrix; changeOrigin(CHANGE_SET); }
static void OPENVR_FNTABLE_CALLTYPE setStanding(HmdMatrix34_t* matrix) { fake.sets[1]++; fake.working[1] = *matrix; changeOrigin(CHANGE_SET); }
static bool OPENVR_FNTABLE_CALLTYPE getBounds(HmdQuad_t* quads, uint32_t* count) {
  if (quads && *count >= 1) *quads = fake.workingBounds;
  *count = 1;
  return fake.boundsRead;
}
static void OPENVR_FNTABLE_CALLTYPE setBounds(HmdQuad_t* quads, uint32_t count) {
  if (count != 1) abort();
  fake.workingBounds = *quads;
}
static bool OPENVR_FNTABLE_CALLTYPE commit(EChaperoneConfigFile file) {
  if (file != EChaperoneConfigFile_Live) abort();
  fake.commits++;
  if (fake.commit) {
    memcpy(fake.live, fake.working, sizeof(fake.live));
    fake.bounds = fake.workingBounds;
  }
  return fake.commit;
}
static void reset(bool seated) {
  memset(&fake, 0, sizeof(fake));
  fake.origin = seated ? ETrackingUniverseOrigin_TrackingUniverseSeated : ETrackingUniverseOrigin_TrackingUniverseStanding;
  fake.system.GetDeviceToAbsoluteTrackingPose = poses;
  fake.system.GetRawZeroPoseToStandingAbsoluteTrackingPose = rawToStanding;
  fake.chaperone.GetCalibrationState = calibration;
  fake.setup = (struct VR_IVRChaperoneSetup_FnTable) {
    .RevertWorkingCopy = revert, .CommitWorkingCopy = commit,
    .GetLiveSeatedZeroPoseToRawTrackingPose = liveSeated,
    .GetWorkingSeatedZeroPoseToRawTrackingPose = getSeated,
    .GetWorkingStandingZeroPoseToRawTrackingPose = getStanding,
    .SetWorkingSeatedZeroPoseToRawTrackingPose = setSeated,
    .SetWorkingStandingZeroPoseToRawTrackingPose = setStanding,
    .GetWorkingCollisionBoundsInfo = getBounds, .SetWorkingCollisionBoundsInfo = setBounds
  };
  fake.runtime = (OpenVRRuntime) { .initialized = true, .system = &fake.system,
    .chaperone = &fake.chaperone, .chaperoneSetup = &fake.setup };
  fake.calibration = ChaperoneCalibrationState_OK;
  fake.read = fake.liveRead = fake.commit = fake.boundsRead = true;
  fake.head = (TrackedDevicePose_t) { .bPoseIsValid = true, .bDeviceIsConnected = true,
    .eTrackingResult = ETrackingResult_TrackingResult_Running_OK,
    .mDeviceToAbsoluteTracking = transform(.7f, 2.f, 1.6f, -3.f) };
  fake.live[0] = transform(-.4f, -3.f, 1.1f, 4.f);
  fake.live[1] = transform(1.2f, 5.f, -.3f, -6.f);
  for (unsigned i = 0; i < 4; i++) fake.bounds.vCorners[i] = (HmdVector3_t) { .v = { i & 1 ? 4.f : -4.f, i & 2 ? 2.f : 0.f, -5.f } };
}

static bool statusMatrix(void) {
  reset(false);
  CHECK(lovrOpenVRGetRecenterStatus(&fake.runtime, fake.origin) == HEADSET_RECENTER_AVAILABLE);
  CHECK(fake.reverts == 0 && fake.commits == 0 && fake.queries == 1);
  fake.runtime.initialized = false;
  CHECK(lovrOpenVRGetRecenterStatus(&fake.runtime, fake.origin) == HEADSET_RECENTER_UNAVAILABLE);
  fake.runtime.initialized = true;
  fake.runtime.chaperoneSetup = NULL;
  CHECK(lovrOpenVRGetRecenterStatus(&fake.runtime, fake.origin) == HEADSET_RECENTER_UNSUPPORTED);
  fake.runtime.chaperoneSetup = &fake.setup;
  fake.setup.CommitWorkingCopy = NULL;
  CHECK(lovrOpenVRGetRecenterStatus(&fake.runtime, fake.origin) == HEADSET_RECENTER_UNSUPPORTED);
  reset(false);
  fake.calibration = ChaperoneCalibrationState_Error_PlayAreaInvalid;
  CHECK(lovrOpenVRGetRecenterStatus(&fake.runtime, fake.origin) == HEADSET_RECENTER_UNAVAILABLE);
  reset(false);
  fake.head.bPoseIsValid = false;
  CHECK(lovrOpenVRGetRecenterStatus(&fake.runtime, fake.origin) == HEADSET_RECENTER_UNTRACKED);
  fake.head.bPoseIsValid = true;
  fake.head.bDeviceIsConnected = false;
  CHECK(lovrOpenVRGetRecenterStatus(&fake.runtime, fake.origin) == HEADSET_RECENTER_UNTRACKED);
  reset(false);
  fake.head.eTrackingResult = ETrackingResult_TrackingResult_Fallback_RotationOnly;
  CHECK(lovrOpenVRGetRecenterStatus(&fake.runtime, fake.origin) == HEADSET_RECENTER_UNTRACKED);
  reset(false);
  fake.head.mDeviceToAbsoluteTracking.m[0][3] = NAN;
  CHECK(lovrOpenVRGetRecenterStatus(&fake.runtime, fake.origin) == HEADSET_RECENTER_UNTRACKED);
  reset(false);
  fake.head.mDeviceToAbsoluteTracking = (HmdMatrix34_t) { .m = { { 1, 0, 0, 2 }, { 0, 0, -1, 1.7f }, { 0, 1, 0, 3 } } };
  CHECK(lovrOpenVRGetRecenterStatus(&fake.runtime, fake.origin) == HEADSET_RECENTER_UNTRACKED);
  reset(false);
  CHECK(lovrOpenVRGetRecenterStatus(&fake.runtime, ETrackingUniverseOrigin_TrackingUniverseRawAndUncalibrated) == HEADSET_RECENTER_UNSUPPORTED);
  for (unsigned seated = 0; seated < 2; seated++) {
    reset(seated);
    unsigned selected = seated ? 0 : 1;
    // The working copy is unrelated to status and must not be refreshed by a query.
    fake.working[selected] = transform(0.f, 0.f, 0.f, 0.f);
    fake.live[selected] = (HmdMatrix34_t) { .m = { { 1, 0, 0, 2 }, { 0, .8f, -.6f, .3f }, { 0, .6f, .8f, 4 } } };
    CHECK(lovrOpenVRGetRecenterStatus(&fake.runtime, fake.origin) == HEADSET_RECENTER_UNAVAILABLE);
    CHECK(fake.reverts == 0 && fake.commits == 0 && fake.gets[0] == 0 && fake.gets[1] == 0);
    CHECK(fake.sets[0] == 0 && fake.sets[1] == 0);
    fake.live[1 - selected] = fake.live[selected];
    fake.live[selected] = transform(.5f, 1.f, .2f, 3.f);
    CHECK(lovrOpenVRGetRecenterStatus(&fake.runtime, fake.origin) == HEADSET_RECENTER_AVAILABLE);
    fake.live[selected].m[0][0] = NAN;
    CHECK(lovrOpenVRGetRecenterStatus(&fake.runtime, fake.origin) == HEADSET_RECENTER_UNAVAILABLE);
    reset(seated);
    if (seated) fake.setup.GetLiveSeatedZeroPoseToRawTrackingPose = NULL;
    else fake.system.GetRawZeroPoseToStandingAbsoluteTrackingPose = NULL;
    CHECK(lovrOpenVRGetRecenterStatus(&fake.runtime, fake.origin) == HEADSET_RECENTER_UNSUPPORTED);
  }
  reset(true);
  fake.liveRead = false;
  CHECK(lovrOpenVRGetRecenterStatus(&fake.runtime, fake.origin) == HEADSET_RECENTER_UNAVAILABLE);
  return true;
}

static void point(const HmdMatrix34_t* t, const float p[3], float out[3]) {
  for (unsigned i = 0; i < 3; i++) out[i] = t->m[i][3] + t->m[i][0] * p[0] + t->m[i][1] * p[1] + t->m[i][2] * p[2];
}

static bool horizontalYawAndFloor(void) {
  for (unsigned seated = 0; seated < 2; seated++) {
    for (int sample = -10; sample <= 10; sample++) {
      reset(seated);
      float yaw = sample * .3f, pitch = sample * .08f, roll = sample * .05f;
      float c = cosf(yaw), s = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch), cr = cosf(roll), sr = sinf(roll);
      fake.head.mDeviceToAbsoluteTracking = (HmdMatrix34_t) { .m = {
        { c * cr + s * sp * sr, -c * sr + s * sp * cr, s * cp, sample * .2f },
        { cp * sr, cp * cr, -sp, 1.7f },
        { -s * cr + c * sp * sr, s * sr + c * sp * cr, c * cp, sample * -.4f }
      } };
      HmdMatrix34_t before = fake.live[seated ? 0 : 1], other = fake.live[seated ? 1 : 0];
      HmdQuad_t bounds = fake.bounds;
      float headRaw[3];
      float position[3] = { fake.head.mDeviceToAbsoluteTracking.m[0][3], 1.7f, fake.head.mDeviceToAbsoluteTracking.m[2][3] };
      point(&before, position, headRaw);
      CHECK(lovrOpenVRRecenter(&fake.runtime, fake.origin));
      HmdMatrix34_t after = fake.live[seated ? 0 : 1];
      CHECK(fake.commits == 1 && fake.sets[seated ? 0 : 1] == 1 && fake.sets[seated ? 1 : 0] == 0);
      CHECK(memcmp(&other, &fake.live[seated ? 1 : 0], sizeof(other)) == 0);
      CHECK(after.m[1][3] == before.m[1][3]);
      float centered[3] = { 0, 1.7f, 0 }, centeredRaw[3];
      point(&after, centered, centeredRaw);
      for (unsigned i = 0; i < 3; i++) CHECK(fabsf(centeredRaw[i] - headRaw[i]) < 1e-4f);
      for (unsigned i = 0; i < 3; i++) {
        float forward = before.m[i][0] * s + before.m[i][2] * c;
        CHECK(fabsf(after.m[i][2] - forward) < 1e-4f);
      }
      for (unsigned corner = 0; corner < 4; corner++) {
        float oldRaw[3], newRaw[3];
        point(&before, bounds.vCorners[corner].v, oldRaw);
        point(&after, fake.bounds.vCorners[corner].v, newRaw);
        if (!seated) for (unsigned i = 0; i < 3; i++) CHECK(fabsf(oldRaw[i] - newRaw[i]) < 1e-4f);
      }
      if (seated) CHECK(memcmp(&bounds, &fake.bounds, sizeof(bounds)) == 0);
    }
  }
  return true;
}

static bool failuresAreAtomic(void) {
  for (unsigned seated = 0; seated < 2; seated++) {
    for (unsigned phase = CHANGE_SAMPLE; phase <= CHANGE_SET; phase++) {
      reset(seated);
      HmdMatrix34_t other = fake.live[seated ? 1 : 0];
      HmdMatrix34_t changed = transform(-.8f, 7.f, .2f, 9.f);
      HmdQuad_t bounds = fake.bounds;
      fake.change = phase;
      CHECK(!lovrOpenVRRecenter(&fake.runtime, fake.origin));
      CHECK(fake.commits == 0);
      CHECK(memcmp(&changed, &fake.live[seated ? 0 : 1], sizeof(changed)) == 0);
      CHECK(memcmp(&other, &fake.live[seated ? 1 : 0], sizeof(other)) == 0);
      CHECK(memcmp(&bounds, &fake.bounds, sizeof(bounds)) == 0);
      CHECK(strstr(lovrGetError(), "openvr: recenter:") != NULL);
    }
  }
  for (unsigned seated = 0; seated < 2; seated++) {
    for (unsigned failure = 0; failure < 7; failure++) {
      reset(seated);
      HmdMatrix34_t live[2]; memcpy(live, fake.live, sizeof(live));
      HmdQuad_t bounds = fake.bounds;
      if (failure == 0) fake.read = false;
      if (failure == 1) fake.commit = false;
      if (failure == 2) fake.head.bPoseIsValid = false;
      if (failure == 3) fake.live[seated ? 0 : 1].m[0][0] = NAN;
      if (failure == 4) {
        fake.head.mDeviceToAbsoluteTracking = (HmdMatrix34_t) { .m = { { 1, 0, 0, 2 }, { 0, 0, -1, 1.7f }, { 0, 1, 0, 3 } } };
      }
      if (failure == 5) {
        if (seated) fake.calibration = ChaperoneCalibrationState_Warning_SeatedBoundsInvalid;
        else fake.boundsRead = false;
      }
      if (failure == 6) {
        HmdMatrix34_t* matrix = &fake.live[seated ? 0 : 1];
        *matrix = (HmdMatrix34_t) { .m = { { 1, 0, 0, 2 }, { 0, .8f, -.6f, .3f }, { 0, .6f, .8f, 4 } } };
        memcpy(live, fake.live, sizeof(live));
      }
      CHECK(!lovrOpenVRRecenter(&fake.runtime, fake.origin));
      CHECK(strstr(lovrGetError(), "openvr: recenter:") != NULL);
      if (failure != 3) CHECK(memcmp(live, fake.live, sizeof(live)) == 0);
      CHECK(memcmp(&bounds, &fake.bounds, sizeof(bounds)) == 0);
      CHECK(fake.commits == (failure == 1 ? 1u : 0u));
      if (failure == 1) CHECK(memcmp(fake.live, fake.working, sizeof(fake.live)) == 0);
    }
  }
  return true;
}

int main(int argc, char** argv) {
  const NativeTest tests[] = {
    { "status", statusMatrix },
    { "horizontal-yaw-floor-and-bounds", horizontalYawAndFloor },
    { "atomic-failures", failuresAreAtomic }
  };
  return nativeRunTests(argc, argv, tests, sizeof(tests) / sizeof(tests[0]));
}
