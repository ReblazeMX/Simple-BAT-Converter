#pragma once

// Shared payload layout between sbc (compiler) and launcher stub.
// Appended after the PE image of the generated executable.
//
// Layout (little-endian), written at EOF:
//   [script bytes][original name UTF-8][nameLen u16][flags u32][scriptSize u64][magic 4]
//
// Magic is at the very end so the launcher can locate it with a tail read.

#include <cstdint>

namespace sbc {

constexpr char kPayloadMagic[4] = {'S', 'B', 'C', '1'};
constexpr std::uint32_t kFlagNoConsole = 1u << 0;
// When set, never pause after the script (even if double-clicked).
constexpr std::uint32_t kFlagNoPause = 1u << 1;
// When set, always pause after the script in a console build.
constexpr std::uint32_t kFlagAlwaysPause = 1u << 2;

#pragma pack(push, 1)
struct PayloadTrailer {
  std::uint16_t nameLen;       // UTF-8 original script base name length
  std::uint32_t flags;
  std::uint64_t scriptSize;
  char magic[4];
};
#pragma pack(pop)

static_assert(sizeof(PayloadTrailer) == 2 + 4 + 8 + 4, "PayloadTrailer size");

} // namespace sbc
