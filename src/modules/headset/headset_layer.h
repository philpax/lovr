#pragma once

#include "headset/headset_ops.h"
#include <stdatomic.h>

typedef struct LayerHeader {
  atomic_uint ref;
  LayerInfo info;
  const HeadsetOps* creator;
  uint32_t generation;
} LayerHeader;

bool lovrLayerIsValid(Layer* layer);

static inline const HeadsetOps* lovrLayerGetCreator(Layer* layer) {
  return ((LayerHeader*) layer)->creator;
}
