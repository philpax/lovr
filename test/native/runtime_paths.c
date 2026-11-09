#include "headset/runtime_paths.h"
#include "test.h"
#include "util.h"

typedef struct {
  const unsigned char* bytes;
  size_t length;
  size_t offset;
  bool fail;
} ManifestReader;

static bool readManifest(void* context, void* buffer, size_t capacity, size_t* length) {
  ManifestReader* reader = context;
  if (reader->fail) return false;
  *length = reader->length - reader->offset;
  if (*length > capacity) *length = capacity;
  if (*length > 3) *length = 3;
  if (*length) memcpy(buffer, reader->bytes + reader->offset, *length);
  reader->offset += *length;
  return true;
}

static bool configuration(void) {
  CHECK(lovrHeadsetSetRuntimePaths(NULL, NULL));
  HeadsetRuntimePaths paths = lovrHeadsetGetRuntimePaths();
  CHECK(!paths.manifest && !paths.library);
  char manifest[] = "/runtime/active.json";
  char library[] = "/runtime/libmonado.so";
  CHECK(lovrHeadsetSetRuntimePaths(manifest, library));
  CHECK(!lovrHeadsetGetRuntimePaths().monadoStageUnmapped);
  CHECK(lovrHeadsetSetMonadoStageUnmapped(true));
  CHECK(lovrHeadsetGetRuntimePaths().monadoStageUnmapped);
  manifest[1] = library[1] = 'x';
  paths = lovrHeadsetGetRuntimePaths();
  CHECK(!strcmp(paths.manifest, "/runtime/active.json"));
  CHECK(!strcmp(paths.library, "/runtime/libmonado.so"));
  CHECK(lovrHeadsetSetRuntimePaths(paths.manifest, paths.library));
  CHECK(!lovrHeadsetGetRuntimePaths().monadoStageUnmapped);
  CHECK(!lovrHeadsetValidateRuntimeManifest());
  CHECK(!lovrHeadsetSetRuntimeManifestSnapshot(NULL, 1));
  unsigned char snapshot[] = { '{', '}', 0, '\n' };
  CHECK(lovrHeadsetSetRuntimeManifestSnapshot(snapshot, sizeof(snapshot)));
  snapshot[0] = '[';
  paths = lovrHeadsetGetRuntimePaths();
  CHECK(paths.snapshotLength == sizeof(snapshot));
  CHECK(((const unsigned char*) paths.snapshot)[0] == '{');
  CHECK(lovrHeadsetSetRuntimeManifestSnapshot(paths.snapshot, paths.snapshotLength));
  const unsigned char exact[] = { '{', '}', 0, '\n' };
  ManifestReader reader = { .bytes = exact, .length = sizeof(exact) };
  CHECK(lovrHeadsetValidateRuntimeManifestWithReader(readManifest, &reader));
  reader = (ManifestReader) { .bytes = exact, .length = sizeof(exact) - 1 };
  CHECK(!lovrHeadsetValidateRuntimeManifestWithReader(readManifest, &reader));
  reader = (ManifestReader) { .bytes = snapshot, .length = sizeof(snapshot) };
  CHECK(!lovrHeadsetValidateRuntimeManifestWithReader(readManifest, &reader));
  const unsigned char longer[] = { '{', '}', 0, '\n', ' ' };
  reader = (ManifestReader) { .bytes = longer, .length = sizeof(longer) };
  CHECK(!lovrHeadsetValidateRuntimeManifestWithReader(readManifest, &reader));
  reader = (ManifestReader) { .fail = true };
  CHECK(!lovrHeadsetValidateRuntimeManifestWithReader(readManifest, &reader));
  CHECK(!lovrHeadsetValidateRuntimeManifestWithReader(NULL, NULL));
  const char* invalid[][2] = {
    { "/runtime/active.json", NULL },
    { NULL, "/runtime/libmonado.so" },
    { "runtime/active.json", "/runtime/libmonado.so" },
    { "/runtime/active.json", "runtime/libmonado.so" },
    { "", "/runtime/libmonado.so" },
    { "/runtime/active.json", "" }
  };
  paths = lovrHeadsetGetRuntimePaths();
  for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
    CHECK(!lovrHeadsetSetRuntimePaths(invalid[i][0], invalid[i][1]));
    CHECK(strstr(lovrGetError(), "openxr: runtime paths:"));
    HeadsetRuntimePaths unchanged = lovrHeadsetGetRuntimePaths();
    CHECK(unchanged.manifest == paths.manifest && unchanged.library == paths.library);
  }
  CHECK(lovrHeadsetSetRuntimePaths(NULL, NULL));
  paths = lovrHeadsetGetRuntimePaths();
  CHECK(!paths.manifest && !paths.library && !paths.snapshot && !paths.snapshotLength);
  CHECK(!lovrHeadsetSetRuntimeManifestSnapshot("{}", 2));
  CHECK(lovrHeadsetSetRuntimePaths("/runtime/active.json", "/runtime/libmonado.so"));
  CHECK(lovrHeadsetSetRuntimeManifestSnapshot("", 0));
  reader = (ManifestReader) { 0 };
  CHECK(lovrHeadsetValidateRuntimeManifestWithReader(readManifest, &reader));
  CHECK(lovrHeadsetSetRuntimeManifestSnapshot(NULL, 0));
  CHECK(!lovrHeadsetValidateRuntimeManifestWithReader(readManifest, &reader));
  CHECK(lovrHeadsetSetRuntimePaths(NULL, NULL));
  return true;
}

static bool lease(void) {
  CHECK(lovrHeadsetSetRuntimePaths(NULL, NULL));
  lovrHeadsetRuntimePathsLock(true);
  CHECK(!lovrHeadsetSetRuntimePaths(NULL, NULL));
  CHECK(!lovrHeadsetSetRuntimePaths("/runtime/active.json", "/runtime/libmonado.so"));
  lovrHeadsetRuntimePathsLock(false);
  CHECK(lovrHeadsetSetRuntimePaths("/runtime/active.json", "/runtime/libmonado.so"));
  CHECK(lovrHeadsetSetRuntimeManifestSnapshot("{}", 2));
  HeadsetRuntimePaths paths = lovrHeadsetGetRuntimePaths();
  lovrHeadsetRuntimePathsLock(true);
  CHECK(!lovrHeadsetSetMonadoStageUnmapped(true));
  CHECK(!lovrHeadsetSetRuntimeManifestSnapshot("[]", 2));
  CHECK(!lovrHeadsetSetRuntimeManifestSnapshot(NULL, 0));
  CHECK(!lovrHeadsetSetRuntimePaths(NULL, NULL));
  CHECK(!lovrHeadsetSetRuntimePaths("/other/active.json", "/other/libmonado.so"));
  HeadsetRuntimePaths unchanged = lovrHeadsetGetRuntimePaths();
  CHECK(unchanged.manifest == paths.manifest && unchanged.library == paths.library);
  CHECK(unchanged.snapshot == paths.snapshot && unchanged.snapshotLength == paths.snapshotLength);
  lovrHeadsetRuntimePathsLock(false);
  CHECK(lovrHeadsetSetRuntimePaths(NULL, NULL));
  return true;
}

int main(int argc, char** argv) {
  NativeTest tests[] = {
    { "runtime.paths.configuration", configuration },
    { "runtime.paths.lease", lease }
  };
  return nativeRunTests(argc, argv, tests, sizeof(tests) / sizeof(tests[0]));
}
