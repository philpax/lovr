#ifndef LOVR_NATIVE_TEST_H
#define LOVR_NATIVE_TEST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
    return false; \
  } \
} while (0)

typedef struct {
  const char* name;
  bool (*run)(void);
} NativeTest;

static int nativeRunTests(int argc, char** argv, const NativeTest* tests, size_t count) {
  bool list = argc == 2 && strcmp(argv[1], "--list") == 0;
  const char* selected = argc == 3 && strcmp(argv[1], "--case") == 0 ? argv[2] : NULL;
  if (selected) {
    bool found = false;
    for (size_t i = 0; i < count; i++) found |= strcmp(tests[i].name, selected) == 0;
    if (!found) {
      fprintf(stderr, "unknown test case: %s\n", selected);
      return 2;
    }
  }
  if (argc > 1 && !list && !selected) {
    fprintf(stderr, "unknown test argument: %s\n", argv[1]);
    return 2;
  }
  size_t failures = 0;
  size_t executed = 0;
  for (size_t i = 0; i < count; i++) {
    if (list) {
      puts(tests[i].name);
      continue;
    }
    if (selected && strcmp(tests[i].name, selected) != 0) continue;
    executed++;
    bool passed = tests[i].run();
    printf("%s %s\n", passed ? "PASS" : "FAIL", tests[i].name);
    failures += !passed;
  }
  if (!list) {
    printf("%zu tests, %zu failures\n", executed, failures);
  }
  return failures ? 1 : 0;
}

#endif
