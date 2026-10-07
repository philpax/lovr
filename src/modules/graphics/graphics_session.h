#ifndef LOVR_GRAPHICS_SESSION_H
#define LOVR_GRAPHICS_SESSION_H

#include <stdbool.h>

typedef struct Texture Texture;
typedef struct Pass Pass;
typedef struct Material Material;

bool lovrTextureIsValid(Texture* texture);
bool lovrPassIsValid(Pass* pass);
bool lovrMaterialIsValid(Material* material);
bool lovrGraphicsRegisterSessionTexture(Texture* texture);
bool lovrGraphicsRegisterSessionPass(Pass* pass);
void lovrGraphicsInvalidateSessionTexture(Texture* texture);
void lovrGraphicsInvalidateSessionPass(Pass* pass);
void lovrGraphicsInvalidateSessionResources(void);
bool lovrGraphicsPrepareSessionTeardown(void);

#endif
