#pragma once

#include "io/Logger.h"

namespace io {
namespace global {

Logger::Callback logger_callback();
void set_logger_callback(Logger::Callback callback);

} // namespace global
} // namespace io