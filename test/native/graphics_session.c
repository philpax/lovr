#include "test.h"
#ifndef LOVR_LAYER_GRAPHICS_HARNESS
#include "../../src/modules/graphics/graphics.c"
#endif

struct gpu_texture { bool view; bool destroyed; };
struct gpu_buffer { void* pointer; };
static unsigned textureDestroys;
static unsigned bufferDestroys;
static unsigned tallyDestroys;
static unsigned liveViews;
static bool runtimeAlive = true;
static bool prepareFails;
static unsigned deferredDestroys;
static unsigned drainCalls;
static bool bufferInitFails;
static char error[256];

#ifndef LOVR_LAYER_GRAPHICS_HARNESS
size_t gpu_sizeof_texture(void) { return sizeof(gpu_texture); }
bool gpu_texture_init_view(gpu_texture* texture, gpu_texture_view_info* info) {
  if (!runtimeAlive || !info->source || info->source->destroyed) abort();
  *texture = (gpu_texture) { .view = true };
  liveViews++;
  return true;
}
void gpu_texture_destroy(gpu_texture* texture) {
  if (!runtimeAlive || texture->destroyed) abort();
  if (texture->view) liveViews--;
  else if (liveViews) abort();
  texture->destroyed = true;
  textureDestroys++;
  deferredDestroys++;
}
#endif
void gpu_buffer_destroy(gpu_buffer* buffer) { if (!runtimeAlive) abort(); lovrFree(buffer->pointer); buffer->pointer = NULL; bufferDestroys++; }
void gpu_tally_destroy(gpu_tally* tally) { (void) tally; if (!runtimeAlive) abort(); tallyDestroys++; }
void gpu_sampler_destroy(gpu_sampler* sampler) { (void) sampler; if (!runtimeAlive) abort(); }
void gpu_shader_destroy(gpu_shader* shader) { (void) shader; }
void gpu_pipeline_destroy(gpu_pipeline* pipeline) { (void) pipeline; }
void gpu_tree_destroy(gpu_tree* tree) { (void) tree; }
char* gpu_get_error(void) { return error; }
#ifndef LOVR_LAYER_GRAPHICS_HARNESS
bool gpu_wait_idle(void) {
  if (prepareFails) return false;
  deferredDestroys = 0;
  drainCalls++;
  return true;
}
void gpu_flush_deferred(void) { deferredDestroys = 0; drainCalls++; }
bool gpu_prepare_teardown(void) { return !prepareFails; }
void gpu_flush_deferred_after_idle(void) { deferredDestroys = 0; drainCalls++; }
#endif
size_t gpu_sizeof_buffer(void) { return sizeof(gpu_buffer); }
size_t gpu_sizeof_bundle_pool(void) { return 8; }
size_t gpu_sizeof_bundle(void) { return 8; }
bool gpu_is_complete(uint32_t tick) { (void) tick; if (state.readbacks) abort(); return true; }
bool gpu_bundle_pool_init(gpu_bundle_pool* pool, gpu_bundle_pool_info* info) { (void) pool; (void) info; return true; }
void gpu_bundle_pool_destroy(gpu_bundle_pool* pool) { (void) pool; }
bool gpu_buffer_init(gpu_buffer* buffer, gpu_buffer_info* info) {
  if (bufferInitFails) return false;
  buffer->pointer = lovrCalloc(info->size);
  if (info->pointer) *info->pointer = buffer->pointer;
  return true;
}
void gpu_buffer_flush(gpu_buffer* buffer, uint32_t offset, uint32_t extent) { (void) buffer; (void) offset; (void) extent; abort(); }
void gpu_bundle_write(gpu_bundle** bundles, gpu_bundle_info* info, uint32_t count) {
  (void) bundles;
  for (uint32_t i = 0; i < count; i++) {
    for (uint32_t j = 0; j < info[i].count; j++) {
      if (info[i].bindings[j].type == GPU_SLOT_SAMPLED_TEXTURE) {
        gpu_texture* texture = info[i].bindings[j].texture.object;
        if (!texture || texture->destroyed) abort();
      }
    }
  }
}
void gpu_copy_buffers(gpu_stream* stream, gpu_buffer* src, gpu_buffer* dst, uint32_t srcOffset, uint32_t dstOffset, uint32_t extent) {
  (void) stream; (void) src; (void) dst; (void) srcOffset; (void) dstOffset; (void) extent; abort();
}
#ifndef LOVR_LAYER_GRAPHICS_HARNESS
bool gpu_texture_init(gpu_texture* texture, gpu_texture_info* info) { (void) info; *texture = (gpu_texture) { 0 }; return true; }
#endif
bool gpu_texture_upload(gpu_texture* texture, gpu_upload_info* info) { (void) texture; (void) info; return true; }
void gpu_import_acquire(gpu_stream* stream, gpu_texture* texture, uint32_t layout, uint32_t family) {
  (void) stream; (void) texture; (void) layout; (void) family; abort();
}
void gpu_import_release(gpu_stream* stream, gpu_texture* texture, uint32_t layout, uint32_t family) {
  (void) stream; (void) texture; (void) layout; (void) family; abort();
}


gpu_stream* gpu_stream_begin(const char* label) { abort(); }
bool gpu_stream_end(gpu_stream* stream) { abort(); }
void gpu_render_begin(gpu_stream* stream, gpu_canvas* canvas, gpu_timestamp_writes* timestamps) { abort(); }
void gpu_render_end(gpu_stream* stream, gpu_canvas* canvas, gpu_timestamp_writes* timestamps) { abort(); }
void gpu_compute_begin(gpu_stream* stream, gpu_timestamp_writes* timestamps) { abort(); }
void gpu_compute_end(gpu_stream* stream, gpu_timestamp_writes* timestamps) { abort(); }
void gpu_set_viewport(gpu_stream* stream, float viewport[4], float depthRange[2]) { abort(); }
void gpu_set_scissor(gpu_stream* stream, uint32_t scissor[4]) { abort(); }
void gpu_push_constants(gpu_stream* stream, gpu_shader* shader, void* data, uint32_t size) { abort(); }
void gpu_bind_pipeline(gpu_stream* stream, gpu_pipeline* pipeline, gpu_pipeline_type type) { abort(); }
void gpu_bind_bundles(gpu_stream* stream, gpu_shader* shader, gpu_bundle** bundles, uint32_t first, uint32_t count, uint32_t* dynamicOffsets, uint32_t dynamicOffsetCount) { abort(); }
void gpu_bind_vertex_buffers(gpu_stream* stream, gpu_buffer** buffers, uint32_t* offsets, uint32_t first, uint32_t count) { abort(); }
void gpu_bind_index_buffer(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, gpu_index_type type) { abort(); }
void gpu_draw(gpu_stream* stream, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t baseInstance) { abort(); }
void gpu_draw_indexed(gpu_stream* stream, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t baseVertex, uint32_t baseInstance) { abort(); }
void gpu_draw_indirect(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t drawCount, uint32_t stride) { abort(); }
void gpu_draw_indirect_indexed(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t drawCount, uint32_t stride) { abort(); }
void gpu_compute(gpu_stream* stream, uint32_t x, uint32_t y, uint32_t z) { abort(); }
void gpu_compute_indirect(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset) { abort(); }
void gpu_copy_textures(gpu_stream* stream, gpu_texture* src, gpu_texture* dst, uint32_t srcOffset[4], uint32_t dstOffset[4], uint32_t extent[3]) { abort(); }
void gpu_copy_buffer_texture(gpu_stream* stream, gpu_buffer* src, gpu_texture* dst, uint32_t srcOffset, uint32_t dstOffset[4], uint32_t extent[3], uint32_t texelsPerRow) { abort(); }
void gpu_copy_texture_buffer(gpu_stream* stream, gpu_texture* src, gpu_buffer* dst, uint32_t srcOffset[4], uint32_t dstOffset, uint32_t extent[3]) { abort(); }
void gpu_copy_tally_buffer(gpu_stream* stream, gpu_tally* src, gpu_buffer* dst, uint32_t srcIndex, uint32_t dstOffset, uint32_t count) { abort(); }
void gpu_clear_buffer(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t extent, uint32_t value) { abort(); }
void gpu_clear_texture(gpu_stream* stream, gpu_texture* texture, float value[4], uint32_t layer, uint32_t layerCount, uint32_t level, uint32_t levelCount) { abort(); }
void gpu_clear_tally(gpu_stream* stream, gpu_tally* tally, uint32_t index, uint32_t count) { abort(); }
void gpu_blit(gpu_stream* stream, gpu_texture* src, gpu_texture* dst, uint32_t srcOffset[4], uint32_t dstOffset[4], uint32_t srcExtent[3], uint32_t dstExtent[3], gpu_filter filter) { abort(); }
void gpu_build_tree(gpu_stream* stream, gpu_tree* tree, gpu_build_info* info) { abort(); }
void gpu_sync(gpu_stream* stream, gpu_barrier* barriers, uint32_t count) { abort(); }
void gpu_tally_begin(gpu_stream* stream, gpu_tally* tally, uint32_t index) { abort(); }
void gpu_tally_finish(gpu_stream* stream, gpu_tally* tally, uint32_t index) { abort(); }
void gpu_xr_acquire(gpu_stream* stream, gpu_texture* texture) { abort(); }
void gpu_xr_release(gpu_stream* stream, gpu_texture* texture) { abort(); }
size_t gpu_sizeof_tally(void) { return 8; }
size_t gpu_sizeof_pipeline(void) { return 8; }
size_t gpu_sizeof_shader(void) { return 8; }
size_t gpu_sizeof_layout(void) { return 8; }
bool gpu_tally_init(gpu_tally* tally, gpu_tally_info* info) { (void) tally; (void) info; return true; }
bool gpu_pipeline_init_graphics(gpu_pipeline* pipeline, gpu_pipeline_info* info, bool* slow) { abort(); }
bool gpu_pipeline_init_compute(gpu_pipeline* pipeline, gpu_compute_pipeline_info* info) { (void) pipeline; (void) info; return true; }
bool gpu_shader_init(gpu_shader* shader, gpu_shader_info* info) { (void) shader; (void) info; return true; }
bool gpu_layout_init(gpu_layout* layout, gpu_layout_info* info) { (void) layout; (void) info; return true; }
bool gpu_submit(gpu_stream** streams, uint32_t count, uint32_t tick) { abort(); }
bool job_start(fn_job* fn, void* arg) { abort(); }
void job_spin(void) { abort(); }
double os_get_time(void) { abort(); }
#ifndef LOVR_LAYER_GRAPHICS_HARNESS
bool lovrHeadsetIsActive(void) { abort(); }
double lovrHeadsetGetDisplayTime(void) { abort(); }
#endif
double lovrTimerGetTime(void) { abort(); }
double lovrTimerGetEpoch(void) { abort(); }

static Texture* rootTexture(void) {
  state.limits.textureSize2D = 1024;
  state.limits.textureLayers = 16;
  state.limits.renderSize[0] = state.limits.renderSize[1] = 1024;
  state.limits.renderSize[2] = 4;
  state.features.sampleCounts = 1;
  state.features.formats[FORMAT_RGBA8][0] = GPU_FEATURE_SAMPLE | GPU_FEATURE_RENDER;
  TextureInfo info = { .type = TEXTURE_ARRAY, .format = FORMAT_RGBA8, .width = 64, .height = 64,
    .layers = 2, .mipmaps = 1, .samples = 1, .usage = TEXTURE_SAMPLE | TEXTURE_RENDER };
  Texture* texture = lovrTextureCreate(&info);
  if (!texture) abort();
  return texture;
}

static bool textureLifetime(void) {
  runtimeAlive = true;
  textureDestroys = 0;
  state.limits.renderSize[2] = 4;
  Texture* root = rootTexture();
  CHECK(lovrGraphicsRegisterSessionTexture(root));
  CHECK(lovrGraphicsRegisterSessionTexture(root));
  CHECK(atomic_load(&root->ref) == 1);
  TextureViewInfo info = { .type = TEXTURE_2D, .layerCount = 1, .levelCount = 1 };
  Texture* view = lovrTextureCreateView(root, &info);
  CHECK(view);
  Texture* nested = lovrTextureCreateView(view, &info);
  CHECK(nested);
  lovrRelease(view, lovrTextureDestroy);
  CHECK(textureDestroys == 1);
  lovrGraphicsInvalidateSessionResources();
  CHECK(textureDestroys == 3);
  CHECK(root->sessionDead && nested->sessionDead);
  CHECK(!lovrTextureIsValid(root) && !lovrTextureIsValid(nested));
  CHECK(!root->gpu && !nested->gpu && !nested->renderView);
  CHECK(lovrTextureGetInfo(nested)->width == 64);
  CHECK(!lovrTextureCreateView(nested, &info));
  CHECK(!lovrTextureToMaterial(nested));
  CHECK(!lovrTextureImportAcquire(nested, 0, 0));
  CHECK(!lovrTextureImportRelease(root, 0, 0));
  CHECK(strstr(lovrGetError(), "invalidated headset session"));
  lovrGraphicsInvalidateSessionResources();
  CHECK(textureDestroys == 3);
  runtimeAlive = false;
  lovrRelease(root, lovrTextureDestroy);
  lovrRelease(nested, lovrTextureDestroy);
  CHECK(textureDestroys == 3);
  CHECK(!sessionTextures);
  return true;
}

static bool passLifetime(void) {
  runtimeAlive = true;
  bufferDestroys = tallyDestroys = 0;
  Pass* pass = lovrPassCreate("Session");
  CHECK(lovrGraphicsRegisterSessionPass(pass));
  CHECK(atomic_load(&pass->ref) == 1);
  Texture* root = rootTexture();
  CHECK(lovrGraphicsRegisterSessionTexture(root));
  state.features.sampleCounts |= 4;
  state.features.formats[FORMAT_D32F][0] = GPU_FEATURE_RENDER;
  Canvas canvas = { .color[0].texture = root, .depthFormat = FORMAT_D32F, .samples = 4 };
  CHECK(lovrPassSetCanvas(pass, &canvas));
  CHECK(pass->tempColor[0] && pass->tempDepth);
  CHECK(pass->target.color[0].texture != root->renderView);
  CHECK(pass->target.depth.texture);
  CHECK(lovrPassBeginTally(pass, NULL));
  CHECK(lovrPassFinishTally(pass, NULL));
  unsigned destroyedBefore = textureDestroys;
  lovrGraphicsInvalidateSessionResources();
  CHECK(!pass->canvas.color[0].texture && !pass->target.color[0].texture);
  CHECK(!pass->target.depth.texture && !pass->tempDepth);
  CHECK(lovrPassGetWidth(pass) == 64);
  lovrRelease(root, lovrTextureDestroy);
  CHECK(pass->sessionDead && !lovrPassIsValid(pass));
  CHECK(textureDestroys == destroyedBefore + 3);
  CHECK(bufferDestroys == 1 && tallyDestroys == 1);
  CHECK(!lovrPassCompute(pass, 1, 1, 1, NULL, 0));
  CHECK(!lovrGraphicsSubmit(&pass, 1));
  CHECK(strstr(lovrGetError(), "invalidated headset session"));
  CHECK(!lovrPassSetCanvas(pass, NULL));
  lovrPassReset(pass);
  CHECK(pass->sessionDead);
  CHECK(!checkPassSession(pass));
  CHECK(strcmp(lovrPassGetLabel(pass), "Session") == 0);
  runtimeAlive = false;
  lovrRelease(pass, lovrPassDestroy);
  CHECK(textureDestroys == destroyedBefore + 3);
  CHECK(bufferDestroys == 1 && tallyDestroys == 1);
  CHECK(!sessionPasses);
  return true;
}

static bool staleSample(void) {
  runtimeAlive = true;
  Texture* root = rootTexture();
  root->info.usage = TEXTURE_SAMPLE;
  TextureViewInfo info = { .type = TEXTURE_2D, .layerCount = 1, .levelCount = 1 };
  Texture* view = lovrTextureCreateView(root, &info);
  CHECK(view);
  CHECK(lovrGraphicsRegisterSessionTexture(root));
  Pass* pass = lovrPassCreate(NULL);
  trackTexture(pass, root, GPU_PHASE_SHADER_FRAGMENT, GPU_CACHE_TEXTURE);
  CHECK(pass->access[ACCESS_RENDER]);
  root->info.usage |= TEXTURE_STORAGE;
  trackTexture(pass, root, GPU_PHASE_SHADER_COMPUTE, GPU_CACHE_STORAGE_READ);
  CHECK(pass->access[ACCESS_COMPUTE]);
  root->info.usage = TEXTURE_SAMPLE;
  CHECK(checkPassSession(pass));
  lovrGraphicsInvalidateSessionResources();
  CHECK(!checkPassSession(pass));
  Readback pending = { .ref = 1, .type = READBACK_TIMESTAMP, .tick = 1 };
  state.readbacks = &pending;
  CHECK(mtx_init(&state.lock, mtx_plain) == thrd_success);
  CHECK(mtx_lock(&state.lock) == thrd_success);
  CHECK(!lovrGraphicsSubmit(&pass, 1));
  CHECK(mtx_unlock(&state.lock) == thrd_success);
  mtx_destroy(&state.lock);
  CHECK(state.readbacks == &pending && !pending.complete);
  state.readbacks = NULL;
  CHECK(view->sessionDead);
  lovrRelease(pass, lovrPassDestroy);
  lovrRelease(view, lovrTextureDestroy);
  lovrRelease(root, lovrTextureDestroy);
  return true;
}

static bool lastRootView(void) {
  runtimeAlive = true;
  textureDestroys = 0;
  Texture* root = rootTexture();
  TextureViewInfo info = { .type = TEXTURE_2D, .layerCount = 1, .levelCount = 1 };
  Texture* view = lovrTextureCreateView(root, &info);
  CHECK(view);
  lovrRelease(root, lovrTextureDestroy);
  CHECK(textureDestroys == 0);
  lovrRelease(view, lovrTextureDestroy);
  CHECK(textureDestroys == 2 && liveViews == 0);
  CHECK(!sessionTextures);
  return true;
}

static void beginMaterials(void) {
  static Layout layout;
  runtimeAlive = true;
  if (mtx_init(&state.lock, mtx_plain) != thrd_success) abort();
  state.tick = 1;
  state.limits.uniformBufferAlign = 16;
  state.materialLayout = &layout;
  state.defaultTexture = rootTexture();
}

static void endMaterials(void) {
  runtimeAlive = true;
  lovrRelease(state.defaultTexture, lovrTextureDestroy);
  state.defaultTexture = NULL;
  while (state.materials) {
    MaterialBlock* block = state.materials;
    state.materials = block->next;
    uint32_t available = 0;
    for (uint32_t i = block->head; i != ~0u; i = block->nodes[i].next) {
      if (++available > MATERIAL_BLOCK_SIZE) abort();
    }
    if (available != MATERIAL_BLOCK_SIZE) abort();
    gpu_buffer_destroy(block->buffer);
    gpu_bundle_pool_destroy(block->bundlePool);
    lovrFree(block->buffer);
    lovrFree(block->bundlePool);
    lovrFree(block->bundles);
    lovrFree(block);
  }
  state.materialLayout = NULL;
  mtx_destroy(&state.lock);
}

static bool cachedMaterial(void) {
  beginMaterials();
  Texture* root = rootTexture();
  CHECK(lovrGraphicsRegisterSessionTexture(root));
  Material* material = lovrTextureToMaterial(root);
  CHECK(material && lovrMaterialIsValid(material));
  CHECK(material->textures[TEXTURE_COLOR] == NULL && atomic_load(&root->ref) == 1);
  lovrRetain(material);
  Pass* pass = lovrPassCreate(NULL);
  lovrPassSetMaterial(pass, material);
  CHECK(checkPassSession(pass));
  lovrGraphicsInvalidateSessionResources();
  CHECK(material->sessionDead && !lovrMaterialIsValid(material));
  CHECK(!checkPassSession(pass));
  CHECK(!lovrGraphicsSubmit(&pass, 1));
  lovrRelease(pass, lovrPassDestroy);
  CHECK(atomic_load(&material->ref) == 1);
  runtimeAlive = false;
  lovrRelease(root, lovrTextureDestroy);
  lovrRelease(material, lovrMaterialDestroy);
  endMaterials();
  return true;
}

static bool materialRootDiesFirst(void) {
  beginMaterials();
  Texture* root = rootTexture();
  CHECK(lovrGraphicsRegisterSessionTexture(root));
  Material* material = lovrTextureToMaterial(root);
  CHECK(material && lovrMaterialIsValid(material));
  lovrRetain(material);
  unsigned before = textureDestroys;
  lovrRelease(root, lovrTextureDestroy);
  CHECK(textureDestroys == before + 1);
  CHECK(!sessionTextures);
  CHECK(material->sessionDead && !lovrMaterialIsValid(material));
  Pass* pass = lovrPassCreate(NULL);
  lovrPassSetMaterial(pass, material);
  CHECK(pass->pipeline->material == NULL);
  lovrGraphicsInvalidateSessionResources();
  CHECK(!lovrMaterialIsValid(material));
  lovrRelease(pass, lovrPassDestroy);
  lovrRelease(material, lovrMaterialDestroy);
  endMaterials();
  return true;
}

static bool materialDeadTexture(void) {
  beginMaterials();
  Texture* root = rootTexture();
  CHECK(lovrGraphicsRegisterSessionTexture(root));
  Material* material = lovrMaterialCreate(root);
  CHECK(material && lovrMaterialIsValid(material));
  CHECK(atomic_load(&root->ref) == 2);
  lovrRelease(root, lovrTextureDestroy);
  lovrGraphicsInvalidateSessionResources();
  CHECK(!material->sessionDead && !lovrMaterialIsValid(material));
  CHECK(!lovrMaterialCreate(root));
  CHECK(!lovrTextureToMaterial(root));
  Pass* pass = lovrPassCreate(NULL);
  pass->pipeline->material = material;
  lovrRetain(material);
  CHECK(!checkPassSession(pass));
  CHECK(!lovrGraphicsSubmit(&pass, 1));
  runtimeAlive = false;
  lovrRelease(pass, lovrPassDestroy);
  lovrRelease(material, lovrMaterialDestroy);
  endMaterials();
  return true;
}

static bool materialPoolReuse(void) {
  beginMaterials();
  Texture* root = rootTexture();
  CHECK(lovrGraphicsRegisterSessionTexture(root));
  Material* material = lovrTextureToMaterial(root);
  CHECK(material);
  Material* retained[255];
  for (uint32_t i = 0; i < COUNTOF(retained); i++) {
    retained[i] = lovrMaterialCreate(NULL);
    CHECK(retained[i]);
  }
  MaterialBlock* block = material->block;
  uint32_t index = material->index;
  CHECK(block->head == ~0u);
  lovrGraphicsInvalidateSessionResources();
  CHECK(!root->material && block->head == index);
  Material* reused = lovrMaterialCreate(NULL);
  CHECK(reused->block == block && reused->index == index);
  CHECK(!reused->sessionDead && lovrMaterialIsValid(reused));
  Pass* pass = lovrPassCreate(NULL);
  lovrPassSetMaterial(pass, reused);
  CHECK(pass->pipeline->material == reused && checkPassSession(pass));
  lovrRelease(pass, lovrPassDestroy);
  lovrRelease(reused, lovrMaterialDestroy);
  for (uint32_t i = 0; i < COUNTOF(retained); i++) lovrRelease(retained[i], lovrMaterialDestroy);
  lovrRelease(root, lovrTextureDestroy);
  endMaterials();
  return true;
}

static const uint32_t sampleShader[] = {
  0x07230203, 0x00010000, 0x000d000b, 0x00000013, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
  0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
  0x0006000f, 0x00000004, 0x00000004, 0x6e69616d, 0x00000000, 0x00000009, 0x00030010, 0x00000004,
  0x00000007, 0x00030003, 0x00000002, 0x000001c2, 0x000a0004, 0x475f4c47, 0x4c474f4f, 0x70635f45,
  0x74735f70, 0x5f656c79, 0x656e696c, 0x7269645f, 0x69746365, 0x00006576, 0x00080004, 0x475f4c47,
  0x4c474f4f, 0x6e695f45, 0x64756c63, 0x69645f65, 0x74636572, 0x00657669, 0x00040005, 0x00000004,
  0x6e69616d, 0x00000000, 0x00040005, 0x00000009, 0x6f6c6f63, 0x00000072, 0x00040005, 0x0000000d,
  0x67726174, 0x00007465, 0x00040047, 0x00000009, 0x0000001e, 0x00000000, 0x00040047, 0x0000000d,
  0x00000022, 0x00000002, 0x00040047, 0x0000000d, 0x00000021, 0x00000000, 0x00020013, 0x00000002,
  0x00030021, 0x00000003, 0x00000002, 0x00030016, 0x00000006, 0x00000020, 0x00040017, 0x00000007,
  0x00000006, 0x00000004, 0x00040020, 0x00000008, 0x00000003, 0x00000007, 0x0004003b, 0x00000008,
  0x00000009, 0x00000003, 0x00090019, 0x0000000a, 0x00000006, 0x00000001, 0x00000000, 0x00000000,
  0x00000000, 0x00000001, 0x00000000, 0x0003001b, 0x0000000b, 0x0000000a, 0x00040020, 0x0000000c,
  0x00000000, 0x0000000b, 0x0004003b, 0x0000000c, 0x0000000d, 0x00000000, 0x00040017, 0x0000000f,
  0x00000006, 0x00000002, 0x0004002b, 0x00000006, 0x00000010, 0x00000000, 0x0005002c, 0x0000000f,
  0x00000011, 0x00000010, 0x00000010, 0x00050036, 0x00000002, 0x00000004, 0x00000000, 0x00000003,
  0x000200f8, 0x00000005, 0x0004003d, 0x0000000b, 0x0000000e, 0x0000000d, 0x00050057, 0x00000007,
  0x00000012, 0x0000000e, 0x00000011, 0x0003003e, 0x00000009, 0x00000012, 0x000100fd, 0x00010038,
};

static const uint32_t storageShader[] = {
  0x07230203, 0x00010000, 0x000d000b, 0x00000016, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
  0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
  0x0005000f, 0x00000005, 0x00000004, 0x6e69616d, 0x00000000, 0x00060010, 0x00000004, 0x00000011,
  0x00000001, 0x00000001, 0x00000001, 0x00030003, 0x00000002, 0x000001c2, 0x000a0004, 0x475f4c47,
  0x4c474f4f, 0x70635f45, 0x74735f70, 0x5f656c79, 0x656e696c, 0x7269645f, 0x69746365, 0x00006576,
  0x00080004, 0x475f4c47, 0x4c474f4f, 0x6e695f45, 0x64756c63, 0x69645f65, 0x74636572, 0x00657669,
  0x00040005, 0x00000004, 0x6e69616d, 0x00000000, 0x00040005, 0x00000009, 0x67726174, 0x00007465,
  0x00040047, 0x00000009, 0x00000022, 0x00000000, 0x00040047, 0x00000009, 0x00000021, 0x00000000,
  0x00040047, 0x00000015, 0x0000000b, 0x00000019, 0x00020013, 0x00000002, 0x00030021, 0x00000003,
  0x00000002, 0x00030016, 0x00000006, 0x00000020, 0x00090019, 0x00000007, 0x00000006, 0x00000001,
  0x00000000, 0x00000000, 0x00000000, 0x00000002, 0x00000004, 0x00040020, 0x00000008, 0x00000000,
  0x00000007, 0x0004003b, 0x00000008, 0x00000009, 0x00000000, 0x00040015, 0x0000000b, 0x00000020,
  0x00000001, 0x00040017, 0x0000000c, 0x0000000b, 0x00000002, 0x0004002b, 0x0000000b, 0x0000000d,
  0x00000000, 0x0005002c, 0x0000000c, 0x0000000e, 0x0000000d, 0x0000000d, 0x00040017, 0x0000000f,
  0x00000006, 0x00000004, 0x0004002b, 0x00000006, 0x00000010, 0x3f800000, 0x0007002c, 0x0000000f,
  0x00000011, 0x00000010, 0x00000010, 0x00000010, 0x00000010, 0x00040015, 0x00000012, 0x00000020,
  0x00000000, 0x00040017, 0x00000013, 0x00000012, 0x00000003, 0x0004002b, 0x00000012, 0x00000014,
  0x00000001, 0x0006002c, 0x00000013, 0x00000015, 0x00000014, 0x00000014, 0x00000014, 0x00050036,
  0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200f8, 0x00000005, 0x0004003d, 0x00000007,
  0x0000000a, 0x00000009, 0x00040063, 0x0000000a, 0x0000000e, 0x00000011, 0x000100fd, 0x00010038,
};

static bool recordedTextureBinding(bool compute) {
  beginMaterials();
  initAllocator(&thread.stack);
  Layout builtin = { 0 };
  Sampler sampler = { 0 };
  state.builtinLayout = &builtin;
  state.defaultSamplers[FILTER_LINEAR] = &sampler;
  state.pipelines = lovrCalloc(8);
  state.pipelineCount = 0;
  state.limits.workgroupSize[0] = state.limits.workgroupSize[1] = state.limits.workgroupSize[2] = 1;
  state.limits.workgroupCount[0] = state.limits.workgroupCount[1] = state.limits.workgroupCount[2] = 1;
  state.limits.totalWorkgroupSize = 1;
  state.defaultBuffer = lovrBufferCreate(&(BufferInfo) { .size = 16 }, NULL);
  CHECK(state.defaultBuffer);
  state.defaultMaterial = lovrMaterialCreate(NULL);
  CHECK(state.defaultMaterial);
  ShaderSource stages[] = {
    lovrGraphicsGetDefaultShaderSource(SHADER_MASK, STAGE_VERTEX),
    { STAGE_FRAGMENT, NULL, sampleShader, sizeof(sampleShader) }
  };
  ShaderSource storage = { STAGE_COMPUTE, NULL, storageShader, sizeof(storageShader) };
  ShaderInfo shaderInfo = { .type = compute ? SHADER_COMPUTE : SHADER_GRAPHICS,
    .stages = compute ? &storage : stages, .stageCount = compute ? 1 : 2 };
  Shader* shader = lovrShaderCreate(&shaderInfo);
  CHECK(shader);
  TextureInfo textureInfo = *lovrTextureGetInfo(state.defaultTexture);
  textureInfo.usage = TEXTURE_SAMPLE | TEXTURE_STORAGE;
  state.features.formats[FORMAT_RGBA8][0] |= GPU_FEATURE_STORAGE;
  Texture* root = lovrTextureCreate(&textureInfo);
  CHECK(root && lovrGraphicsRegisterSessionTexture(root));
  TextureViewInfo info = { .type = TEXTURE_2D, .layerCount = 1, .levelCount = 1 };
  Texture* view = lovrTextureCreateView(root, &info);
  CHECK(view);
  Pass* pass = lovrPassCreate(NULL);
  lovrPassSetShader(pass, shader);
  CHECK(lovrPassSendTexture(pass, "target", 6, &view, 1));
  gpu_texture* recorded = compute ? view->storageView : view->sampleViewFloat;
  if (compute) {
    CHECK(lovrPassCompute(pass, 1, 1, 1, NULL, 0));
    CHECK(pass->computeCount == 1 && pass->computes[0].bindings[0].texture.object == recorded);
  } else {
    CHECK(lovrPassMeshIndirect(pass, NULL, NULL, state.defaultBuffer, 1, 0, 0));
    CHECK(pass->drawCount == 1 && pass->draws[0].bindings[0].texture.object == recorded);
  }
  CHECK(pass->access[compute ? ACCESS_COMPUTE : ACCESS_RENDER]);
  CHECK(checkPassSession(pass));
  lovrPassSetShader(pass, NULL);
  lovrRelease(view, lovrTextureDestroy);
  lovrRelease(root, lovrTextureDestroy);
  lovrGraphicsInvalidateSessionResources();
  CHECK(!sessionTextures && !pass->sessionDead);
  CHECK(!lovrGraphicsSubmit(&pass, 1));
  CHECK(strstr(lovrGetError(), "invalidated headset session"));
  lovrRelease(pass, lovrPassDestroy);
  lovrRelease(shader, lovrShaderDestroy);
  lovrRelease(state.defaultBuffer, lovrBufferDestroy);
  state.defaultBuffer = NULL;
  lovrRelease(state.defaultMaterial, lovrMaterialDestroy);
  state.defaultMaterial = NULL;
  while (state.layouts) {
    Layout* layout = state.layouts;
    state.layouts = layout->next;
    mtx_destroy(&layout->lock);
    lovrFree(layout);
  }
  lovrFree(state.pipelines);
  state.pipelines = NULL;
  state.pipelineCount = 0;
  lovrFree(thread.stack.memory);
  thread.stack = (Allocator) { 0 };
  state.builtinLayout = NULL;
  state.defaultSamplers[FILTER_LINEAR] = NULL;
  endMaterials();
  return true;
}

static bool recordedSampleBinding(void) { return recordedTextureBinding(false); }
static bool recordedStorageBinding(void) { return recordedTextureBinding(true); }

static bool rejectedIndirect(void) {
  beginMaterials();
  initAllocator(&thread.stack);
  Layout builtin = { 0 };
  state.builtinLayout = &builtin;
  ShaderSource stages[] = {
    lovrGraphicsGetDefaultShaderSource(SHADER_UNLIT, STAGE_VERTEX),
    lovrGraphicsGetDefaultShaderSource(SHADER_UNLIT, STAGE_FRAGMENT)
  };
  ShaderInfo shaderInfo = { .type = SHADER_GRAPHICS, .stages = stages, .stageCount = 2, .isDefault = true };
  Shader* shader = lovrShaderCreate(&shaderInfo);
  CHECK(shader);
  for (unsigned reset = 0; reset < 2; reset++) {
    Texture* root = rootTexture();
    CHECK(lovrGraphicsRegisterSessionTexture(root));
    Material* material = lovrMaterialCreate(root);
    CHECK(material);
    Pass* pass = lovrPassCreate(NULL);
    lovrPassSetMaterial(pass, material);
    lovrPassSetShader(pass, shader);
    lovrGraphicsInvalidateSessionTexture(root);
    CHECK(atomic_load(&material->ref) == 2);
    CHECK(atomic_load(&shader->ref) == 2);
    Buffer draws = { .info.size = 16 };
    uint32_t flags = pass->flags;
    CHECK(!lovrPassMeshIndirect(pass, NULL, NULL, &draws, 1, 0, 0));
    CHECK(strstr(lovrGetError(), "invalidated headset session"));
    CHECK(pass->drawCount == 0 && pass->flags == flags);
    CHECK(!pass->access[ACCESS_RENDER]);
    CHECK(atomic_load(&material->ref) == 2);
    CHECK(atomic_load(&shader->ref) == 2);
    if (reset) lovrPassReset(pass);
    lovrRelease(pass, lovrPassDestroy);
    CHECK(atomic_load(&material->ref) == 1);
    CHECK(atomic_load(&shader->ref) == 1);
    lovrRelease(material, lovrMaterialDestroy);
    lovrRelease(root, lovrTextureDestroy);
  }
  for (unsigned reset = 0; reset < 2; reset++) {
    Material* material = lovrMaterialCreate(NULL);
    CHECK(material);
    DataField format = { .type = TYPE_F32x3, .stride = 12 };
    Buffer* vertices = lovrBufferCreate(&(BufferInfo) { .size = 36, .format = &format }, NULL);
    Buffer* draws = lovrBufferCreate(&(BufferInfo) { .size = 16 }, NULL);
    CHECK(vertices && draws && vertices->supportsMesh);
    Pass* pass = lovrPassCreate(NULL);
    lovrPassSetMaterial(pass, material);
    lovrPassSetShader(pass, shader);
    shader->uniformCount = 1;
    shader->uniformSize = 16;
    pass->uniforms = lovrPassAllocate(pass, 16);
    pass->flags |= DIRTY_UNIFORMS;
    bufferInitFails = true;
    uint32_t flags = pass->flags;
    CHECK(!lovrPassMeshIndirect(pass, vertices, NULL, draws, 1, 0, 0));
    CHECK(strstr(lovrGetError(), "Failed to create GPU buffer"));
    CHECK(pass->drawCount == 0 && pass->flags == flags && pass->pipeline->dirty);
    CHECK(atomic_load(&material->ref) == 2 && atomic_load(&shader->ref) == 2);
    bufferInitFails = false;
    shader->uniformCount = 0;
    shader->uniformSize = 0;
    state.limits.vertexBufferStride = 0;
    CHECK(!lovrPassMeshIndirect(pass, vertices, NULL, draws, 1, 0, 0));
    CHECK(strstr(lovrGetError(), "vertexBufferStride"));
    CHECK(pass->drawCount == 0 && pass->flags == flags);
    CHECK(pass->pipeline->dirty);
    CHECK(atomic_load(&material->ref) == 2 && atomic_load(&shader->ref) == 2);
    if (reset) {
      state.limits.vertexBufferStride = 12;
      CHECK(lovrPassMeshIndirect(pass, vertices, NULL, draws, 1, 0, 0));
      CHECK(pass->drawCount == 1);
      CHECK(atomic_load(&material->ref) == 3 && atomic_load(&shader->ref) == 3);
      lovrPassReset(pass);
    }
    lovrRelease(pass, lovrPassDestroy);
    CHECK(atomic_load(&material->ref) == 1 && atomic_load(&shader->ref) == 1);
    lovrRelease(material, lovrMaterialDestroy);
    lovrRelease(vertices, lovrBufferDestroy);
    lovrRelease(draws, lovrBufferDestroy);
  }
  lovrRelease(shader, lovrShaderDestroy);
  while (state.layouts) {
    Layout* layout = state.layouts;
    state.layouts = layout->next;
    mtx_destroy(&layout->lock);
    lovrFree(layout);
  }
  lovrFree(thread.stack.memory);
  thread.stack = (Allocator) { 0 };
  state.builtinLayout = NULL;
  endMaterials();
  return true;
}

static bool materialViewCascade(void) {
  beginMaterials();
  unsigned before = textureDestroys;
  Texture* root = rootTexture();
  CHECK(lovrGraphicsRegisterSessionTexture(root));
  TextureViewInfo info = { .type = TEXTURE_2D, .layerCount = 1, .levelCount = 1 };
  Texture* sibling = lovrTextureCreateView(root, &info);
  Texture* view = lovrTextureCreateView(root, &info);
  CHECK(sibling && view);
  view->material = lovrMaterialCreate(sibling);
  CHECK(view->material);
  CHECK(lovrTextureToMaterial(sibling));
  lovrRelease(sibling, lovrTextureDestroy);
  lovrRelease(root, lovrTextureDestroy);
  CHECK(atomic_load(&view->ref) == 1);
  lovrGraphicsInvalidateSessionTexture(view);
  CHECK(!sessionTextures);
  CHECK(view->sessionDead && view->root->sessionDead);
  CHECK(!view->material && !view->gpu);
  CHECK(textureDestroys == before + 3 && liveViews == 0);
  CHECK(atomic_load(&view->ref) == 1);
  CHECK(atomic_load(&view->root->ref) == 1);
  runtimeAlive = false;
  lovrRelease(view, lovrTextureDestroy);
  CHECK(textureDestroys == before + 3);
  endMaterials();
  return true;
}

static bool teardownCompletion(void) {
  runtimeAlive = true;
  prepareFails = true;
  drainCalls = 0;
  deferredDestroys = 0;
  Texture* root = rootTexture();
  CHECK(lovrGraphicsRegisterSessionTexture(root));
  CHECK(!lovrGraphicsPrepareSessionTeardown());
  CHECK(!root->sessionDead && root->gpu);
  CHECK(drainCalls == 0 && deferredDestroys == 0);
  prepareFails = false;
  CHECK(lovrGraphicsPrepareSessionTeardown());
  CHECK(root->sessionDead && !root->gpu);
  CHECK(drainCalls == 1 && deferredDestroys == 0);
  runtimeAlive = false;
  lovrRelease(root, lovrTextureDestroy);
  return true;
}

static bool selectiveInvalidation(void) {
  runtimeAlive = true;
  Texture* root = rootTexture();
  Texture* other = rootTexture();
  CHECK(lovrGraphicsRegisterSessionTexture(root));
  CHECK(lovrGraphicsRegisterSessionTexture(other));
  TextureViewInfo info = { .type = TEXTURE_2D, .layerCount = 1, .levelCount = 1 };
  Texture* view = lovrTextureCreateView(root, &info);
  CHECK(view);
  Pass* pass = lovrPassCreate(NULL);
  Pass* otherPass = lovrPassCreate(NULL);
  CHECK(lovrGraphicsRegisterSessionPass(pass));
  CHECK(lovrGraphicsRegisterSessionPass(otherPass));
  lovrGraphicsInvalidateSessionPass(pass);
  lovrGraphicsInvalidateSessionTexture(view);
  CHECK(pass->sessionDead && !otherPass->sessionDead);
  CHECK(root->sessionDead && view->sessionDead && !other->sessionDead);
  lovrGraphicsInvalidateSessionPass(pass);
  lovrGraphicsInvalidateSessionTexture(root);
  lovrRelease(pass, lovrPassDestroy);
  lovrRelease(view, lovrTextureDestroy);
  lovrRelease(root, lovrTextureDestroy);
  lovrGraphicsInvalidateSessionResources();
  CHECK(otherPass->sessionDead && other->sessionDead);
  lovrRelease(otherPass, lovrPassDestroy);
  lovrRelease(other, lovrTextureDestroy);
  return true;
}

int main(int argc, char** argv) {
  NativeTest tests[] = {
    { "graphics.session.texture-lifetime", textureLifetime },
    { "graphics.session.pass-lifetime", passLifetime },
    { "graphics.session.stale-sample", staleSample },
    { "graphics.session.last-root-view", lastRootView },
    { "graphics.session.cached-material", cachedMaterial },
    { "graphics.session.material-root-dies-first", materialRootDiesFirst },
    { "graphics.session.material-dead-texture", materialDeadTexture },
    { "graphics.session.material-pool-reuse", materialPoolReuse },
    { "graphics.session.recorded-sample-binding", recordedSampleBinding },
    { "graphics.session.recorded-storage-binding", recordedStorageBinding },
    { "graphics.session.rejected-indirect", rejectedIndirect },
    { "graphics.session.material-view-cascade", materialViewCascade },
    { "graphics.session.teardown-completion", teardownCompletion },
    { "graphics.session.selective-invalidation", selectiveInvalidation }
  };
  return nativeRunTests(argc, argv, tests, COUNTOF(tests));
}
