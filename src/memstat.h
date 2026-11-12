// Tiny cross-platform memory probe (enabled with LF_MEM=1). Reports resident set, peak resident set and bytes live in the malloc heap.
#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#else
#include <malloc.h>
#endif
#include <atomic>
namespace mem {
// C++-heap accounting (operator new/delete, Linux only): isolates app-owned memory from the GL driver (Mesa llvmpipe keeps textures in malloc).
inline std::atomic<size_t> gLive{0}, gPeak{0};
inline double appMB() { return gLive.load(std::memory_order_relaxed) / 1048576.0; }
inline double appPeakMB() { return gPeak.load(std::memory_order_relaxed) / 1048576.0; }
struct Snap { double rssMB, peakMB, heapMB; };
inline Snap snap() {
  Snap s{0, 0, 0};
#if defined(_WIN32)
  PROCESS_MEMORY_COUNTERS pmc; if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc)) { s.rssMB = pmc.WorkingSetSize / 1048576.0; s.peakMB = pmc.PeakWorkingSetSize / 1048576.0; s.heapMB = pmc.PagefileUsage / 1048576.0; }
#elif defined(__APPLE__)
  mach_task_basic_info info; mach_msg_type_number_t n = MACH_TASK_BASIC_INFO_COUNT; if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &n) == KERN_SUCCESS) { s.rssMB = info.resident_size / 1048576.0; s.peakMB = info.resident_size_max / 1048576.0; s.heapMB = s.rssMB; }
#else
  if (FILE* f = fopen("/proc/self/status", "r")) { char l[160]; while (fgets(l, sizeof l, f)) { long kb; if (sscanf(l, "VmRSS: %ld", &kb) == 1) s.rssMB = kb / 1024.0; else if (sscanf(l, "VmHWM: %ld", &kb) == 1) s.peakMB = kb / 1024.0; } fclose(f); }
#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33))
  struct mallinfo2 mi = mallinfo2(); s.heapMB = (mi.uordblks + mi.hblkhd) / 1048576.0;
#endif
#endif
  return s;
}
inline bool on() { static int v = -1; if (v < 0) v = getenv("LF_MEM") ? 1 : 0; return v == 1; }
inline void mark(const char* what) { if (!on()) return; Snap s = snap(); fprintf(stderr, "MEM %-26s rss %7.1f MB  peak %7.1f MB  heap %7.1f MB  | app C++ %6.1f MB (peak %6.1f)\n", what, s.rssMB, s.peakMB, s.heapMB, appMB(), appPeakMB()); }
}
