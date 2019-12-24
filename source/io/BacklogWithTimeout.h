#pragma once

#include "Timer.h"
#include "EventLoop.h"
#include "Common.h" // TODO: remove (replace uv_hrtime with custom function)

#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

#include <assert.h>

namespace io {

// TODO: event loop as default template parameter
template<typename T, typename TimerType = ::io::Timer>
class BacklogWithTimeout {
public:
    using TimeGetterType = std::function<std::uint64_t(const T&)>;
    using MonothonicClockGetterType = std::uint64_t(*)();
    using OnItemExpiredCallback = std::function<void(BacklogWithTimeout<T, TimerType>&,  const T& item)>;

    BacklogWithTimeout(EventLoop& loop, std::size_t entity_timeout, OnItemExpiredCallback expired_callback, TimeGetterType time_getter, MonothonicClockGetterType clock_getter = &uv_hrtime) :
        m_entity_timeout(entity_timeout * std::size_t(1000000)),
        m_expired_callback(expired_callback),
        m_time_getter(time_getter),
        m_clock_getter(clock_getter) {

        std::size_t buckets_count = 0;
        std::size_t current_timeout = entity_timeout;
        do {
            m_timers.emplace_back(new TimerType(loop));
            m_timers.back().get()->start(current_timeout, current_timeout, std::bind(&BacklogWithTimeout::on_timer, this, std::placeholders::_1));
            m_timeouts.push_back(current_timeout * 1000000);
            m_timers.back().get()->set_user_data(reinterpret_cast<void*>(m_timers.size() - 1)); // index of the timer
            current_timeout /= 2;
            ++buckets_count;
        } while (current_timeout > 1);

        m_items.resize(buckets_count);
    };

    // TODO: implement reserve() method as it ads more performance

    bool add_item(T t) {
        const std::uint64_t current_time = m_clock_getter();
        const std::uint64_t item_time = m_time_getter(t);
        if (current_time < item_time) { // item from the future
            return false;
        }

        if (m_timers.empty()) {
            return false;
        }

        const auto time_diff = current_time - item_time;
        if (time_diff >= m_entity_timeout) {
            m_expired_callback(*this, t);
            if (m_stopped) { // BacklogWithTimeout object could be stopped in callback
                return false;
            }

            return true;
        }

        auto index = bucket_index_from_time(time_diff);
        m_items[index].push_back(t);

        return true;
    }

    void stop() {
        // TODO: FIXME: need to add schedule_removal to timers to fix this
        // TODO: remove m_stopped after this;
        for (auto& t : m_timers) {
            t->stop();
            m_stopped = true;
        }

        /*
        m_entity_timeout = 0;
        m_expired_callback = nullptr;
        m_time_getter = nullptr;
        m_clock_getter = nullptr;
        m_timers.clear();
        m_timeouts.clear();
        m_items.clear();
        */
    }

protected:
    void on_timer(TimerType& timer) {
        const std::size_t timer_index = reinterpret_cast<std::size_t>(timer.user_data());
        const std::vector<T>& current_bucket = m_items[timer_index];

        std::vector<T> bucket_replacement;
        bucket_replacement.reserve(current_bucket.size());

        for (std::size_t i = 0; i < current_bucket.size(); ++i) {
            const std::uint64_t current_time = m_clock_getter();
            const std::uint64_t item_time = m_time_getter(current_bucket[i]);

            const auto time_diff = current_time - item_time;
            if (time_diff >= m_entity_timeout) {
                m_expired_callback(*this, current_bucket[i]);
                if (m_stopped) { // BacklogWithTimeout object could be stopped in callback
                    return;
                }

                continue;
            }

            auto index = bucket_index_from_time(time_diff);
            if (index != timer_index) {
                m_items[index].push_back(current_bucket[i]);
            } else {
                bucket_replacement.push_back(current_bucket[i]);
            }
        }

        m_items[timer_index].swap(bucket_replacement);
    }

    std::size_t bucket_index_from_time(std::uint64_t time) const {
        assert(!m_timeouts.empty());

        // Linear search on tiny sets is much more faster than tricks with logarithms or unordered map
        for(std::size_t i = 0; i < m_timers.size(); ++i) {
            if (time >= m_timeouts[i]) {
                return i;
            }
        }

        return m_timers.size() - 1;
    }

private:
    std::size_t m_entity_timeout = 0;
    OnItemExpiredCallback m_expired_callback;
    TimeGetterType m_time_getter = nullptr;
    MonothonicClockGetterType m_clock_getter = nullptr;
    std::vector<std::unique_ptr<TimerType>> m_timers;
    std::vector<std::size_t> m_timeouts; // TODO: get timeout from timer directly?
    std::vector<std::vector<T>> m_items;

    bool m_stopped = false;
};

} // namespace io
