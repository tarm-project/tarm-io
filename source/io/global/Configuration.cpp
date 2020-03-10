#include "Configuration.h"

#include <uv.h>

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

namespace detail {

bool is_buffer_size_available(uv_handle_t* handle,
                              std::size_t size,
                              int(*accessor_function)(uv_handle_t* handle, int* value)) {
    //if (size >= 4096) {
    //    return true;
    //} else return false;

    if (size == 0) {
        return false;
    }

    auto size_to_set = static_cast<int>(size);
    auto setter_error = accessor_function(handle, &size_to_set);
    if (setter_error) {
        return false;
    }

    int size_to_get = 0;
    auto getter_error = accessor_function(handle, &size_to_get);
    if (getter_error) {
        return false;
    }

    std::size_t size_multiplier = 1;
#if defined(__linux__)
    // For details read http://man7.org/linux/man-pages/man7/socket.7.html
    // SO_RCVBUF option or similar ones.
    size_multiplier = 2;
#endif

    if (size_to_get != size_to_set * size_multiplier) {
        return false;
    }

    return true;
}

int init_handle_for_buffer_size(uv_loop_t& loop, uv_udp_t& handle) {
    const auto loop_error = uv_loop_init(&loop);
    if (loop_error) {
        return 0;
    }

    const auto init_error = uv_udp_init(&loop, &handle);
    if (init_error) {
        return 0;
    }

    ::sockaddr_storage storage{0};
    storage.ss_family = AF_INET;
    const auto bind_error = uv_udp_bind(&handle, reinterpret_cast<const ::sockaddr*>(&storage), UV_UDP_REUSEADDR);
    if (bind_error) {
        return 0;
    }

    return 0;
}

void close_handle_for_buffer_size(uv_loop_t& loop, uv_udp_t& handle) {
    uv_close(reinterpret_cast<uv_handle_t*>(&handle), nullptr);
    uv_run(&loop, UV_RUN_ONCE);
    uv_loop_close(&loop);
}

enum class BufferSizeSearchDirection {
    MIN,
    MAX
};

std::size_t bound_buffer_size(uv_handle_t& handle,
                              int(*accessor_function)(uv_handle_t* handle, int* value),
                              std::size_t lower_bound,
                              std::size_t upper_bound,
                              BufferSizeSearchDirection direction) {
    auto value_to_test = (upper_bound + lower_bound) / 2;

    while (lower_bound + 1 < upper_bound) {
        if (detail::is_buffer_size_available(reinterpret_cast<uv_handle_t*>(&handle), value_to_test, accessor_function)) {
            if (direction == BufferSizeSearchDirection::MIN) {
                upper_bound = value_to_test;
            } else {
                lower_bound = value_to_test;
            }
        } else {
            if (direction == BufferSizeSearchDirection::MIN) {
                lower_bound = value_to_test;
            } else {
                upper_bound = value_to_test;
            }
        }

        value_to_test = (upper_bound + lower_bound) / 2;
    }

    if (detail::is_buffer_size_available(reinterpret_cast<uv_handle_t*>(&handle), upper_bound, accessor_function)) {
        return direction == BufferSizeSearchDirection::MIN ?
               static_cast<std::size_t>(upper_bound) :
               static_cast<std::size_t>(lower_bound);
    }

    return direction == BufferSizeSearchDirection::MIN ?
               static_cast<std::size_t>(lower_bound) :
               static_cast<std::size_t>(upper_bound);
}

std::size_t min_buffer_size(std::size_t& value,
                            int(*accessor_function)(uv_handle_t* handle, int* value)) {
    uv_loop_t loop;
    uv_udp_t handle;
    auto init_error = detail::init_handle_for_buffer_size(loop, handle);
    if (init_error) {
        return 0;
    }

    auto lower_bound = std::size_t(0);
    auto upper_bound = default_receive_buffer_size();
    value = detail::bound_buffer_size(*reinterpret_cast<uv_handle_t*>(&handle),
                                      &::uv_recv_buffer_size,
                                      lower_bound,
                                      upper_bound,
                                      detail::BufferSizeSearchDirection::MIN);

    detail::close_handle_for_buffer_size(loop, handle);

    return value;
}

std::size_t default_buffer_size(std::size_t& value,
                                int(*accessor_function)(uv_handle_t* handle, int* value)) {
    uv_loop_t loop;
    uv_udp_t handle;
    auto init_error = detail::init_handle_for_buffer_size(loop, handle);
    if (init_error) {
        return 0;
    }

    int size_value = 0;
    const auto size_error = uv_recv_buffer_size(reinterpret_cast<uv_handle_t*>(&handle), &size_value);
    if (size_error) {
        return 0;
    }

    detail::close_handle_for_buffer_size(loop, handle);

    value = static_cast<std::size_t>(size_value);

    return value;
}

std::size_t max_buffer_size(std::size_t& value,
                            int(*accessor_function)(uv_handle_t* handle, int* value)) {
    uv_loop_t loop;
    uv_udp_t handle;

    auto init_error = detail::init_handle_for_buffer_size(loop, handle);
    if (init_error) {
        return 0;
    }

    auto lower_bound = default_receive_buffer_size();
    auto upper_bound = default_receive_buffer_size();
    while (detail::is_buffer_size_available(reinterpret_cast<uv_handle_t*>(&handle), upper_bound, &::uv_recv_buffer_size) ) {
        upper_bound *= 2;
    }

    value = detail::bound_buffer_size(*reinterpret_cast<uv_handle_t*>(&handle),
                                      accessor_function,
                                      lower_bound,
                                      upper_bound,
                                      detail::BufferSizeSearchDirection::MAX);

    detail::close_handle_for_buffer_size(loop, handle);

    return value;
}

} // namespace detail

std::size_t g_min_receive_buffer_size = 0;
std::size_t g_default_receive_buffer_size = 0;
std::size_t g_max_receive_buffer_size = 0;

std::size_t g_min_send_buffer_size = 0;
std::size_t g_default_send_buffer_size = 0;
std::size_t g_max_send_buffer_size = 0;

std::size_t min_receive_buffer_size() {
    return detail::min_buffer_size(g_min_receive_buffer_size, &::uv_recv_buffer_size);
}

std::size_t default_receive_buffer_size() {
    return detail::default_buffer_size(g_default_receive_buffer_size, &::uv_recv_buffer_size);
}

std::size_t max_receive_buffer_size() {
    return detail::max_buffer_size(g_max_receive_buffer_size, &::uv_recv_buffer_size);
}

std::size_t min_send_buffer_size() {
    return detail::min_buffer_size(g_min_send_buffer_size, &::uv_send_buffer_size);
}

std::size_t default_send_buffer_size() {
    return detail::default_buffer_size(g_default_send_buffer_size, &::uv_send_buffer_size);
}

std::size_t max_send_buffer_size() {
    return detail::max_buffer_size(g_max_send_buffer_size, &::uv_send_buffer_size);
}


} // namespace global
} // namespace io
