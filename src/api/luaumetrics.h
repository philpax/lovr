// Shared value types for the Luau C API and its CodeGen C++ bridge.
#pragma once

#include <stdint.h>

typedef enum {
  LUAU_NATIVE_NONE = 0,
  LUAU_NATIVE_ALL = 1,
  LUAU_NATIVE_MARKED = 2,
  // Marked modules compile at load; other functions compile when the host promotes them after they become hot.
  LUAU_NATIVE_TIERED = 3
} LuauNativeScope;

// Counters are cumulative per VM; snapshots can be subtracted without resetting live gauges.
// Times cover bridge calls only.  CPU time is the calling thread's CLOCK_THREAD_CPUTIME_ID.
// Availability is zero when the platform or a clock read cannot provide that time domain.
typedef struct {
  uint64_t bytecodeCompileCalls;
  uint64_t bytecodeCompileInputBytes;
  uint64_t bytecodeCompileOutputBytes;
  uint64_t bytecodeCompileElapsedNs;
  uint64_t bytecodeCompileCpuNs;
  uint64_t bytecodeLoadCalls;
  uint64_t bytecodeLoadInputBytes;
  uint64_t bytecodeLoadElapsedNs;
  uint64_t bytecodeLoadCpuNs;
  uint64_t nativeCompileCalls;
  // Luau CompilationStats totals describe generated work, not current resident memory.
  // Metadata counts instruction-offset mappings only, as reported by Luau.
  uint64_t nativeFunctionsTotal;
  uint64_t nativeFunctionsCompiled;
  uint64_t nativeFunctionsBound;
  uint64_t nativeBytecodeBytes;
  uint64_t nativeCodeBytes;
  uint64_t nativeDataBytes;
  uint64_t nativeMetadataBytes;
  // Failed top-level results exclude Success, NothingToCompile, and NotNativeModule.
  uint64_t nativeCompileResultErrors;
  // Number of per-function failure records, including partial compilation failures.
  uint64_t nativeCompileFailures;
  uint64_t nativeCompileElapsedNs;
  uint64_t nativeCompileCpuNs;
  // Hot promotion is a subset of the native totals above: calls that compiled queued functions, the functions they bound, and their time.
  uint64_t hotPromotionCalls;
  uint64_t hotFunctionsPromoted;
  uint64_t hotPromotionElapsedNs;
  uint64_t hotPromotionCpuNs;
  // CodeGen callbacks report whole arena block reservations, not live executable code.
  uint64_t codegenReservedBytes;
  uint64_t codegenPeakReservedBytes;
  uint64_t codegenReservationBytes;
  uint64_t codegenReservationCount;
  uint64_t codegenLiveReservationCount;
  uint64_t codegenPeakReservationCount;
  // The Luau CodeGenCompilationResult value; -1 means no native compile call.
  int32_t lastNativeCompileResult;
  uint32_t elapsedClockAvailable;
  uint32_t threadCpuClockAvailable;
  uint32_t nativeCodegenAvailable;
} LuauMetrics;
