#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace recomp {
    struct MemoryWrite {
        uint32_t address;
        uint32_t mask;
        std::array<uint8_t,32> bytes{}; // N64 byte order; only masked bytes are valid.
    };
    // Initialize before guest workers start. Watching and collection belong to
    // the graphics thread; guest/native producers may record writes concurrently.
    void initialize_memory_writes(uint8_t *rdram,uint32_t size);
    void watch_memory_writes(uint32_t address,uint32_t size,bool enable);
    std::vector<MemoryWrite> collect_memory_writes();
}
