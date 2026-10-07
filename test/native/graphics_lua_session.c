#define main graphicsSessionMain
#include "graphics_session.c"
#undef main
#include "../../src/api/api.c"
#include "../../src/api/l_graphics_pass.c"
#include "../../src/api/l_graphics_material.c"
#include <lualib.h>

StringEntry lovrMaterialTexture[] = {
  [TEXTURE_COLOR] = ENTRY("color"),
  [TEXTURE_GLOW] = ENTRY("glow"),
  [TEXTURE_METALNESS] = ENTRY("metalness"),
  [TEXTURE_ROUGHNESS] = ENTRY("roughness"),
  [TEXTURE_CLEARCOAT] = ENTRY("clearcoat"),
  [TEXTURE_OCCLUSION] = ENTRY("occlusion"),
  [TEXTURE_NORMAL] = ENTRY("normal"),
  { 0 }
};

static bool luaSessionGuards(void) {
  beginMaterials();
  Texture* texture = rootTexture();
  CHECK(lovrGraphicsRegisterSessionTexture(texture));
  Pass* pass = lovrPassCreate("Session");
  Pass* live = lovrPassCreate("Live");
  CHECK(lovrGraphicsRegisterSessionPass(pass));
  Material* material = lovrMaterialCreate(texture);
  CHECK(material);
  const luaL_Reg methods[] = {
    { "origin", l_lovrPassOrigin },
    { "translate", l_lovrPassTranslate },
    { "setColor", l_lovrPassSetColor },
    { "setMaterial", l_lovrPassSetMaterial },
    { "getLabel", l_lovrPassGetLabel },
    { NULL, NULL }
  };
  lua_State* L = luaL_newstate();
  CHECK(L);
  luaL_openlibs(L);
  _luax_registertype(L, T_Pass, "Pass", lovrPassDestroy, methods);
  _luax_registertype(L, T_Material, "Material", lovrMaterialDestroy, lovrMaterial);
  const luaL_Reg textureMethods[] = { { NULL, NULL } };
  _luax_registertype(L, T_Texture, "Texture", lovrTextureDestroy, textureMethods);
  luax_pushtype(L, Pass, pass);
  lua_setglobal(L, "dead");
  luax_pushtype(L, Pass, live);
  lua_setglobal(L, "live");
  luax_pushtype(L, Material, material);
  lua_setglobal(L, "material");
  lovrGraphicsInvalidateSessionResources();
  const char* script =
    "local function rejects(f) "
    "local ok, err = pcall(f) "
    "assert(not ok and string.find(err, 'invalidated headset session', 1, true), tostring(err)) end "
    "rejects(function() dead:origin() end) "
    "rejects(function() dead:translate(1, 2, 3) end) "
    "rejects(function() dead:setColor(1, 1, 1) end) "
    "rejects(function() dead:setMaterial(nil) end) "
    "rejects(function() live:setMaterial(material) end) "
    "assert(dead:getLabel() == 'Session') "
    "assert(type(material:getProperties()) == 'table') "
    "live:origin() live:translate(1, 2, 3) live:setColor(1, 1, 1)";
  bool success = luax_loadbufferx(L, script, strlen(script), "session guards", "t") == 0 && lua_pcall(L, 0, 0, 0) == 0;
  if (!success) fprintf(stderr, "%s\n", lua_tostring(L, -1));
  lua_close(L);
  lovrRelease(pass, lovrPassDestroy);
  lovrRelease(live, lovrPassDestroy);
  lovrRelease(texture, lovrTextureDestroy);
  CHECK(success);
  CHECK(atomic_load(&material->ref) == 1);
  lovrRelease(material, lovrMaterialDestroy);
  endMaterials();
  return true;
}

int main(int argc, char** argv) {
  const NativeTest tests[] = {
    { "graphics.session.lua-boundary", luaSessionGuards }
  };
  return nativeRunTests(argc, argv, tests, COUNTOF(tests));
}
