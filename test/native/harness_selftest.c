#include "test.h"

static bool passing(void) {
  CHECK(true);
  return true;
}

static bool failing(void) {
  CHECK(false);
  return true;
}

static bool afterFailure(void) {
  puts("after-failure-ran");
  return true;
}

int main(int argc, char** argv) {
  const NativeTest tests[] = {
    { "harness.passing", passing }
  };
  if (argc == 2 && strcmp(argv[1], "--fail-child") == 0) {
    const NativeTest failures[] = {
      { "harness.intentional-failure", failing },
      { "harness.after-failure", afterFailure }
    };
    return nativeRunTests(1, argv, failures, sizeof(failures) / sizeof(failures[0]));
  }
  return nativeRunTests(argc, argv, tests, sizeof(tests) / sizeof(tests[0]));
}
