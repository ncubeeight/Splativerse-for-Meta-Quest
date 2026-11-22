#pragma once
#include <cstddef>

// Shared by the application and the pinned SPZ decoder. These are allocation
// bounds, not a guarantee of interactive performance at the maximum size.
namespace svlimits {
constexpr size_t maxPoints = 1000000;
constexpr size_t maxFileBytes = 128 * 1024 * 1024;
// SPZ v3 / SH3 is the largest supported record: 65 bytes plus a 16-byte header.
constexpr size_t maxInflatedBytes = 16 + 65 * maxPoints;
}
