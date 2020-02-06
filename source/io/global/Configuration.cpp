#include "Configuration.h"

namespace io {
namespace global {

Logger::Callback g_logger_callback = nullptr;
std::string g_ciphers_list{"ALL:!SHA256:!SHA384:!aPSK:!ECDSA+SHA1:!ADH:!LOW:!EXP:!MD5"};

Logger::Callback logger_callback() {
    return g_logger_callback;
}

void set_logger_callback(Logger::Callback callback) {
    g_logger_callback = callback;
}

void set_ciphers_list(const std::string& ciphers) {
    g_ciphers_list = ciphers;
}

const std::string& ciphers_list() {
    return g_ciphers_list;
}

} // namespace global
} // namespace io
