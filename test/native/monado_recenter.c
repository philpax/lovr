#include "headset/monado_recenter.h"
#include "test.h"
#include "util.h"
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

static struct {
  unsigned opens, closes, symbols, creates, destroys, gets, sets, liveRoots;
  uint32_t major, minor;
  const char* missing;
  bool loadFails, nullRoot, rejectFloor, mutateBeforeError;
  mnd_result_t createResult, getResult, setResult;
  mnd_pose_t offset, written;
  mnd_reference_space_type_t queried, selected;
} fake;
static float position[3], orientation[4];
static char rootToken, libraryToken;

static void version(uint32_t* major, uint32_t* minor, uint32_t* patch) {
  *major = fake.major; *minor = fake.minor; *patch = 0;
}
static mnd_result_t create(mnd_root_t** root) {
  fake.creates++;
  *root = fake.nullRoot ? NULL : (mnd_root_t*) &rootToken;
  fake.liveRoots += *root != NULL;
  return fake.createResult;
}
static void destroy(mnd_root_t** root) {
  if (*root != (mnd_root_t*) &rootToken) abort();
  fake.destroys++;
  fake.liveRoots--;
  *root = NULL;
}
static mnd_result_t get(mnd_root_t* root, mnd_reference_space_type_t space, mnd_pose_t* offset) {
  if (root != (mnd_root_t*) &rootToken) abort();
  fake.gets++; fake.queried = space; *offset = fake.offset;
  return fake.getResult;
}
static mnd_result_t set(mnd_root_t* root, mnd_reference_space_type_t space, const mnd_pose_t* offset) {
  if (root != (mnd_root_t*) &rootToken) abort();
  fake.sets++; fake.selected = space; fake.written = *offset;
  if (fake.rejectFloor && space == MND_SPACE_REFERENCE_TYPE_LOCAL_FLOOR) return MND_ERROR_INVALID_OPERATION;
  if (fake.setResult >= 0 || fake.mutateBeforeError) fake.offset = *offset;
  return fake.setResult;
}
static void* openLibrary(void* context, const char* path) {
  if (context != &fake || strcmp(path, "/application/runtime/libmonado.so") != 0) abort();
  fake.opens++;
  return fake.loadFails ? NULL : &libraryToken;
}
static void* symbol(void* context, void* library, const char* name) {
  if (context != &fake || library != &libraryToken) abort();
  fake.symbols++;
  if (fake.missing && strcmp(fake.missing, name) == 0) return NULL;
  if (!strcmp(name, "mnd_api_get_version")) return (void*) version;
  if (!strcmp(name, "mnd_root_create")) return (void*) create;
  if (!strcmp(name, "mnd_root_destroy")) return (void*) destroy;
  if (!strcmp(name, "mnd_root_get_reference_space_offset")) return (void*) get;
  if (!strcmp(name, "mnd_root_set_reference_space_offset")) return (void*) set;
  abort();
}
static void closeLibrary(void* context, void* library) {
  if (context != &fake || library != &libraryToken || fake.liveRoots != 0) abort();
  fake.closes++;
}
static const MonadoRecenterLoader loader = { &fake, openLibrary, symbol, closeLibrary };

static void fixture(void) {
  memset(&fake, 0, sizeof(fake));
  fake.major = 1; fake.minor = 8;
  fake.offset.orientation.w = 1.f;
  position[0] = 2.f; position[1] = 1.7f; position[2] = -3.f;
  orientation[0] = orientation[1] = orientation[2] = 0.f; orientation[3] = 1.f;
}
static bool init(MonadoRecenter* adapter) {
  return lovrMonadoRecenterInitWithLoader(adapter, "/application/runtime/libmonado.so", &loader);
}
static HeadsetRecenterStatus status(MonadoRecenter* adapter) {
  return lovrMonadoGetRecenterStatus(adapter, MND_SPACE_REFERENCE_TYPE_LOCAL, position, orientation, true);
}
static bool errorContext(void) {
  return strncmp(lovrGetError(), "monado: recenter:", 17) == 0;
}

static bool lazyLifecycle(void) {
  fixture();
  MonadoRecenter adapter;
  char path[] = "/application/runtime/libmonado.so";
  CHECK(lovrMonadoRecenterInitWithLoader(&adapter, path, &loader));
  path[1] = 'x';
  CHECK(fake.opens == 0 && fake.creates == 0);
  CHECK(status(&adapter) == HEADSET_RECENTER_AVAILABLE);
  CHECK(status(&adapter) == HEADSET_RECENTER_AVAILABLE);
  CHECK(fake.opens == 1 && fake.creates == 1 && fake.gets == 2 && fake.sets == 0);
  lovrMonadoRecenterDestroy(&adapter);
  lovrMonadoRecenterDestroy(&adapter);
  CHECK(fake.destroys == 1 && fake.closes == 1);
  CHECK(init(&adapter));
  CHECK(status(&adapter) == HEADSET_RECENTER_AVAILABLE);
  lovrMonadoRecenterDestroy(&adapter);
  CHECK(fake.destroys == 2 && fake.closes == 2);
  return true;
}

static bool unsupported(void) {
  fixture();
  MonadoRecenter adapter;
  const char* paths[] = { NULL, "" };
  for (unsigned i = 0; i < 2; i++) {
    CHECK(lovrMonadoRecenterInitWithLoader(&adapter, paths[i], &loader));
    CHECK(status(&adapter) == HEADSET_RECENTER_UNSUPPORTED);
    lovrMonadoRecenterDestroy(&adapter);
  }
  CHECK(fake.opens == 0);
  CHECK(!lovrMonadoRecenterInitWithLoader(&adapter, "libmonado.so", &loader));
  CHECK(!lovrMonadoRecenterInitWithLoader(&adapter, "./libmonado.so", &loader));
  CHECK(fake.opens == 0 && errorContext());
  lovrMonadoRecenterDestroy(&adapter);
  const char* symbols[] = { "mnd_api_get_version", "mnd_root_create", "mnd_root_destroy",
    "mnd_root_get_reference_space_offset", "mnd_root_set_reference_space_offset" };
  for (unsigned i = 0; i < 5; i++) {
    fixture(); fake.missing = symbols[i];
    CHECK(init(&adapter));
    CHECK(status(&adapter) == HEADSET_RECENTER_UNSUPPORTED);
    CHECK(status(&adapter) == HEADSET_RECENTER_UNSUPPORTED);
    CHECK(fake.creates == 0 && fake.closes == 1 && fake.opens == 1 && errorContext());
    lovrMonadoRecenterDestroy(&adapter);
  }
  const uint32_t versions[][2] = { { 0, 8 }, { 2, 8 }, { 1, 2 }, { 1, 3 }, { 1, 99 } };
  for (unsigned i = 0; i < 5; i++) {
    fixture(); fake.major = versions[i][0]; fake.minor = versions[i][1];
    CHECK(init(&adapter));
    CHECK(status(&adapter) == (i < 3 ? HEADSET_RECENTER_UNSUPPORTED : HEADSET_RECENTER_AVAILABLE));
    lovrMonadoRecenterDestroy(&adapter);
  }
  fixture(); CHECK(init(&adapter));
  CHECK(lovrMonadoGetRecenterStatus(&adapter, MND_SPACE_REFERENCE_TYPE_VIEW, position, orientation, true) == HEADSET_RECENTER_UNSUPPORTED);
  CHECK(lovrMonadoGetRecenterStatus(&adapter, MND_SPACE_REFERENCE_TYPE_UNBOUNDED, position, orientation, true) == HEADSET_RECENTER_UNSUPPORTED);
  CHECK(fake.sets == 0 && fake.opens == 0);
  lovrMonadoRecenterDestroy(&adapter);
  MonadoRecenterLoader incomplete = loader;
  incomplete.symbol = NULL;
  CHECK(!lovrMonadoRecenterInitWithLoader(&adapter, "explicit", &incomplete));
  CHECK(errorContext());
  lovrMonadoRecenterDestroy(&adapter);
  return true;
}

static bool failures(void) {
  MonadoRecenter adapter;
  fixture(); fake.loadFails = true; CHECK(init(&adapter));
  CHECK(status(&adapter) == HEADSET_RECENTER_UNAVAILABLE && errorContext());
  fake.loadFails = false;
  CHECK(status(&adapter) == HEADSET_RECENTER_AVAILABLE);
  lovrMonadoRecenterDestroy(&adapter);
  fixture(); fake.createResult = MND_ERROR_CONNECTING_FAILED; CHECK(init(&adapter));
  CHECK(status(&adapter) == HEADSET_RECENTER_UNAVAILABLE && errorContext());
  CHECK(fake.destroys == 1);
  fake.createResult = MND_SUCCESS;
  CHECK(status(&adapter) == HEADSET_RECENTER_AVAILABLE);
  lovrMonadoRecenterDestroy(&adapter);
  fixture(); fake.nullRoot = true; CHECK(init(&adapter));
  CHECK(status(&adapter) == HEADSET_RECENTER_UNAVAILABLE);
  lovrMonadoRecenterDestroy(&adapter);
  fixture(); CHECK(init(&adapter)); fake.getResult = MND_ERROR_INVALID_OPERATION;
  CHECK(status(&adapter) == HEADSET_RECENTER_UNAVAILABLE && errorContext());
  CHECK(!lovrMonadoRecenter(&adapter, MND_SPACE_REFERENCE_TYPE_LOCAL, position, orientation, true));
  CHECK(fake.sets == 0 && !adapter.writeAttempted);
  fake.getResult = MND_SUCCESS; fake.setResult = MND_ERROR_INVALID_OPERATION;
  CHECK(!lovrMonadoRecenter(&adapter, MND_SPACE_REFERENCE_TYPE_LOCAL, position, orientation, true));
  CHECK(fake.sets == 1 && fake.selected == MND_SPACE_REFERENCE_TYPE_LOCAL && errorContext());
  CHECK(adapter.writeAttempted);
  CHECK(fake.offset.position.x == 0.f);
  lovrMonadoRecenterDestroy(&adapter);
  return true;
}

static bool connectionLoss(void) {
  fixture(); MonadoRecenter adapter; CHECK(init(&adapter));
  CHECK(status(&adapter) == HEADSET_RECENTER_AVAILABLE);
  fake.getResult = MND_ERROR_OPERATION_FAILED;
  CHECK(status(&adapter) == HEADSET_RECENTER_UNAVAILABLE);
  CHECK(fake.destroys == 1 && fake.liveRoots == 0);
  fake.getResult = MND_SUCCESS;
  CHECK(status(&adapter) == HEADSET_RECENTER_AVAILABLE);
  CHECK(fake.creates == 2 && fake.opens == 1);
  fake.setResult = MND_ERROR_OPERATION_FAILED;
  CHECK(!lovrMonadoRecenter(&adapter, MND_SPACE_REFERENCE_TYPE_LOCAL, position, orientation, true));
  CHECK(fake.destroys == 2 && fake.liveRoots == 0);
  fake.setResult = MND_SUCCESS;
  CHECK(status(&adapter) == HEADSET_RECENTER_AVAILABLE);
  CHECK(fake.creates == 3 && fake.opens == 1);
  lovrMonadoRecenterDestroy(&adapter);
  return true;
}

static bool invalidPoses(void) {
  fixture(); MonadoRecenter adapter; CHECK(init(&adapter));
  CHECK(lovrMonadoGetRecenterStatus(&adapter, MND_SPACE_REFERENCE_TYPE_LOCAL, position, orientation, false) == HEADSET_RECENTER_UNTRACKED);
  CHECK(lovrMonadoGetRecenterStatus(&adapter, MND_SPACE_REFERENCE_TYPE_LOCAL, NULL, orientation, true) == HEADSET_RECENTER_UNTRACKED);
  CHECK(lovrMonadoGetRecenterStatus(&adapter, MND_SPACE_REFERENCE_TYPE_LOCAL, position, NULL, true) == HEADSET_RECENTER_UNTRACKED);
  for (unsigned i = 0; i < 3; i++) {
    float saved = position[i]; position[i] = NAN;
    CHECK(status(&adapter) == HEADSET_RECENTER_UNTRACKED); position[i] = saved;
  }
  for (unsigned i = 0; i < 4; i++) {
    float saved = orientation[i]; orientation[i] = INFINITY;
    CHECK(status(&adapter) == HEADSET_RECENTER_UNTRACKED); orientation[i] = saved;
  }
  orientation[3] = 0.f;
  CHECK(status(&adapter) == HEADSET_RECENTER_UNTRACKED);
  orientation[3] = 2.f;
  CHECK(status(&adapter) == HEADSET_RECENTER_UNTRACKED);
  orientation[0] = sqrtf(.5f); orientation[3] = sqrtf(.5f);
  CHECK(status(&adapter) == HEADSET_RECENTER_UNTRACKED);
  orientation[0] = 0.f; orientation[3] = 1.f;
  fake.offset.orientation.x = .1f; fake.offset.orientation.w = sqrtf(.99f);
  CHECK(status(&adapter) == HEADSET_RECENTER_UNAVAILABLE);
  CHECK(!lovrMonadoRecenter(&adapter, MND_SPACE_REFERENCE_TYPE_LOCAL, position, orientation, true));
  fake.offset.orientation.x = 0.f; fake.offset.orientation.w = 0.f;
  CHECK(status(&adapter) == HEADSET_RECENTER_UNAVAILABLE);
  fake.offset.orientation.w = 1.f; fake.offset.position.y = NAN;
  CHECK(status(&adapter) == HEADSET_RECENTER_UNAVAILABLE);
  CHECK(fake.sets == 0);
  lovrMonadoRecenterDestroy(&adapter);
  return true;
}

static bool localFloorRejection(void) {
  fixture(); fake.rejectFloor = true;
  MonadoRecenter adapter; CHECK(init(&adapter));
  fake.offset.position.y = -1.3f;
  CHECK(lovrMonadoGetRecenterStatus(&adapter, MND_SPACE_REFERENCE_TYPE_LOCAL_FLOOR,
    position, orientation, true) == HEADSET_RECENTER_UNSUPPORTED);
  CHECK(!lovrMonadoRecenter(&adapter, MND_SPACE_REFERENCE_TYPE_LOCAL_FLOOR, position, orientation, true));
  CHECK(fake.sets == 0 && !adapter.writeAttempted && fake.opens == 0);
  CHECK(fake.offset.position.x == 0.f && fake.offset.position.y == -1.3f);
  CHECK(errorContext());
  mnd_pose_t snapshot;
  CHECK(lovrMonadoReadRecenterOffset(&adapter, MND_SPACE_REFERENCE_TYPE_LOCAL_FLOOR, &snapshot));
  CHECK(!lovrMonadoRecenterFromOffset(&adapter, MND_SPACE_REFERENCE_TYPE_LOCAL_FLOOR, &snapshot, position, orientation, true));
  CHECK(fake.sets == 0 && !adapter.writeAttempted);
  CHECK(lovrMonadoRecenter(&adapter, MND_SPACE_REFERENCE_TYPE_LOCAL, position, orientation, true));
  CHECK(fake.sets == 1 && fake.selected == MND_SPACE_REFERENCE_TYPE_LOCAL && adapter.writeAttempted);
  CHECK(fake.offset.position.x == 2.f && fake.offset.position.y == -1.3f);
  lovrMonadoRecenterDestroy(&adapter);
  return true;
}

static bool largeFiniteComposition(void) {
  fixture(); MonadoRecenter adapter; CHECK(init(&adapter));
  fake.offset.position.x = FLT_MAX * .75f; fake.offset.position.z = FLT_MAX * .75f;
  fake.offset.orientation.y = sinf(.3926990817f); fake.offset.orientation.w = cosf(.3926990817f);
  position[0] = FLT_MAX; position[2] = -FLT_MAX;
  CHECK(lovrMonadoRecenter(&adapter, MND_SPACE_REFERENCE_TYPE_LOCAL, position, orientation, true));
  CHECK(isfinite(fake.written.position.x) && isfinite(fake.written.position.z));
  CHECK(fabsf(fake.written.position.x / FLT_MAX - .75f) < 1e-5f);
  // X cancels after an overflowing float intermediate; Z remains within the float range.
  fake.offset.position.x = FLT_MAX * .75f;
  fake.offset.position.z = 0.f;
  position[0] = FLT_MAX * .6f; position[2] = -FLT_MAX * .6f;
  CHECK(lovrMonadoRecenter(&adapter, MND_SPACE_REFERENCE_TYPE_LOCAL, position, orientation, true));
  CHECK(isfinite(fake.written.position.x) && isfinite(fake.written.position.z));
  CHECK(fabsf(fake.written.position.x / FLT_MAX - .75f) < 1e-5f);
  CHECK(fabsf(fake.written.position.z / FLT_MAX + .84852814f) < 1e-5f);
  lovrMonadoRecenterDestroy(&adapter);
  return true;
}

static bool offsetSnapshot(void) {
  fixture(); MonadoRecenter adapter; CHECK(init(&adapter));
  mnd_pose_t snapshot;
  CHECK(lovrMonadoReadRecenterOffset(&adapter, MND_SPACE_REFERENCE_TYPE_LOCAL, &snapshot));
  CHECK(fake.opens == 1 && fake.gets == 1);
  fake.offset.position.x = 1.f;
  CHECK(!lovrMonadoRecenterFromOffset(&adapter, MND_SPACE_REFERENCE_TYPE_LOCAL, &snapshot, position, orientation, true));
  CHECK(fake.sets == 0 && !adapter.writeAttempted && strstr(lovrGetError(), "changed") != NULL);
  CHECK(lovrMonadoReadRecenterOffset(&adapter, MND_SPACE_REFERENCE_TYPE_LOCAL, &snapshot));
  CHECK(lovrMonadoRecenterFromOffset(&adapter, MND_SPACE_REFERENCE_TYPE_LOCAL, &snapshot, position, orientation, true));
  CHECK(fake.sets == 1 && fake.written.position.x == 3.f && adapter.writeAttempted);
  CHECK(!lovrMonadoRecenterFromOffset(&adapter, MND_SPACE_REFERENCE_TYPE_LOCAL, NULL, position, orientation, true));
  CHECK(!lovrMonadoReadRecenterOffset(&adapter, MND_SPACE_REFERENCE_TYPE_LOCAL, NULL));
  snapshot.orientation.w = NAN;
  CHECK(!lovrMonadoRecenterFromOffset(&adapter, MND_SPACE_REFERENCE_TYPE_LOCAL, &snapshot, position, orientation, true));
  CHECK(fake.sets == 1 && !adapter.writeAttempted);
  CHECK(lovrMonadoReadRecenterOffset(&adapter, MND_SPACE_REFERENCE_TYPE_LOCAL, &snapshot));
  fake.mutateBeforeError = true; fake.setResult = MND_ERROR_INVALID_OPERATION;
  CHECK(!lovrMonadoRecenterFromOffset(&adapter, MND_SPACE_REFERENCE_TYPE_LOCAL, &snapshot, position, orientation, true));
  CHECK(adapter.writeAttempted && fake.sets == 2 && fake.offset.position.x == 5.f);
  CHECK(!lovrMonadoRecenterFromOffset(&adapter, MND_SPACE_REFERENCE_TYPE_LOCAL, &snapshot, position, orientation, true));
  CHECK(!adapter.writeAttempted && fake.sets == 2);
  CHECK(!lovrMonadoRecenter(&adapter, MND_SPACE_REFERENCE_TYPE_LOCAL, position, orientation, false));
  CHECK(!adapter.writeAttempted && fake.sets == 2);
  lovrMonadoRecenterDestroy(&adapter);
  return true;
}

static bool composition(void) {
  const mnd_reference_space_type_t spaces[] = {
    MND_SPACE_REFERENCE_TYPE_LOCAL, MND_SPACE_REFERENCE_TYPE_STAGE
  };
  for (unsigned i = 0; i < 2; i++) {
    fixture(); MonadoRecenter adapter; CHECK(init(&adapter));
    fake.offset.position.x = 10.f; fake.offset.position.y = .4f; fake.offset.position.z = 20.f;
    fake.offset.orientation.y = sqrtf(.5f); fake.offset.orientation.w = sqrtf(.5f);
    // The head includes pitch and roll; projected forward still has a 90-degree yaw.
    orientation[0] = .1f; orientation[1] = .7f; orientation[2] = -.1f; orientation[3] = .7f;
    CHECK(lovrMonadoRecenter(&adapter, spaces[i], position, orientation, true));
    CHECK(fake.gets == 1 && fake.sets == 1 && fake.queried == spaces[i] && fake.selected == spaces[i]);
    CHECK(fabsf(fake.written.position.x - 7.f) < 1e-5f);
    CHECK(fake.written.position.y == .4f);
    CHECK(fabsf(fake.written.position.z - 18.f) < 1e-5f);
    CHECK(fabsf(fake.written.orientation.x) < 1e-5f && fabsf(fake.written.orientation.z) < 1e-5f);
    CHECK(fabsf(fabsf(fake.written.orientation.y) - 1.f) < 1e-5f);
    CHECK(fabsf(fake.written.orientation.w) < 1e-5f);
    position[0] = position[2] = 0.f;
    orientation[0] = orientation[1] = orientation[2] = 0.f; orientation[3] = 1.f;
    CHECK(lovrMonadoRecenter(&adapter, spaces[i], position, orientation, true));
    CHECK(fabsf(fake.written.position.x - 7.f) < 1e-5f && fake.written.position.y == .4f);
    lovrMonadoRecenterDestroy(&adapter);
  }
  fixture(); MonadoRecenter adapter; CHECK(init(&adapter));
  fake.offset.position.x = FLT_MAX; position[0] = FLT_MAX;
  CHECK(!lovrMonadoRecenter(&adapter, MND_SPACE_REFERENCE_TYPE_STAGE, position, orientation, true));
  CHECK(fake.sets == 0 && errorContext());
  lovrMonadoRecenterDestroy(&adapter);
  return true;
}

int main(int argc, char** argv) {
  NativeTest tests[] = {
    { "monado.recenter.lazy-lifecycle", lazyLifecycle },
    { "monado.recenter.unsupported", unsupported },
    { "monado.recenter.failures", failures },
    { "monado.recenter.invalid-poses", invalidPoses },
    { "monado.recenter.composition", composition },
    { "monado.recenter.local-floor-rejection", localFloorRejection },
    { "monado.recenter.offset-snapshot", offsetSnapshot },
    { "monado.recenter.connection-loss", connectionLoss },
    { "monado.recenter.large-finite-composition", largeFiniteComposition }
  };
  return nativeRunTests(argc, argv, tests, sizeof(tests) / sizeof(tests[0]));
}
