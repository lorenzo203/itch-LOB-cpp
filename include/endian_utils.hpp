#pragma once

#include <cstdint>

namespace itch {

// Byte-swap 16-bit integer (Big-Endian to Little-Endian and vice versa)
[[nodiscard]] constexpr inline uint16_t bswap16(uint16_t val) noexcept {
    return __builtin_bswap16(val);
}

// Byte-swap 32-bit integer
[[nodiscard]] constexpr inline uint32_t bswap32(uint32_t val) noexcept {
    return __builtin_bswap32(val);
}

// Byte-swap 64-bit integer
[[nodiscard]] constexpr inline uint64_t bswap64(uint64_t val) noexcept {
    return __builtin_bswap64(val);
}

// Decode 6-byte (48-bit) Big-Endian ITCH timestamp into nanoseconds since midnight
[[nodiscard]] constexpr inline uint64_t decode_timestamp(const uint8_t ts[6]) noexcept {
    return (static_cast<uint64_t>(ts[0]) << 40) |
           (static_cast<uint64_t>(ts[1]) << 32) |
           (static_cast<uint64_t>(ts[2]) << 24) |
           (static_cast<uint64_t>(ts[3]) << 16) |
           (static_cast<uint64_t>(ts[4]) << 8)  |
           (static_cast<uint64_t>(ts[5]));
}

} // namespace itch