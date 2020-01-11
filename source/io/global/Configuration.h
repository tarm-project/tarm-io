#pragma once

#include "io/Export.h"
#include "io/Logger.h"

namespace io {
namespace global {

IO_DLL_PUBLIC
Logger::Callback logger_callback();

IO_DLL_PUBLIC
void set_logger_callback(Logger::Callback callback);

} // namespace global
} // namespace io
