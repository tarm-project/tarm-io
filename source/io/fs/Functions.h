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

} // namespace fs
} // namespace io
} // namespace tarm
