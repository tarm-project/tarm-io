/*----------------------------------------------------------------------------------------------
*  Copyright (c) 2020 - present Alexander Voitenko
*  Licensed under the MIT License. See License.txt in the project root for license information.
*----------------------------------------------------------------------------------------------*/

#pragma once

#include "Error.h"
#include "EventLoop.h"
#include "Export.h"
#include "Path.h"
#include "StatData.h"

#include <functional>

namespace tarm {
namespace io {
namespace fs {

using StatCallback = std::function<void(const Path&, const StatData&, const Error&)>;
IO_DLL_PUBLIC void stat(EventLoop& loop, const Path& path, const StatCallback& callback);

IO_DLL_PUBLIC bool is_regular_file(const StatData& stat_data);
IO_DLL_PUBLIC bool is_directory(const StatData& stat_data);
IO_DLL_PUBLIC bool is_symbolic_link(const StatData& stat_data);
IO_DLL_PUBLIC bool is_block_special(const StatData& stat_data);
IO_DLL_PUBLIC bool is_char_special(const StatData& stat_data);
IO_DLL_PUBLIC bool is_fifo(const StatData& stat_data);
IO_DLL_PUBLIC bool is_socket(const StatData& stat_data);


} // namespace fs
} // namespace io
} // namespace tarm

