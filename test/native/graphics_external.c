#include "../../src/modules/graphics/graphics.c"
#define LOVR_LAYER_GRAPHICS_HARNESS
#define main sessionTestsMain
#define gpu_stream_begin unused_stream_begin
#define gpu_stream_end unused_stream_end
#define gpu_sync unused_sync
#define gpu_submit unused_submit
#define gpu_render_begin unused_render_begin
#define gpu_render_end unused_render_end
#include "graphics_session.c"
#undef main
#undef gpu_stream_begin
#undef gpu_stream_end
#undef gpu_sync
#undef gpu_submit
#undef gpu_render_begin
#undef gpu_render_end

static char trace[256];
static unsigned submits;
static unsigned failSubmit;
static unsigned ends;
static unsigned failEnd;
static bool failBeginBarrier;
static bool failEndBarrier;
static bool rejectImage;
static bool callbackResult;
static bool lockHeld;
static bool reentryRejected;
static bool failStream;
static bool failAfterDescriptor;
static bool invalidateAfterDescriptor;
static unsigned descriptors;
static bool releaseLastReference;
static bool destroyedUnlocked;
static bool quiesceLocked;
static bool drainLocked;
static Texture texture;
static Sync textureSync;
static gpu_texture backing;

static void record(const char* event) { strcat(trace, event); }
size_t gpu_sizeof_texture(void) { return sizeof(gpu_texture); }
void gpu_texture_destroy(gpu_texture* object) {
  object->destroyed = true;
  if (releaseLastReference) {
    destroyedUnlocked = mtx_trylock(&state.lock) == thrd_success;
    if (destroyedUnlocked) mtx_unlock(&state.lock);
    record("destroy,");
  }
}
bool gpu_texture_init_view(gpu_texture* object, gpu_texture_view_info* info) { return false; }
bool gpu_texture_init(gpu_texture* object, gpu_texture_info* info) {
  *object = (gpu_texture) { 0 };
  return true;
}
bool gpu_begin_teardown(void) { abort(); }
bool gpu_destroy(void) { abort(); }
void lovrHeadsetGraphicsDestroyed(void) { abort(); }
void gpu_layout_destroy(gpu_layout* object) { abort(); }
bool gpu_wait_idle(void) { return true; }
bool gpu_prepare_teardown(void) { return true; }
bool gpu_quiesce_locked(void) {
  quiesceLocked = mtx_trylock(&state.lock) != thrd_success;
  if (!quiesceLocked) mtx_unlock(&state.lock);
  return !prepareFails;
}
void gpu_flush_deferred(void) {}
void gpu_flush_deferred_after_idle(void) {
  drainLocked = mtx_trylock(&state.lock) != thrd_success;
  if (!drainLocked) mtx_unlock(&state.lock);
}
bool lovrHeadsetIsActive(void) { return false; }
double lovrHeadsetGetDisplayTime(void) { return 0.; }
bool os_window_is_open(void) { abort(); }
void os_window_get_size(uint32_t* width, uint32_t* height) { abort(); }
float os_window_get_pixel_density(void) { abort(); }
uintptr_t os_get_xcb_connection(void) { abort(); }
uintptr_t os_get_xcb_window(void) { abort(); }
void os_on_resize(fn_resize* callback) { abort(); }
void lovrEventPush(Event event) { abort(); }
bool gpu_surface_init(gpu_surface_info* info) { abort(); }
gpu_texture_format gpu_surface_get_format(void) { abort(); }
bool gpu_surface_resize(uint32_t width, uint32_t height) { abort(); }
bool gpu_surface_acquire(gpu_texture** object, uint32_t* width, uint32_t* height) { abort(); }
bool gpu_surface_present(void) { abort(); }
bool gpu_wait_tick(uint32_t tick) { return true; }
gpu_stream* gpu_stream_begin(const char* label) { return failStream ? NULL : (gpu_stream*) &backing; }
bool gpu_stream_end(gpu_stream* stream) { return stream && ++ends != failEnd; }
void gpu_sync(gpu_stream* stream, gpu_barrier* barriers, uint32_t count) {}
bool gpu_submit(gpu_stream** streams, uint32_t count, uint32_t tick) {
  record("submit,");
  return ++submits != failSubmit;
}
void gpu_render_begin(gpu_stream* stream, gpu_canvas* canvas, gpu_timestamp_writes* timestamps) { record("render,"); }
void gpu_render_end(gpu_stream* stream, gpu_canvas* canvas, gpu_timestamp_writes* timestamps) {}
bool gpu_texture_get_external_image(gpu_texture* object, gpu_external_image* image) {
  if (!object || object->view || object->destroyed || rejectImage) return false;
  *image = (gpu_external_image) { .image = 42, .samples = 1 };
  descriptors++;
  if (failAfterDescriptor) state.submissionFailed = true;
  if (invalidateAfterDescriptor) rejectImage = true;
  return true;
}
bool gpu_texture_external_barrier(gpu_stream* stream, gpu_texture* object, bool begin) {
  if (object->destroyed) abort();
  record(begin ? "begin," : "end,");
  return !(begin ? failBeginBarrier : failEndBarrier);
}
static bool callback(const gpu_external_image* image, void* data) {
  record("callback,");
  lockHeld = mtx_trylock(&state.lock) != thrd_success;
  if (!lockHeld) mtx_unlock(&state.lock);
  unsigned references = atomic_load(&ref);
  lovrGraphicsDestroy();
  reentryRejected = atomic_load(&ref) == references && state.initialized &&
    !lovrGraphicsHandoffTexture(&texture, callback, NULL, NULL) &&
    !lovrGraphicsSubmit(NULL, 0) && !lovrGraphicsPresent() && !lovrGraphicsWait() &&
    !lovrGraphicsPrepareSessionTeardown() && !lovrGraphicsQuiesceSessionResources() &&
    !lovrGraphicsDrainSessionResources() && !lockQueue(NULL);
  return image->image == 42 && data == &texture && callbackResult;
}
static void setup(void) {
  memset(&state, 0, sizeof(state));
  memset(&texture, 0, sizeof(texture));
  memset(&textureSync, 0, sizeof(textureSync));
  backing = (gpu_texture) { 0 };
  texture.ref = 1;
  texture.root = &texture;
  texture.gpu = &backing;
  texture.sync = &textureSync;
  texture.info = (TextureInfo) { .type = TEXTURE_2D, .layers = 1, .samples = 1, .mipmaps = 1 };
  textureSync.barrier = &state.barrier;
  state.initialized = true;
  state.lockReady = mtx_init(&state.lock, mtx_plain) == thrd_success;
  state.stream = (gpu_stream*) &backing;
  state.tick = 10;
  trace[0] = '\0';
  submits = ends = failSubmit = failEnd = 0;
  failBeginBarrier = failEndBarrier = rejectImage = failStream = false;
  callbackResult = true;
  lockHeld = reentryRejected = false;
  failAfterDescriptor = invalidateAfterDescriptor = false;
  descriptors = 0;
  releaseLastReference = destroyedUnlocked = false;
  initAllocator(&thread.stack);
}
static void cleanup(void) {
  lovrFree(thread.stack.memory);
  thread.stack = (Allocator) { 0 };
  mtx_destroy(&state.lock);
}
static bool arbitraryTextureCallback(const gpu_external_image* image, void* data) {
  return !lovrTextureGenerateMipmaps(&texture, 0, 1) &&
    !lovrTextureSetPixels(&texture, NULL, NULL, NULL, NULL);
}
static bool arbitraryTextureAPIReentryRejected(void) {
  setup();
  texture.info.usage = TEXTURE_TRANSFER;
  texture.info.mipmaps = 2;
  state.features.formats[texture.info.format][0] = GPU_FEATURE_BLIT;
  CHECK(lovrGraphicsHandoffTexture(&texture, arbitraryTextureCallback, NULL, NULL));
  CHECK(!strcmp(trace, "submit,begin,submit,end,submit,"));
  cleanup();
  return true;
}
static bool selectiveQuiescence(void) {
  setup();
  CHECK(lovrGraphicsRegisterSessionTexture(&texture));
  CHECK(lovrGraphicsQuiesceSessionResources());
  CHECK(quiesceLocked && lovrTextureIsValid(&texture) && !backing.destroyed);
  CHECK(lovrGraphicsDrainSessionResources());
  CHECK(drainLocked && lovrTextureIsValid(&texture));
  prepareFails = true;
  CHECK(!lovrGraphicsQuiesceSessionResources());
  CHECK(lovrTextureIsValid(&texture));
  prepareFails = false;
  sessionTextures = NULL;
  cleanup();
  return true;
}
static bool ordering(void) {
  setup();
  Pass pass = { 0 };
  pass.canvas.color[0].texture = &texture;
  pass.target.width = pass.target.height = 1;
  Pass* passes[] = { &pass };
  CHECK(lovrGraphicsSubmit(passes, 1));
  uint32_t tick = 999;
  CHECK(lovrGraphicsHandoffTexture(&texture, callback, &texture, &tick));
  CHECK(!strcmp(trace, "render,submit,submit,begin,submit,callback,end,submit,"));
  CHECK(tick == 13 && lockHeld && reentryRejected && descriptors == 2);
  cleanup();
  return true;
}
static bool callbackFailure(void) {
  setup();
  callbackResult = false;
  uint32_t tick = 999;
  CHECK(!lovrGraphicsHandoffTexture(&texture, callback, &texture, &tick));
  CHECK(!strcmp(trace, "submit,begin,submit,callback,end,submit,"));
  CHECK(tick == 12 && lockHeld && reentryRejected && lovrTextureIsValid(&texture));
  cleanup();
  return true;
}
static bool invalid(void) {
  setup();
  uint32_t tick = 999;
  CHECK(!lovrGraphicsHandoffTexture(NULL, callback, &texture, &tick));
  CHECK(!lovrGraphicsHandoffTexture(&texture, NULL, &texture, &tick));
  texture.sessionDead = true;
  CHECK(!lovrGraphicsHandoffTexture(&texture, callback, &texture, &tick));
  texture.sessionDead = false;
  Texture root = texture;
  texture.root = &root;
  CHECK(!lovrGraphicsHandoffTexture(&texture, callback, &texture, &tick));
  texture.root = &texture;
  rejectImage = true;
  CHECK(!lovrGraphicsHandoffTexture(&texture, callback, &texture, &tick));
  rejectImage = false;
  state.initialized = false;
  CHECK(!lovrGraphicsHandoffTexture(&texture, callback, &texture, &tick));
  state.initialized = true;
  state.lockReady = false;
  CHECK(lockQueue(NULL));
  unlockQueue(NULL);
  CHECK(!lovrGraphicsHandoffTexture(&texture, callback, &texture, &tick));
  state.lockReady = true;
  texture.gpu = NULL;
  CHECK(!lovrGraphicsHandoffTexture(&texture, callback, &texture, &tick));
  texture.gpu = &backing;
  backing.view = true;
  CHECK(!lovrGraphicsHandoffTexture(&texture, callback, &texture, &tick));
  backing.view = false;
  CHECK(!trace[0] && tick == 999);
  CHECK(lovrGraphicsHandoffTexture(&texture, callback, &texture, NULL));
  cleanup();
  return true;
}
static bool failures(void) {
  for (unsigned stale = 0; stale < 2; stale++) {
    setup();
    failAfterDescriptor = stale == 0;
    invalidateAfterDescriptor = stale == 1;
    uint32_t tick = 999;
    CHECK(!lovrGraphicsHandoffTexture(&texture, callback, &texture, &tick));
    CHECK(!trace[0] && tick == 999 && !texture.externalFailed);
    cleanup();
  }
  for (unsigned failure = 0; failure < 8; failure++) {
    setup();
    if (failure < 3) failSubmit = failure + 1;
    if (failure == 3) failBeginBarrier = true;
    if (failure == 4) failEndBarrier = true;
    if (failure == 5) failEnd = 2;
    if (failure == 6) failEnd = 3;
    if (failure == 7) failStream = true;
    uint32_t tick = 999;
    CHECK(!lovrGraphicsHandoffTexture(&texture, callback, &texture, &tick));
    CHECK(tick == 999);
    if (failure == 0 || failure == 1 || failure == 3 || failure == 5 || failure == 7) CHECK(!strstr(trace, "callback,"));
    CHECK(!lovrTextureIsValid(&texture));
    Texture view = { .root = &texture };
    CHECK(!lovrTextureIsValid(&view));
    Pass pass = { 0 };
    pass.canvas.color[0].texture = &texture;
    Pass* passes[] = { &pass };
    unsigned previousSubmits = submits;
    CHECK(!lovrGraphicsSubmit(passes, 1));
    CHECK(submits == previousSubmits);
    cleanup();
  }
  return true;
}
static bool releaseCallback(const gpu_external_image* image, void* data) {
  Texture* owned = data;
  record("callback,");
  if (atomic_load(&owned->ref) != 2) return false;
  lovrRelease(owned, lovrTextureDestroy);
  return callbackResult;
}
static Texture* unrelatedTexture(void) {
  state.limits.textureSize2D = 64;
  state.limits.textureLayers = 1;
  state.features.sampleCounts = 1;
  state.features.formats[FORMAT_RGBA8][0] = GPU_FEATURE_SAMPLE;
  return lovrTextureCreate(&(TextureInfo) {
    .type = TEXTURE_2D, .format = FORMAT_RGBA8, .width = 4, .height = 4,
    .layers = 1, .mipmaps = 1, .samples = 1, .usage = TEXTURE_SAMPLE
  });
}

static Texture* releaseTexture;

static bool materialReleaseCallback(const gpu_external_image* image, void* data) {
  Material* material = data;
  record("callback,");
  if (material->textures[TEXTURE_COLOR]) {
    lovrRelease(material, lovrMaterialDestroy);
  } else {
    lovrRelease(releaseTexture, lovrTextureDestroy);
    releaseTexture = NULL;
  }
  record("released,");
  return callbackResult;
}

static bool callbackMaterialLastReferenceRelease(void) {
  for (unsigned direct = 0; direct < 2; direct++) {
    for (unsigned failure = 0; failure < 4; failure++) {
      setup();
      static Layout layout;
      runtimeAlive = true;
      state.limits.uniformBufferAlign = 16;
      state.materialLayout = &layout;
      state.defaultTexture = &texture;
      texture.sampleViewFloat = &backing;
      Texture* unrelated = unrelatedTexture();
      CHECK(unrelated);
      Material* material = direct ? lovrMaterialCreate(unrelated) :
        lovrTextureToMaterial(unrelated);
      CHECK(material && atomic_load(&material->ref) == 1);
      if (!direct) CHECK(lovrTextureToMaterial(unrelated) == material);
      if (direct) lovrRelease(unrelated, lovrTextureDestroy);
      else releaseTexture = unrelated;
      CHECK(atomic_load(&unrelated->ref) == 1);
      MaterialBlock* block = material->block;
      uint32_t index = material->index;
      releaseLastReference = true;
      callbackResult = failure != 1;
      failEndBarrier = failure == 2;
      if (failure == 3) failSubmit = 3;
      CHECK(lovrGraphicsHandoffTexture(&texture, materialReleaseCallback, material, NULL) == (failure == 0));
      CHECK(destroyedUnlocked && !externalDestroyMaterials);
      CHECK(block->nodes[index].tick == state.tick && block->tail == index);
      CHECK(atomic_load(&texture.ref) == 1 && state.textureMemory == 0);
      CHECK(!strcmp(trace, failure == 2 ? "submit,begin,submit,callback,released,end,destroy," :
        "submit,begin,submit,callback,released,end,submit,destroy,"));
      state.defaultTexture = NULL;
      lovrFree(thread.stack.memory);
      thread.stack = (Allocator) { 0 };
      endMaterials();
    }
  }
  return true;
}

static bool callbackLastReferenceRelease(void) {
  for (unsigned failure = 0; failure < 4; failure++) {
    setup();
    Texture* owned = lovrCalloc(sizeof(Texture) + sizeof(gpu_texture));
    *owned = texture;
    owned->root = owned;
    owned->gpu = (gpu_texture*) (owned + 1);
    owned->sync = lovrCalloc(sizeof(Sync));
    owned->sync->barrier = &state.barrier;
    releaseLastReference = true;
    callbackResult = failure != 1;
    failEndBarrier = failure == 2;
    if (failure == 3) failSubmit = 3;
    uint32_t tick = 999;
    bool success = lovrGraphicsHandoffTexture(owned, releaseCallback, owned, &tick);
    if (!strstr(trace, "callback,end,")) lovrRelease(owned, lovrTextureDestroy);
    CHECK(success == (failure == 0));
    CHECK(destroyedUnlocked);
    CHECK(!strcmp(trace, failure == 2 ? "submit,begin,submit,callback,end,destroy," :
      "submit,begin,submit,callback,end,submit,destroy,"));
    CHECK(tick == (failure < 2 ? 12 : 999));
    cleanup();
  }
  return true;
}
int main(int argc, char** argv) {
  const NativeTest tests[] = {
    { "graphics.external.callback-material-last-reference-release", callbackMaterialLastReferenceRelease },
    { "graphics.external.callback-last-reference-release", callbackLastReferenceRelease },
    { "graphics.external.arbitrary-texture-api-reentry-rejected", arbitraryTextureAPIReentryRejected },
    { "graphics.external.selective-quiescence", selectiveQuiescence },
    { "graphics.external.ordering", ordering },
    { "graphics.external.callback-failure", callbackFailure },
    { "graphics.external.invalid", invalid },
    { "graphics.external.failures", failures }
  };
  return nativeRunTests(argc, argv, tests, COUNTOF(tests));
}
