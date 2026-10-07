#ifndef LOVR_GRAPHICS_EXTERNAL_H
#define LOVR_GRAPHICS_EXTERNAL_H

#include "core/gpu.h"
#include "graphics/graphics.h"

typedef bool (*lovrGraphicsExternalCallback)(const gpu_external_image* image, void* data);

bool lovrGraphicsHandoffTexture(Texture* texture, lovrGraphicsExternalCallback callback, void* data, uint32_t* completion);

#endif
