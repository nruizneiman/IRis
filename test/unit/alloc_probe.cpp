#include "alloc_probe.h"

#include <cstdlib>
#include <new>

namespace {
size_t g_live = 0;
size_t g_peak = 0;
constexpr size_t kHeader = 16;  // keeps 16-byte alignment and stores the block size

void* allocate(size_t n) {
  void* p = std::malloc(n + kHeader);
  if (!p) throw std::bad_alloc();
  *static_cast<size_t*>(p) = n;
  g_live += n;
  if (g_live > g_peak) g_peak = g_live;
  return static_cast<char*>(p) + kHeader;
}

void release(void* p) noexcept {
  if (!p) return;
  char* base = static_cast<char*>(p) - kHeader;
  g_live -= *reinterpret_cast<size_t*>(base);
  std::free(base);
}
}  // namespace

size_t allocLiveBytes() { return g_live; }
size_t allocPeakBytes() { return g_peak; }
void allocResetPeak() { g_peak = g_live; }

#ifndef IRIS_NO_ALLOC_PROBE
void* operator new(size_t n) { return allocate(n); }
void* operator new[](size_t n) { return allocate(n); }
void operator delete(void* p) noexcept { release(p); }
void operator delete[](void* p) noexcept { release(p); }
void operator delete(void* p, size_t) noexcept { release(p); }
void operator delete[](void* p, size_t) noexcept { release(p); }
#endif
