#include "test.h"
#include "headset/openvr_frame.h"
#include "core/maf.h"
#include <math.h>
#include <float.h>

static struct {
  unsigned waits, poses, reads, clocks;
  double waitAdvance;
  bool clockOrder;
  ETrackingUniverseOrigin origin;
  uint32_t timeout;
  EVROverlayError error;
  ETrackedPropertyError propertyError;
  float frequency, vsync, photons, prediction;
  bool timing;
  uint32_t width, height;
  TrackedDevicePose_t head;
  HmdMatrix34_t eyes[2];
  float tangents[2][4];
} fake;

static EVROverlayError OPENVR_FNTABLE_CALLTYPE waitFrame(uint32_t timeout) {
  fake.waits++;
  fake.timeout = timeout;
  fake.clockOrder = true;
  return fake.error;
}

static double clockNow(void* context) {
  fake.clocks++;
  if (!fake.clockOrder) return NAN;
  fake.clockOrder = false;
  return *(double*) context + fake.waitAdvance;
}

static OpenVRFrameResult updateAt(OpenVRFrame* frame, ETrackingUniverseOrigin origin, double now) {
  return lovrOpenVRFrameUpdate(frame, origin, clockNow, &now);
}

static float OPENVR_FNTABLE_CALLTYPE property(TrackedDeviceIndex_t device, ETrackedDeviceProperty prop,
    ETrackedPropertyError* error) {
  (void) device;
  fake.reads++;
  *error = fake.propertyError;
  return prop == ETrackedDeviceProperty_Prop_DisplayFrequency_Float ? fake.frequency : fake.photons;
}

static bool OPENVR_FNTABLE_CALLTYPE timing(float* seconds, uint64_t* counter) {
  *seconds = fake.vsync;
  *counter = 12;
  return fake.timing;
}

static void OPENVR_FNTABLE_CALLTYPE dimensions(uint32_t* width, uint32_t* height) {
  *width = fake.width;
  *height = fake.height;
}

static void OPENVR_FNTABLE_CALLTYPE poses(ETrackingUniverseOrigin origin, float prediction,
    TrackedDevicePose_t* output, uint32_t count) {
  fake.origin = origin;
  (void) count;
  fake.poses++;
  fake.prediction = prediction;
  output[0] = fake.head;
}

static HmdMatrix34_t OPENVR_FNTABLE_CALLTYPE eye(EVREye eye) {
  return fake.eyes[eye];
}

static void OPENVR_FNTABLE_CALLTYPE projection(EVREye eye, float* left, float* right, float* top, float* bottom) {
  *left = fake.tangents[eye][0];
  *right = fake.tangents[eye][1];
  *top = fake.tangents[eye][2];
  *bottom = fake.tangents[eye][3];
}

static struct VR_IVRSystem_FnTable system = {
  .GetFloatTrackedDeviceProperty = property,
  .GetTimeSinceLastVsync = timing,
  .GetRecommendedRenderTargetSize = dimensions,
  .GetDeviceToAbsoluteTrackingPose = poses,
  .GetEyeToHeadTransform = eye,
  .GetProjectionRaw = projection
};
static struct VR_IVROverlay_FnTable overlay = { .WaitFrameSync = waitFrame };

static OpenVRFrame setup(void) {
  memset(&fake, 0, sizeof(fake));
  fake.frequency = 100.f;
  fake.photons = .003f;
  fake.vsync = .002f;
  fake.timing = true;
  fake.width = 1200;
  fake.height = 1300;
  fake.head.bDeviceIsConnected = fake.head.bPoseIsValid = true;
  fake.head.eTrackingResult = ETrackingResult_TrackingResult_Running_OK;
  fake.head.mDeviceToAbsoluteTracking = (HmdMatrix34_t) { .m = { { 0, 0, 1, 2 }, { 0, 1, 0, 3 }, { -1, 0, 0, 4 } } };
  fake.head.vVelocity = (HmdVector3_t) { .v = { 5, 6, 7 } };
  fake.head.vAngularVelocity = (HmdVector3_t) { .v = { 8, 9, 10 } };
  fake.eyes[0] = (HmdMatrix34_t) { .m = { { 0, -1, 0, -.03f }, { 1, 0, 0, .01f }, { 0, 0, 1, .02f } } };
  fake.eyes[1] = fake.eyes[0];
  fake.eyes[1].m[0][3] = .04f;
  for (unsigned i = 0; i < 2; i++) {
    float tangents[4] = { -1.2f, .8f, -.7f, 1.1f };
    memcpy(fake.tangents[i], tangents, sizeof(tangents));
  }
  OpenVRFrame frame;
  lovrOpenVRFrameInit(&frame, &system, &overlay);
  return frame;
}

static bool closeTo(double a, double b) { return fabs(a - b) < 1e-5; }

static bool predictedPose(void) {
  OpenVRFrame frame = setup();
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 10.).status == OPENVR_FRAME_OK);
  CHECK(fake.waits == 1 && fake.clocks == 1 && fake.poses == 1 && fake.timeout > 0 && fake.timeout <= 100);
  CHECK(closeTo(fake.prediction, .011));
  CHECK(frame.snapshot.width == 1200 && frame.snapshot.height == 1300);
  CHECK(frame.snapshot.frequency == 100.f && closeTo(frame.snapshot.time, 10.011));
  CHECK(frame.snapshot.head.valid && frame.snapshot.eyes[0].pose.valid);
  CHECK(closeTo(frame.snapshot.eyes[0].pose.position[0], 2.02));
  CHECK(closeTo(frame.snapshot.eyes[0].pose.position[1], 3.01));
  CHECK(closeTo(frame.snapshot.eyes[0].pose.position[2], 4.03));
  CHECK(closeTo(frame.snapshot.eyes[1].pose.position[2], 3.96));
  CHECK(frame.snapshot.eyes[0].pose.matrix[1] == 1.f);
  CHECK(frame.snapshot.head.matrix[2] == -1.f);
  CHECK(closeTo(fabsf(frame.snapshot.eyes[0].pose.orientation[0]), .5));
  CHECK(closeTo(fabsf(frame.snapshot.eyes[0].pose.orientation[1]), .5));
  CHECK(closeTo(fabsf(frame.snapshot.eyes[0].pose.orientation[2]), .5));
  CHECK(closeTo(fabsf(frame.snapshot.eyes[0].pose.orientation[3]), .5));
  const float* quaternion = frame.snapshot.eyes[0].pose.orientation;
  CHECK(closeTo(fabsf(.5f * (quaternion[0] + quaternion[1] + quaternion[2] + quaternion[3])), 1.));
  CHECK(fake.origin == ETrackingUniverseOrigin_TrackingUniverseStanding);
  CHECK(frame.snapshot.velocityValid && frame.snapshot.velocity[1] == 6.f && frame.snapshot.angularVelocity[2] == 10.f);
  CHECK(memcmp(frame.snapshot.eyes[0].tangents, fake.tangents[0], sizeof(fake.tangents[0])) == 0);
  OpenVRFrameSnapshot snapshot = frame.snapshot;
  fake.head.mDeviceToAbsoluteTracking.m[0][3] = 100.f;
  CHECK(memcmp(&snapshot, &frame.snapshot, sizeof(snapshot)) == 0 && fake.poses == 1);
  return true;
}

static bool timeoutProgress(void) {
  OpenVRFrame frame = setup();
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 10.).status == OPENVR_FRAME_OK);
  fake.error = EVROverlayError_VROverlayError_TimedOut;
  double previous = frame.snapshot.time;
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 10.03).status == OPENVR_FRAME_TIMEOUT);
  CHECK(frame.snapshot.time > previous && closeTo(frame.snapshot.delta, .03));
  CHECK(fake.waits == 2 && fake.poses == 2);
  CHECK(frame.snapshot.serial == 2);
  OpenVRFrameSnapshot retained = frame.snapshot;
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseSeated, 10.035).status == OPENVR_FRAME_TIMEOUT);
  CHECK(frame.snapshot.serial == 3 && frame.snapshot.origin == ETrackingUniverseOrigin_TrackingUniverseSeated);
  CHECK(fake.origin == ETrackingUniverseOrigin_TrackingUniverseSeated);
  CHECK(retained.serial == 2 && retained.origin == ETrackingUniverseOrigin_TrackingUniverseStanding);
  fake.error = EVROverlayError_VROverlayError_RequestFailed;
  OpenVRFrameResult result = updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 10.04);
  CHECK(result.status == OPENVR_FRAME_RUNTIME_ERROR && result.error == fake.error);
  CHECK(!frame.snapshot.head.valid && fake.poses == 3);
  return true;
}

static bool sessionClock(void) {
  OpenVRFrame frame = setup();
  fake.waitAdvance = .05;
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 399.95).status == OPENVR_FRAME_OK);
  CHECK(closeTo(frame.snapshot.time, 400.011) && fake.clocks == 1);
  frame = setup();
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 400.).status == OPENVR_FRAME_OK);
  fake.vsync = .009f;
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 400.01).status == OPENVR_FRAME_OK);
  CHECK(closeTo(frame.snapshot.displayTime, .013) && closeTo(frame.snapshot.delta, .003));
  CHECK(!closeTo(frame.snapshot.delta, .01));
  frame = setup();
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 400.).status == OPENVR_FRAME_OK);
  CHECK(closeTo(frame.snapshot.displayTime, .01) && closeTo(frame.snapshot.displayPeriod, .01));
  CHECK(closeTo(frame.snapshot.delta, .01));
  fake.error = EVROverlayError_VROverlayError_TimedOut;
  fake.vsync = .009f;
  double previous = frame.lastDisplayTime;
  double epoch = frame.epoch;
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 400.001).status == OPENVR_FRAME_INVALID);
  CHECK(frame.snapshot.displayTime == 0. && frame.epoch == epoch);
  CHECK(frame.snapshot.delta == 0. && !frame.snapshot.head.valid && !frame.snapshot.eyes[0].pose.valid);
  CHECK(frame.lastDisplayTime == previous);
  fake.vsync = .008f;
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 400.001).status == OPENVR_FRAME_INVALID);
  CHECK(frame.snapshot.displayTime == 0. && frame.snapshot.delta == 0.);
  CHECK(frame.lastDisplayTime == previous && frame.epoch == epoch);
  for (unsigned i = 0; i < 20; i++) {
    fake.vsync = i % 2 == 0 ? .002f : .009f;
    OpenVRFrameResult result = updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 400.001);
    CHECK(frame.epoch == epoch && frame.snapshot.delta >= 0.);
    if (i % 2 == 0) {
      CHECK(result.status == OPENVR_FRAME_TIMEOUT && frame.snapshot.head.valid);
      CHECK(closeTo(frame.snapshot.displayTime, frame.snapshot.time - epoch));
      if (i > 0) CHECK(frame.snapshot.displayTime == previous && frame.snapshot.delta == 0.);
      previous = frame.snapshot.displayTime;
    } else {
      CHECK(result.status == OPENVR_FRAME_INVALID && frame.snapshot.delta == 0.);
      CHECK(frame.snapshot.displayTime == 0. && !frame.snapshot.head.valid && frame.lastDisplayTime == previous);
    }
  }
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 400.02).status == OPENVR_FRAME_TIMEOUT);
  CHECK(closeTo(frame.snapshot.displayTime, frame.snapshot.time - frame.epoch));
  previous = frame.snapshot.displayTime;
  fake.timing = false;
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 400.03).status == OPENVR_FRAME_INVALID);
  CHECK(frame.snapshot.displayTime == 0. && frame.snapshot.delta == 0.);
  CHECK(frame.lastDisplayTime == previous && frame.epoch == epoch);
  CHECK(!frame.snapshot.head.valid);
  fake.timing = true;
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 400.04).status == OPENVR_FRAME_TIMEOUT);
  CHECK(closeTo(frame.snapshot.displayTime, frame.snapshot.time - frame.epoch));
  CHECK(closeTo(frame.snapshot.delta, frame.snapshot.displayTime - previous));
  lovrOpenVRFrameInit(&frame, &system, &overlay);
  fake.timing = true;
  fake.error = EVROverlayError_VROverlayError_None;
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 900.).status == OPENVR_FRAME_OK);
  CHECK(closeTo(frame.snapshot.displayTime, .01) && closeTo(frame.snapshot.delta, .01));
  CHECK(frame.snapshot.serial == 1 && frame.snapshot.origin == ETrackingUniverseOrigin_TrackingUniverseStanding);
  lovrOpenVRFrameInit(&frame, &system, &overlay);
  fake.timing = false;
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 1000.).status == OPENVR_FRAME_INVALID);
  CHECK(!frame.hasEpoch && frame.snapshot.displayTime == 0. && !frame.snapshot.head.valid);
  fake.timing = true;
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 1000.01).status == OPENVR_FRAME_OK);
  CHECK(closeTo(frame.snapshot.displayTime, .01) && closeTo(frame.snapshot.delta, .01));
  return true;
}

static bool invalidInputs(void) {
  for (unsigned i = 0; i < 19; i++) {
    OpenVRFrame frame = setup();
    switch (i) {
      case 0: fake.frequency = NAN; break;
      case 1: fake.frequency = 0; break;
      case 2: fake.vsync = INFINITY; break;
      case 3: fake.vsync = -.1f; break;
      case 4: fake.photons = NAN; break;
      case 5: fake.propertyError = ETrackedPropertyError_TrackedProp_UnknownProperty; break;
      case 6: fake.timing = false; break;
      case 7: fake.width = 0; break;
      case 8: fake.head.bPoseIsValid = false; break;
      case 9: fake.head.bDeviceIsConnected = false; break;
      case 10: fake.head.mDeviceToAbsoluteTracking.m[0][0] = NAN; break;
      case 11: fake.head.mDeviceToAbsoluteTracking.m[0][0] = 2; break;
      case 12: fake.head.vVelocity.v[0] = INFINITY; break;
      case 13: fake.eyes[1].m[1][1] = 2; break;
      case 14: fake.eyes[1].m[0][3] = NAN; break;
      case 15: fake.tangents[1][2] = INFINITY; break;
      case 16: fake.tangents[1][0] = 2; break;
      case 17: fake.eyes[1].m[2][2] = -1; break;
      case 18: fake.eyes[1].m[0][3] = 3e38f; fake.head.mDeviceToAbsoluteTracking.m[2][3] = -3e38f; break;
    }
    OpenVRFrameResult result = updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 10.);
    CHECK(fake.waits == 1);
    if (i < 8) CHECK(result.status == OPENVR_FRAME_INVALID && !frame.snapshot.head.valid);
    else if (i < 12) CHECK(!frame.snapshot.head.valid && !frame.snapshot.eyes[0].pose.valid);
    else if (i == 12) CHECK(frame.snapshot.head.valid && !frame.snapshot.velocityValid && frame.snapshot.velocity[0] == 0);
    else CHECK(frame.snapshot.head.valid && !frame.snapshot.eyes[1].pose.valid);
  }
  OpenVRFrame frame = setup();
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, NAN).status == OPENVR_FRAME_INVALID);
  CHECK(fake.waits == 1 && fake.clocks == 1);
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 10.).status == OPENVR_FRAME_OK);
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 9.).status == OPENVR_FRAME_INVALID);
  CHECK(!frame.snapshot.head.valid);
  frame = setup();
  fake.vsync = 120.f;
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 10.).status == OPENVR_FRAME_OK);
  CHECK(frame.snapshot.prediction >= fake.photons && frame.snapshot.prediction <= .01f + fake.photons);
  frame = setup();
  fake.frequency = 1.f;
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 10.).status == OPENVR_FRAME_OK);
  CHECK(fake.timeout == 100);
  frame = setup();
  fake.frequency = 1000.f;
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 10.).status == OPENVR_FRAME_OK);
  CHECK(fake.timeout == 2);
  frame = setup();
  struct VR_IVRSystem_FnTable incomplete = system;
  incomplete.GetProjectionRaw = NULL;
  frame.system = &incomplete;
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 10.).status == OPENVR_FRAME_INVALID);
  CHECK(fake.waits == 0 && fake.clocks == 0);
  CHECK(lovrOpenVRFrameUpdate(NULL, ETrackingUniverseOrigin_TrackingUniverseStanding, clockNow, NULL).status == OPENVR_FRAME_INVALID);
  frame = setup();
  CHECK(lovrOpenVRFrameUpdate(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, NULL, NULL).status == OPENVR_FRAME_INVALID);
  CHECK(fake.waits == 0 && fake.clocks == 0);
  return true;
}

static bool extremeClock(void) {
  OpenVRFrame frame = setup();
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 10.).status == OPENVR_FRAME_OK);
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, DBL_MAX).status == OPENVR_FRAME_INVALID);
  CHECK(!frame.snapshot.head.valid && !frame.snapshot.eyes[0].pose.valid);
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 1e16).status == OPENVR_FRAME_INVALID);
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 10.02).status == OPENVR_FRAME_OK);
  CHECK(closeTo(frame.snapshot.displayTime, .03));
  fake.head.bPoseIsValid = false;
  CHECK(updateAt(&frame, ETrackingUniverseOrigin_TrackingUniverseStanding, 10.03).status == OPENVR_FRAME_OK);
  CHECK(!frame.snapshot.head.valid && !frame.snapshot.velocityValid && !frame.snapshot.eyes[0].pose.valid);
  for (unsigned i = 0; i < 16; i++) CHECK(frame.snapshot.head.matrix[i] == 0.f);
  return true;
}

static bool projectionValidation(void) {
  float tangents[4] = { -1.f, 1.f, -1.f, 1.f };
  float output[16], saved[16];
  for (unsigned i = 0; i < 16; i++) output[i] = saved[i] = 42.f + i;
  CHECK(!lovrOpenVRFrameProjection(tangents, FLT_MIN, FLT_MAX, output));
  CHECK(memcmp(output, saved, sizeof(output)) == 0);
  CHECK(!lovrOpenVRFrameProjection(NULL, .1f, 0.f, output));
  CHECK(!lovrOpenVRFrameProjection(tangents, .1f, 0.f, NULL));
  tangents[2] = 2.f;
  CHECK(!lovrOpenVRFrameProjection(tangents, .1f, 0.f, output));
  tangents[2] = NAN;
  CHECK(!lovrOpenVRFrameProjection(tangents, .1f, 0.f, output));
  CHECK(memcmp(output, saved, sizeof(output)) == 0);
  tangents[0] = -FLT_MAX;
  tangents[1] = FLT_MAX;
  tangents[2] = -FLT_MAX;
  tangents[3] = FLT_MAX;
  CHECK(lovrOpenVRFrameProjection(tangents, .1f, 0.f, output));
  CHECK(output[0] > 0.f && output[5] < 0.f);
  tangents[0] = tangents[2] = -1.f;
  tangents[1] = tangents[3] = 1.f;
  CHECK(lovrOpenVRFrameProjection(tangents, 1.f, nextafterf(1.f, INFINITY), output));
  CHECK(closeTo(-output[10] + output[14], 1.));
  memcpy(output, saved, sizeof(output));
  CHECK(!lovrOpenVRFrameProjection(tangents, FLT_MAX / 2.f, nextafterf(FLT_MAX / 2.f, INFINITY), output));
  CHECK(memcmp(output, saved, sizeof(output)) == 0);
  return true;
}

static bool frustumEdges(void) {
  float tangents[4] = { -1.2f, .8f, -.7f, 1.1f }, matrix[16];
  for (unsigned finite = 0; finite < 2; finite++) {
    CHECK(lovrOpenVRFrameProjection(tangents, .1f, finite ? 100.f : 0.f, matrix));
    CHECK(closeTo(matrix[0] * tangents[0] - matrix[8], -1));
    CHECK(closeTo(matrix[0] * tangents[1] - matrix[8], 1));
    // OpenVR's raw vertical tangents use the opposite sign to physical eye-space Y.
    CHECK(closeTo(matrix[5] * -tangents[2] - matrix[9], -1));
    CHECK(closeTo(matrix[5] * -tangents[3] - matrix[9], 1));
    float oracle[16];
    mat4_fov(oracle, -atanf(tangents[0]), atanf(tangents[1]), -atanf(tangents[2]), atanf(tangents[3]),
      .1f, finite ? 100.f : 0.f);
    // mat4_fov uses a different finite-depth convention; compare the independent XY terms only.
    unsigned xy[] = { 0, 5, 8, 9 };
    for (unsigned i = 0; i < 4; i++) CHECK(closeTo(matrix[xy[i]], oracle[xy[i]]));
    CHECK(closeTo((-matrix[10] * .1f + matrix[14]) / .1f, 1));
    if (finite) CHECK(closeTo((-matrix[10] * 100.f + matrix[14]) / 100.f, 0));
    else CHECK(matrix[10] == 0.f && closeTo(matrix[14], .1));
  }
  CHECK(!lovrOpenVRFrameProjection(tangents, 0, 0, matrix));
  CHECK(!lovrOpenVRFrameProjection(tangents, 1, .5f, matrix));
  tangents[0] = NAN;
  CHECK(!lovrOpenVRFrameProjection(tangents, .1f, 0, matrix));
  tangents[0] = 2;
  CHECK(!lovrOpenVRFrameProjection(tangents, .1f, 0, matrix));
  return true;
}

int main(int argc, char** argv) {
  NativeTest tests[] = {
    { "predicted_pose_conversion", predictedPose },
    { "overlay_frame_timeout", timeoutProgress },
    { "frame_invalid_inputs", invalidInputs },
    { "frame_session_clock_reset", sessionClock },
    { "projection_frustum_edges", frustumEdges },
    { "frame_extreme_clock_recovery", extremeClock },
    { "projection_validation", projectionValidation }
  };
  return nativeRunTests(argc, argv, tests, sizeof(tests) / sizeof(tests[0]));
}
