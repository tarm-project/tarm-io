#include "StatData.h"

#include "detail/Common.h"

#include <cstddef>

namespace io {

// These checks are required to be sure that we can safe reinterpret cast uv_stat_t to StatData
static_assert(sizeof(StatData) == sizeof(uv_stat_t), "");
static_assert(offsetof(StatData, device_id)               == offsetof(uv_stat_t, st_dev), "");
static_assert(offsetof(StatData, mode)                    == offsetof(uv_stat_t, st_mode), "");
static_assert(offsetof(StatData, hard_links_number)       == offsetof(uv_stat_t, st_nlink), "");
static_assert(offsetof(StatData, user_id)                 == offsetof(uv_stat_t, st_uid), "");
static_assert(offsetof(StatData, group_id)                == offsetof(uv_stat_t, st_gid), "");
static_assert(offsetof(StatData, special_device_id)       == offsetof(uv_stat_t, st_rdev), "");
static_assert(offsetof(StatData, inode_number)            == offsetof(uv_stat_t, st_ino), "");
static_assert(offsetof(StatData, size)                    == offsetof(uv_stat_t, st_size), "");
static_assert(offsetof(StatData, block_size)              == offsetof(uv_stat_t, st_blksize), "");
static_assert(offsetof(StatData, blocks_number)           == offsetof(uv_stat_t, st_blocks), "");
static_assert(offsetof(StatData, flags)                   == offsetof(uv_stat_t, st_flags), "");
static_assert(offsetof(StatData, generation_number)       == offsetof(uv_stat_t, st_gen), "");
static_assert(offsetof(StatData, last_access_time)        == offsetof(uv_stat_t, st_atim), "");
static_assert(offsetof(StatData, last_modification_time)  == offsetof(uv_stat_t, st_mtim), "");
static_assert(offsetof(StatData, last_status_change_time) == offsetof(uv_stat_t, st_ctim), "");
static_assert(offsetof(StatData, birth_time)              == offsetof(uv_stat_t, st_birthtim), "");

// uv_timespec_t to StatData::TimeSpec
static_assert(sizeof(StatData::TimeSpec) == sizeof(uv_timespec_t), "");
static_assert(offsetof(StatData::TimeSpec, seconds)     == offsetof(uv_timespec_t, tv_sec), "");
static_assert(offsetof(StatData::TimeSpec, nanoseconds) == offsetof(uv_timespec_t, tv_nsec), "");

} // namespace io