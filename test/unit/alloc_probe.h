// Counts heap use in the test executable (global operator new/delete are replaced in alloc_probe.cpp),
// so tests can assert memory budgets. Numbers are for a 64-bit PC: pointers are twice as big as on the
// ESP32, so they overestimate what the device needs - the right direction for a budget check.
#pragma once

#include <cstddef>

size_t allocLiveBytes();
size_t allocPeakBytes();
void allocResetPeak();  // peak := live
