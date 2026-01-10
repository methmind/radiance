//
// Created by sexey on 08.01.2026.
//

module;
#include <cstdint>

export module radiance.memory.metadata;

export namespace radiance::memory
{
    struct alignas(16) block_cookie_s
    {
        size_t size; // total block size (including cookie)
        uint32_t magic;
    };

    static_assert(sizeof(block_cookie_s) == 16, "block_cookie_s must be 16 bytes");

    struct free_block_s
    {
        uint8_t* start;
        size_t size;

        bool operator<(const free_block_s& other) const {
            return start < other.start;
        }
    };
}