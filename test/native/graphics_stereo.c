// Stereo canvas playback through the real graphics.c recording path, with a fake GPU layer that
// records render passes, draws, camera offsets, pipelines, and timestamp writes.
#include "../../src/modules/graphics/graphics.c"
#define LOVR_LAYER_GRAPHICS_HARNESS
#define main graphicsSessionMain
#define gpu_stream_begin unused_stream_begin
#define gpu_stream_end unused_stream_end
#define gpu_submit unused_submit
#define gpu_sync unused_sync
#define gpu_buffer_flush unused_buffer_flush
#define gpu_render_begin unused_render_begin
#define gpu_render_end unused_render_end
#define gpu_set_viewport unused_set_viewport
#define gpu_set_scissor unused_set_scissor
#define gpu_push_constants unused_push_constants
#define gpu_bind_pipeline unused_bind_pipeline
#define gpu_bind_bundles unused_bind_bundles
#define gpu_bind_vertex_buffers unused_bind_vertex_buffers
#define gpu_draw unused_draw
#define gpu_clear_tally unused_clear_tally
#define gpu_copy_tally_buffer unused_copy_tally_buffer
#define gpu_pipeline_init_graphics unused_pipeline_init_graphics
#define os_get_time unused_get_time
#include "graphics_session.c"
#undef main
#undef gpu_stream_begin
#undef gpu_stream_end
#undef gpu_submit
#undef gpu_sync
#undef gpu_buffer_flush
#undef gpu_render_begin
#undef gpu_render_end
#undef gpu_set_viewport
#undef gpu_set_scissor
#undef gpu_push_constants
#undef gpu_bind_pipeline
#undef gpu_bind_bundles
#undef gpu_bind_vertex_buffers
#undef gpu_draw
#undef gpu_clear_tally
#undef gpu_copy_tally_buffer
#undef gpu_pipeline_init_graphics
#undef os_get_time

#define MAX_RECORDED_PASSES 4

typedef struct {
  gpu_canvas canvas;
  // Copies, because submission passes timestamp writes that live only for the duration of its call.
  bool hasBeginTimestamps;
  gpu_timestamp_writes beginTimestamps;
  bool hasEndTimestamps;
  gpu_timestamp_writes endTimestamps;
  bool ended;
  unsigned draws;
  unsigned pipelineBinds;
  unsigned viewports;
  unsigned cameraBinds;
  uint32_t cameraOffset;
} RecordedPass;

static RecordedPass recorded[MAX_RECORDED_PASSES];
static unsigned renderBegins;
static bool insideRender;
static unsigned graphicsPipelines;
static gpu_pipeline_info lastPipelineInfo;
static gpu_texture streamToken;

static bool sameTimestamps(const gpu_timestamp_writes* a, const gpu_timestamp_writes* b) {
  return a->tally == b->tally && a->beginIndex == b->beginIndex && a->endIndex == b->endIndex;
}

static RecordedPass* currentPass(void) {
  if (!insideRender || renderBegins == 0) abort();
  return &recorded[renderBegins - 1];
}

size_t gpu_sizeof_texture(void) { return sizeof(gpu_texture); }
bool gpu_texture_init(gpu_texture* texture, gpu_texture_info* info) { (void) info; *texture = (gpu_texture) { 0 }; return true; }
bool gpu_texture_init_view(gpu_texture* texture, gpu_texture_view_info* info) {
  if (!info->source || info->source->destroyed) abort();
  *texture = (gpu_texture) { .view = true };
  return true;
}
void gpu_texture_destroy(gpu_texture* texture) {
  if (texture->destroyed) abort();
  texture->destroyed = true;
}
bool gpu_wait_idle(void) { return true; }
bool gpu_prepare_teardown(void) { return true; }
bool gpu_quiesce_locked(void) { return true; }
void gpu_flush_deferred(void) {}
void gpu_flush_deferred_after_idle(void) {}
// An active headset supplies the frame time, so recording never reads the timer module.
bool lovrHeadsetIsActive(void) { return true; }
double lovrHeadsetGetDisplayTime(void) { return 0.; }
double os_get_time(void) { return 0.; }

gpu_stream* gpu_stream_begin(const char* label) { (void) label; return (gpu_stream*) &streamToken; }
bool gpu_stream_end(gpu_stream* stream) { return stream == (gpu_stream*) &streamToken; }
bool gpu_submit(gpu_stream** streams, uint32_t count, uint32_t tick) { (void) streams; (void) count; (void) tick; return true; }
void gpu_sync(gpu_stream* stream, gpu_barrier* barriers, uint32_t count) { (void) stream; (void) barriers; (void) count; }
void gpu_buffer_flush(gpu_buffer* buffer, uint32_t offset, uint32_t extent) { (void) buffer; (void) offset; (void) extent; }
void gpu_clear_tally(gpu_stream* stream, gpu_tally* tally, uint32_t index, uint32_t count) { (void) stream; (void) tally; (void) index; (void) count; }
void gpu_copy_tally_buffer(gpu_stream* stream, gpu_tally* src, gpu_buffer* dst, uint32_t srcIndex, uint32_t dstOffset, uint32_t count) {
  (void) stream; (void) src; (void) dst; (void) srcIndex; (void) dstOffset; (void) count;
}
bool gpu_pipeline_init_graphics(gpu_pipeline* pipeline, gpu_pipeline_info* info, bool* slow) {
  (void) pipeline;
  graphicsPipelines++;
  lastPipelineInfo = *info;
  if (slow) *slow = false;
  return true;
}

void gpu_render_begin(gpu_stream* stream, gpu_canvas* canvas, gpu_timestamp_writes* timestamps) {
  (void) stream;
  if (insideRender || renderBegins >= MAX_RECORDED_PASSES) abort();
  recorded[renderBegins] = (RecordedPass) { .canvas = *canvas, .hasBeginTimestamps = timestamps != NULL };
  if (timestamps) recorded[renderBegins].beginTimestamps = *timestamps;
  renderBegins++;
  insideRender = true;
}
void gpu_render_end(gpu_stream* stream, gpu_canvas* canvas, gpu_timestamp_writes* timestamps) {
  (void) stream;
  RecordedPass* pass = currentPass();
  if (canvas->color[0].texture != pass->canvas.color[0].texture || canvas->color[0].resolve != pass->canvas.color[0].resolve) abort();
  pass->hasEndTimestamps = timestamps != NULL;
  if (timestamps) pass->endTimestamps = *timestamps;
  pass->ended = true;
  insideRender = false;
}
void gpu_set_viewport(gpu_stream* stream, float viewport[4], float depthRange[2]) { (void) stream; (void) viewport; (void) depthRange; currentPass()->viewports++; }
void gpu_set_scissor(gpu_stream* stream, uint32_t scissor[4]) { (void) stream; (void) scissor; currentPass(); }
void gpu_push_constants(gpu_stream* stream, gpu_shader* shader, void* data, uint32_t size) { (void) stream; (void) shader; (void) data; (void) size; currentPass(); }
void gpu_bind_pipeline(gpu_stream* stream, gpu_pipeline* pipeline, gpu_pipeline_type type) {
  (void) stream; (void) pipeline;
  if (type != GPU_PIPELINE_GRAPHICS) abort();
  currentPass()->pipelineBinds++;
}
// The builtin bundle is set 0, and it is the only one bound with the camera and draw data offsets.
void gpu_bind_bundles(gpu_stream* stream, gpu_shader* shader, gpu_bundle** bundles, uint32_t first, uint32_t count, uint32_t* dynamicOffsets, uint32_t dynamicOffsetCount) {
  (void) stream; (void) shader; (void) bundles; (void) count;
  RecordedPass* pass = currentPass();
  if (first == 0) {
    if (dynamicOffsetCount != 2) abort();
    pass->cameraOffset = dynamicOffsets[0];
    pass->cameraBinds++;
  }
}
void gpu_bind_vertex_buffers(gpu_stream* stream, gpu_buffer** buffers, uint32_t* offsets, uint32_t first, uint32_t count) {
  (void) stream; (void) buffers; (void) offsets; (void) first; (void) count; currentPass();
}
void gpu_draw(gpu_stream* stream, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t baseInstance) {
  (void) stream; (void) instanceCount; (void) firstVertex; (void) baseInstance;
  if (vertexCount != 3) abort();
  currentPass()->draws++;
}

typedef struct {
  Layout builtin;
  Sampler sampler;
  Shader* shader;
} Recording;

static Texture* createTexture(TextureFormat format, uint32_t size, uint32_t layers, uint32_t usage) {
  TextureInfo info = { .type = layers > 1 ? TEXTURE_ARRAY : TEXTURE_2D, .format = format, .width = size, .height = size,
    .layers = layers, .mipmaps = 1, .samples = 1, .usage = usage };
  Texture* texture = lovrTextureCreate(&info);
  if (!texture) abort();
  return texture;
}

static Texture* colorTexture(void) { return createTexture(FORMAT_RGBA8, 64, 1, TEXTURE_SAMPLE | TEXTURE_RENDER); }
static Texture* densityMap(void) { return createTexture(FORMAT_RG8, 4, 1, TEXTURE_FOVEATION); }

static bool beginRecording(Recording* recording) {
  memset(recorded, 0, sizeof(recorded));
  renderBegins = graphicsPipelines = 0;
  insideRender = false;
  beginMaterials();
  initAllocator(&thread.stack);
  state.features.sampleCounts |= 4;
  state.features.formats[FORMAT_D32F][0] = GPU_FEATURE_RENDER;
  state.features.formats[FORMAT_RG8][0] = GPU_FEATURE_SAMPLE | GPU_FEATURE_RENDER;
  state.limits.vertexBufferStride = 64;
  *recording = (Recording) { 0 };
  CHECK(mtx_init(&recording->builtin.lock, mtx_plain) == thrd_success);
  state.builtinLayout = &recording->builtin;
  state.defaultSamplers[FILTER_LINEAR] = &recording->sampler;
  state.defaultBuffer = lovrBufferCreate(&(BufferInfo) { .size = 16 }, NULL);
  CHECK(state.defaultBuffer);
  state.defaultMaterial = lovrMaterialCreate(NULL);
  CHECK(state.defaultMaterial);
  state.stream = (gpu_stream*) &streamToken;
  ShaderSource stages[] = {
    lovrGraphicsGetDefaultShaderSource(SHADER_UNLIT, STAGE_VERTEX),
    lovrGraphicsGetDefaultShaderSource(SHADER_UNLIT, STAGE_FRAGMENT)
  };
  ShaderInfo info = { .type = SHADER_GRAPHICS, .stages = stages, .stageCount = 2, .isDefault = true };
  recording->shader = lovrShaderCreate(&info);
  CHECK(recording->shader);
  return true;
}

static void endRecording(Recording* recording) {
  if (state.timingReadback) {
    lovrRelease(state.timingReadback, lovrReadbackDestroy);
    state.timingReadback = NULL;
  }
  while (state.readbacks) {
    Readback* readback = state.readbacks;
    state.readbacks = readback->next;
    lovrRelease(readback, lovrReadbackDestroy);
  }
  if (state.timestamps) {
    gpu_tally_destroy(state.timestamps);
    lovrFree(state.timestamps);
    state.timestamps = NULL;
    state.timestampCount = 0;
  }
  state.timingEnabled = false;
  lovrRelease(recording->shader, lovrShaderDestroy);
  lovrRelease(state.defaultBuffer, lovrBufferDestroy);
  state.defaultBuffer = NULL;
  lovrRelease(state.defaultMaterial, lovrMaterialDestroy);
  state.defaultMaterial = NULL;
  for (size_t i = 0; i < COUNTOF(state.bufferAllocators); i++) {
    destroyBuffers(&state.bufferAllocators[i]);
    state.bufferAllocators[i] = (BufferAllocator) { 0 };
  }
  while (state.layouts) {
    Layout* layout = state.layouts;
    state.layouts = layout->next;
    mtx_destroy(&layout->lock);
    lovrFree(layout);
  }
  mtx_destroy(&recording->builtin.lock);
  lovrFree(thread.stack.memory);
  thread.stack = (Allocator) { 0 };
  state.builtinLayout = NULL;
  state.defaultSamplers[FILTER_LINEAR] = NULL;
  state.stream = NULL;
  endMaterials();
}

static bool drawTriangle(Pass* pass, Shader* shader) {
  lovrPassSetShader(pass, shader);
  return lovrPassDraw(pass, &(DrawInfo) { .mode = DRAW_TRIANGLES, .vertex.format = VERTEX_EMPTY, .count = 3 });
}

static bool submit(Pass* pass) {
  return lovrGraphicsSubmit(&pass, 1) && !insideRender;
}

static bool singleDraw(void) {
  Recording recording;
  CHECK(beginRecording(&recording));
  state.timingEnabled = true;
  Texture* left = colorTexture();
  Texture* right = colorTexture();
  Pass* pass = lovrPassCreate("Stereo");
  Canvas canvas = { .color[0].texture = left, .stereo = right, .depthFormat = FORMAT_D32F };
  CHECK(lovrPassSetCanvas(pass, &canvas));
  CHECK(pass->views == 2 && !pass->tempColor[0] && pass->tempDepth);
  CHECK(pass->pipeline->info.viewCount == 1);
  CHECK(drawTriangle(pass, recording.shader));
  CHECK(pass->drawCount == 1 && pass->draws[0].pipelineInfo->viewCount == 1);
  CHECK(submit(pass));

  CHECK(renderBegins == 2);
  CHECK(recorded[0].ended && recorded[1].ended);
  CHECK(recorded[0].canvas.color[0].texture == left->renderView && !recorded[0].canvas.color[0].resolve);
  CHECK(recorded[1].canvas.color[0].texture == right->renderView && !recorded[1].canvas.color[0].resolve);
  CHECK(left->renderView != right->renderView);
  CHECK(recorded[0].canvas.depth.texture && recorded[0].canvas.depth.texture == recorded[1].canvas.depth.texture);
  CHECK(recorded[0].canvas.width == 64 && recorded[1].canvas.width == 64);
  for (unsigned v = 0; v < 2; v++) {
    CHECK(recorded[v].draws == 1);
    CHECK(recorded[v].pipelineBinds == 1 && recorded[v].viewports == 1 && recorded[v].cameraBinds == 1);
  }
  CHECK(recorded[0].cameraOffset == 0);
  CHECK(recorded[1].cameraOffset == recorded[0].cameraOffset + sizeof(Camera));

  CHECK(graphicsPipelines == 1 && lastPipelineInfo.viewCount == 1 && !lastPipelineInfo.foveated);

  // Timing gives the render pass a tally, so the writes carry real indices.
  CHECK(recorded[0].hasBeginTimestamps && !recorded[0].hasEndTimestamps);
  CHECK(!recorded[1].hasBeginTimestamps && recorded[1].hasEndTimestamps);
  CHECK(sameTimestamps(&recorded[0].beginTimestamps, &recorded[1].endTimestamps));
  CHECK(recorded[1].endTimestamps.tally == state.timestamps && state.timestamps);
  CHECK(recorded[1].endTimestamps.beginIndex == 0 && recorded[1].endTimestamps.endIndex == 1);

  lovrRelease(pass, lovrPassDestroy);
  lovrRelease(left, lovrTextureDestroy);
  lovrRelease(right, lovrTextureDestroy);
  endRecording(&recording);
  return true;
}

static bool multisampled(void) {
  Recording recording;
  CHECK(beginRecording(&recording));
  Texture* left = colorTexture();
  Texture* right = colorTexture();
  Pass* pass = lovrPassCreate(NULL);
  Canvas canvas = { .color[0].texture = left, .stereo = right, .depthFormat = FORMAT_D32F, .samples = 4 };
  CHECK(lovrPassSetCanvas(pass, &canvas));
  CHECK(pass->tempColor[0] && pass->canvas.samples == 4);
  gpu_texture* temporary = pass->target.color[0].texture;
  CHECK(temporary && temporary != left->renderView);
  CHECK(drawTriangle(pass, recording.shader));
  CHECK(submit(pass));

  CHECK(renderBegins == 2);
  CHECK(recorded[0].canvas.color[0].texture == temporary && recorded[0].canvas.color[0].resolve == left->renderView);
  CHECK(recorded[1].canvas.color[0].texture == temporary && recorded[1].canvas.color[0].resolve == right->renderView);
  CHECK(recorded[0].canvas.color[0].save == GPU_SAVE_OP_DISCARD && recorded[1].canvas.color[0].save == GPU_SAVE_OP_DISCARD);
  CHECK(recorded[0].draws == 1 && recorded[1].draws == 1);
  CHECK(recorded[1].cameraOffset == recorded[0].cameraOffset + sizeof(Camera));
  CHECK(lastPipelineInfo.viewCount == 1 && lastPipelineInfo.multisample.count == 4);

  lovrRelease(pass, lovrPassDestroy);
  lovrRelease(left, lovrTextureDestroy);
  lovrRelease(right, lovrTextureDestroy);
  endRecording(&recording);
  return true;
}

static bool densityMaps(void) {
  Recording recording;
  CHECK(beginRecording(&recording));
  Texture* left = colorTexture();
  Texture* right = colorTexture();
  Texture* leftMap = densityMap();
  Texture* rightMap = densityMap();
  Pass* pass = lovrPassCreate(NULL);
  Canvas canvas = { .color[0].texture = left, .stereo = right, .foveation = leftMap, .stereoFoveation = rightMap,
    .depthFormat = FORMAT_D32F };
  CHECK(lovrPassSetCanvas(pass, &canvas));
  CHECK(drawTriangle(pass, recording.shader));
  CHECK(submit(pass));
  CHECK(renderBegins == 2);
  CHECK(recorded[0].canvas.foveation == leftMap->gpu && recorded[1].canvas.foveation == rightMap->gpu);
  CHECK(lastPipelineInfo.viewCount == 1 && lastPipelineInfo.foveated);

  // Each rejection leaves the accepted canvas in place.
  Texture* small = createTexture(FORMAT_RGBA8, 32, 1, TEXTURE_SAMPLE | TEXTURE_RENDER);
  Texture* other = createTexture(FORMAT_RG8, 64, 1, TEXTURE_SAMPLE | TEXTURE_RENDER);
  Texture* layered = createTexture(FORMAT_RGBA8, 64, 2, TEXTURE_SAMPLE | TEXTURE_RENDER);
  Texture* depth = createTexture(FORMAT_D32F, 64, 1, TEXTURE_RENDER);
  Texture* mipmapped = lovrTextureCreate(&(TextureInfo) { .type = TEXTURE_2D, .format = FORMAT_RGBA8, .width = 64,
    .height = 64, .layers = 1, .mipmaps = 2, .samples = 1, .usage = TEXTURE_SAMPLE | TEXTURE_RENDER });
  Texture* swapchain = colorTexture();
  CHECK(mipmapped && swapchain);
  swapchain->info.xr = true;
  struct { Canvas canvas; const char* error; } rejected[] = {
    { { .color[0].texture = left, .stereo = right, .foveation = leftMap }, "density map for both views or neither" },
    { { .color[0].texture = left, .stereo = right, .stereoFoveation = rightMap }, "density map for both views or neither" },
    { { .color[0].texture = left, .foveation = leftMap, .stereoFoveation = rightMap }, "Only a stereo canvas has a second density map" },
    { { .color[0].texture = left, .stereo = small }, "Canvas texture sizes must match" },
    { { .color[0].texture = left, .stereo = other }, "Stereo canvas texture formats must match" },
    { { .color[0].texture = layered, .stereo = right }, "Stereo canvas textures must have a single layer" },
    { { .color[0].texture = left, .stereo = right, .depth.texture = depth }, "temporary depth buffer" },
    { { .color[0].texture = left, .stereo = mipmapped }, "can not have mipmaps" },
    { { .color[0].texture = mipmapped, .stereo = right }, "can not have mipmaps" },
    { { .color[0].texture = left, .stereo = swapchain }, "XR swapchain" },
    { { .color[0].texture = swapchain, .stereo = right }, "XR swapchain" }
  };
  for (size_t i = 0; i < COUNTOF(rejected); i++) {
    CHECK(!lovrPassSetCanvas(pass, &rejected[i].canvas));
    CHECK(strstr(lovrGetError(), rejected[i].error));
    CHECK(pass->canvas.stereo == right && pass->canvas.stereoFoveation == rightMap && pass->views == 2);
  }

  lovrRelease(pass, lovrPassDestroy);
  swapchain->info.xr = false;
  Texture* textures[] = { left, right, leftMap, rightMap, small, other, layered, depth, mipmapped, swapchain };
  for (size_t i = 0; i < COUNTOF(textures); i++) {
    CHECK(atomic_load(&textures[i]->ref) == 1);
    lovrRelease(textures[i], lovrTextureDestroy);
  }
  endRecording(&recording);
  return true;
}

static bool mono(void) {
  Recording recording;
  CHECK(beginRecording(&recording));
  Texture* single = colorTexture();
  Texture* layered = createTexture(FORMAT_RGBA8, 64, 2, TEXTURE_SAMPLE | TEXTURE_RENDER);
  Texture* targets[] = { single, layered };
  for (uint32_t t = 0; t < COUNTOF(targets); t++) {
    memset(recorded, 0, sizeof(recorded));
    renderBegins = 0;
    Pass* pass = lovrPassCreate(NULL);
    Canvas canvas = { .color[0].texture = targets[t], .depthFormat = FORMAT_D32F };
    CHECK(lovrPassSetCanvas(pass, &canvas));
    CHECK(pass->views == t + 1);
    CHECK(drawTriangle(pass, recording.shader));
    CHECK(submit(pass));
    CHECK(renderBegins == 1 && recorded[0].ended);
    CHECK(recorded[0].canvas.color[0].texture == targets[t]->renderView);
    CHECK(recorded[0].draws == 1 && recorded[0].cameraOffset == 0);
    CHECK(recorded[0].hasBeginTimestamps && recorded[0].hasEndTimestamps);
    CHECK(sameTimestamps(&recorded[0].beginTimestamps, &recorded[0].endTimestamps));
    CHECK(lastPipelineInfo.viewCount == t + 1);
    lovrRelease(pass, lovrPassDestroy);
  }
  lovrRelease(single, lovrTextureDestroy);
  lovrRelease(layered, lovrTextureDestroy);
  endRecording(&recording);
  return true;
}

int main(int argc, char** argv) {
  NativeTest tests[] = {
    { "graphics.stereo.single-draw", singleDraw },
    { "graphics.stereo.multisampled", multisampled },
    { "graphics.stereo.density-maps", densityMaps },
    { "graphics.stereo.mono", mono }
  };
  return nativeRunTests(argc, argv, tests, COUNTOF(tests));
}
