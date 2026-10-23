// Adapts Luau CodeGen statistics and arena reservation callbacks to the C bridge.
#include "codegen.h"
#include <Luau/CodeGen.h>
#include "lstate.h"
#include <limits>
#if defined(__linux__)
#include <time.h>
#endif
#include <type_traits>

using Luau::CodeGen::CodeGenCompilationResult;
using Luau::CodeGen::CompilationStats;

// Fail at compile time if an upstream field no longer fits the public counters.
static_assert(std::numeric_limits<size_t>::digits <= std::numeric_limits<uint64_t>::digits, "CodeGen byte counts exceed uint64_t");
static_assert(std::is_same<decltype(CompilationStats::bytecodeSizeBytes), size_t>::value, "CodeGen byte count type changed");
static_assert(std::is_same<decltype(CompilationStats::nativeCodeSizeBytes), size_t>::value, "CodeGen byte count type changed");
static_assert(std::is_same<decltype(CompilationStats::nativeDataSizeBytes), size_t>::value, "CodeGen byte count type changed");
static_assert(std::is_same<decltype(CompilationStats::nativeMetadataSizeBytes), size_t>::value, "CodeGen byte count type changed");
static_assert(std::is_same<decltype(CompilationStats::functionsTotal), uint32_t>::value, "CodeGen function count type changed");
static_assert(std::is_same<decltype(CompilationStats::functionsCompiled), uint32_t>::value, "CodeGen function count type changed");
static_assert(std::is_same<decltype(CompilationStats::functionsBound), uint32_t>::value, "CodeGen function count type changed");
static_assert(int(CodeGenCompilationResult::Count) == 10, "Review new CodeGen result values before exposing them");

static void luax_codegenallocation(void* context, void* oldPointer, size_t oldSize, void* newPointer, size_t newSize) {
  LuauMetricsState* state = static_cast<LuauMetricsState*>(context);
  LuauMetrics* metrics = &state->metrics;
  // Retain first so a replacement cannot release the final reference mid-callback.
  if (newPointer) {
    state->references++;
    metrics->codegenReservedBytes += newSize;
    metrics->codegenReservationBytes += newSize;
    metrics->codegenReservationCount++;
    metrics->codegenLiveReservationCount++;
    if (metrics->codegenReservedBytes > metrics->codegenPeakReservedBytes) {
      metrics->codegenPeakReservedBytes = metrics->codegenReservedBytes;
    }
    if (metrics->codegenLiveReservationCount > metrics->codegenPeakReservationCount) {
      metrics->codegenPeakReservationCount = metrics->codegenLiveReservationCount;
    }
  }
  if (oldPointer) {
    metrics->codegenReservedBytes -= oldSize;
    metrics->codegenLiveReservationCount--;
    luax_releaseluaumetrics(state);
  }
}

bool luax_codegenhascontext(lua_State* L) {
  // isNativeExecutionEnabled cannot detect a context whose execution was disabled.
  return L->global->ecb.context != nullptr;
}

bool luax_codegencreate(lua_State* L, LuauMetricsState* state) {
  if (luax_codegenhascontext(L)) return false;
  if (state) {
    Luau::CodeGen::create(L, luax_codegenallocation, state);
  } else {
    Luau::CodeGen::create(L);
  }
  return Luau::CodeGen::isNativeExecutionEnabled(L);
}

static void luax_recordcompile(LuauMetrics* metrics, const CompilationStats& stats, const Luau::CodeGen::CompilationResult& result) {
  metrics->nativeCompileCalls++;
  metrics->nativeFunctionsTotal += stats.functionsTotal;
  metrics->nativeFunctionsCompiled += stats.functionsCompiled;
  metrics->nativeFunctionsBound += stats.functionsBound;
  metrics->nativeBytecodeBytes += stats.bytecodeSizeBytes;
  metrics->nativeCodeBytes += stats.nativeCodeSizeBytes;
  metrics->nativeDataBytes += stats.nativeDataSizeBytes;
  metrics->nativeMetadataBytes += stats.nativeMetadataSizeBytes;
  metrics->lastNativeCompileResult = int32_t(result.result);
  // Unmarked modules and already compiled functions are normal skipped work.
  if (result.result != CodeGenCompilationResult::Success &&
      result.result != CodeGenCompilationResult::NothingToCompile &&
      result.result != CodeGenCompilationResult::NotNativeModule) {
    metrics->nativeCompileResultErrors++;
  }
  metrics->nativeCompileFailures += result.protoFailures.size();
}

void luax_codegencompile(lua_State* L, int index, int nativeScope, LuauMetricsState* state) {
  // Tiered compilation treats `--!native` and `@native` as requests to compile at load, like the marked scope.
  bool markedOnly = nativeScope == LUAU_NATIVE_MARKED || nativeScope == LUAU_NATIVE_TIERED;
  unsigned int flags = markedOnly ? Luau::CodeGen::CodeGen_OnlyNativeModules : 0;
  if (!state) {
    Luau::CodeGen::compile(L, index, flags);
    return;
  }

  LuauMetrics* metrics = &state->metrics;
  CompilationStats stats;
  LuauMetricsClock start = luax_luaumetricsclock(metrics);
  Luau::CodeGen::CompilationResult result = Luau::CodeGen::compile(L, index, flags, &stats);
  luax_luaumetricselapsed(metrics, start, &metrics->nativeCompileElapsedNs, &metrics->nativeCompileCpuNs);
  luax_recordcompile(metrics, stats, result);
}

bool luax_codegensettierup(lua_State* L, int threshold, int queueCapacity) {
  if (threshold < 0 || queueCapacity < 0 || !luax_codegenhascontext(L)) return false;
  Luau::CodeGen::setHotThreshold(L, threshold);
  Luau::CodeGen::setHotQueueCapacity(L, queueCapacity);
  return true;
}

int luax_codegenhotqueuesize(lua_State* L) {
  return Luau::CodeGen::getHotQueueSize(L);
}

int luax_codegenpromote(lua_State* L, size_t instructionBudget, LuauMetricsState* state) {
  if (Luau::CodeGen::getHotQueueSize(L) == 0) return 0;

  CompilationStats stats;
  if (!state) {
    Luau::CodeGen::compileHot(L, instructionBudget, &stats);
    return int(stats.functionsBound);
  }

  LuauMetrics* metrics = &state->metrics;
  LuauMetricsClock start = luax_luaumetricsclock(metrics);
  Luau::CodeGen::CompilationResult result = Luau::CodeGen::compileHot(L, instructionBudget, &stats);
  uint64_t elapsedNs = 0;
  uint64_t cpuNs = 0;
  luax_luaumetricselapsed(metrics, start, &elapsedNs, &cpuNs);
  metrics->nativeCompileElapsedNs += elapsedNs;
  metrics->nativeCompileCpuNs += cpuNs;
  metrics->hotPromotionElapsedNs += elapsedNs;
  metrics->hotPromotionCpuNs += cpuNs;
  metrics->hotPromotionCalls++;
  metrics->hotFunctionsPromoted += stats.functionsBound;
  luax_recordcompile(metrics, stats, result);
  return int(stats.functionsBound);
}

// A promotion batch handed to a worker. The worker cannot touch the VM's metrics, so it records its own clocks for the bind to report.
struct LuauHotJob {
  Luau::CodeGen::HotBatch* batch;
  uint64_t elapsedNs;
  uint64_t cpuNs;
  uint64_t takeElapsedNs;
  uint64_t takeCpuNs;
};

// Clocks are read as the metrics bridge reads them, and report nothing where the platform lacks them.
static bool luax_readclock(bool threadCpu, uint64_t* ns) {
#if defined(__linux__)
  struct timespec time;
  if (clock_gettime(threadCpu ? CLOCK_THREAD_CPUTIME_ID : CLOCK_MONOTONIC, &time) != 0) return false;
  *ns = (uint64_t) time.tv_sec * 1000000000 + time.tv_nsec;
  return true;
#else
  (void) threadCpu;
  (void) ns;
  return false;
#endif
}

void* luax_codegentakehot(lua_State* L, size_t instructionBudget, LuauMetricsState* state) {
  if (Luau::CodeGen::getHotQueueSize(L) == 0) return nullptr;
  LuauMetricsClock start = {};
  if (state) start = luax_luaumetricsclock(&state->metrics);
  Luau::CodeGen::HotBatch* batch = Luau::CodeGen::takeHot(L, instructionBudget);
  if (!batch) return nullptr;
  LuauHotJob* job = new LuauHotJob{batch, 0, 0, 0, 0};
  if (state) luax_luaumetricselapsed(&state->metrics, start, &job->takeElapsedNs, &job->takeCpuNs);
  return job;
}

void luax_codegencompilehot(void* opaque) {
  LuauHotJob* job = static_cast<LuauHotJob*>(opaque);
  uint64_t elapsedStart = 0, cpuStart = 0, elapsedEnd = 0, cpuEnd = 0;
  bool elapsed = luax_readclock(false, &elapsedStart);
  bool cpu = luax_readclock(true, &cpuStart);
  Luau::CodeGen::compileHotBatch(job->batch);
  if (elapsed && luax_readclock(false, &elapsedEnd)) job->elapsedNs = elapsedEnd - elapsedStart;
  if (cpu && luax_readclock(true, &cpuEnd)) job->cpuNs = cpuEnd - cpuStart;
}

int luax_codegenbindhot(lua_State* L, void* opaque, LuauMetricsState* state) {
  LuauHotJob* job = static_cast<LuauHotJob*>(opaque);
  CompilationStats stats;
  LuauMetricsClock start = {};
  if (state) start = luax_luaumetricsclock(&state->metrics);
  Luau::CodeGen::CompilationResult result = Luau::CodeGen::bindHotBatch(L, job->batch, &stats);
  if (state) {
    LuauMetrics* metrics = &state->metrics;
    uint64_t bindElapsedNs = job->takeElapsedNs;
    uint64_t bindCpuNs = job->takeCpuNs;
    luax_luaumetricselapsed(metrics, start, &bindElapsedNs, &bindCpuNs);
    metrics->hotBindElapsedNs += bindElapsedNs;
    metrics->hotBindCpuNs += bindCpuNs;
    metrics->nativeCompileElapsedNs += job->elapsedNs;
    metrics->nativeCompileCpuNs += job->cpuNs;
    metrics->hotPromotionElapsedNs += job->elapsedNs;
    metrics->hotPromotionCpuNs += job->cpuNs;
    metrics->hotPromotionCalls++;
    metrics->hotFunctionsPromoted += stats.functionsBound;
    luax_recordcompile(metrics, stats, result);
  }
  delete job;
  return int(stats.functionsBound);
}

void luax_codegendiscardhot(lua_State* L, void* opaque) {
  LuauHotJob* job = static_cast<LuauHotJob*>(opaque);
  Luau::CodeGen::discardHotBatch(L, job->batch);
  delete job;
}
