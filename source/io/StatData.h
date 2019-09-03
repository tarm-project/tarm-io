#pragma once

#include <cstdint>

namespace io {

struct StatData {
    struct TimeSpec {
        long seconds;
        long nanoseconds;
    };

    std::uint64_t device_id;
    std::uint64_t mode;
    std::uint64_t hard_links_number;
    std::uint64_t user_id;
    std::uint64_t group_id;
    std::uint64_t special_device_id; // most time is 0
    std::uint64_t inode_number;
    std::uint64_t size;
    std::uint64_t block_size;
    std::uint64_t blocks_number;
    std::uint64_t flags;
    std::uint64_t generation_number; // BSD-specific
    TimeSpec      last_access_time;
    TimeSpec      last_modification_time;
    TimeSpec      last_status_change_time;
    TimeSpec      birth_time;
};

} // namespace io