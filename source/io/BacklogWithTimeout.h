#pragma once

#include "CommonMacros.h"
#include "Timer.h"
#include "EventLoop.h"

#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

#include <assert.h>

namespace io {

template<typename T, typename LoopType = ::io::EventLoop, typename TimerType = ::io::Timer>
class BacklogWithTimeout {
public:
    using OnItemExpiredCallback = std::function<void(BacklogWithTimeout<T, LoopType, TimerType>&, const T& item)>;
    using ItemTimeGetter = std::function<std::uint64_t(const T&)>;
    using MonothonicClockGetterType = std::uint64_t(*)();

    // TODO: copy and move constuctors

    BacklogWithTimeout(LoopType& loop, std::size_t entity_timeout, OnItemExpiredCallback expired_callback, ItemTimeGetter time_getter, MonothonicClockGetterType clock_getter) :
        m_entity_timeout(entity_timeout * std::size_t(1000000)),
        m_expired_callback(expired_callback),
        m_time_getter(time_getter),
        m_clock_getter(clock_getter) {

        std::size_t buckets_count = 0;
        std::size_t current_timeout = entity_timeout;
        do {
            m_timers.emplace_back(new TimerType(loop), TimerType::default_delete());
            m_timers.back().get()->start(current_timeout, current_timeout, std::bind(&BacklogWithTimeout::on_timer, this, std::placeholders::_1));
            m_timers.back().get()->set_user_data(reinterpret_cast<void*>(m_timers.size() - 1)); // index of the timer
            current_timeout /= 2;
            ++buckets_count;
        } while (current_timeout > 1);

        m_items.resize(buckets_count);
    };

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
            if (m_timers.empty()) { // BacklogWithTimeout object could be stopped in callback
                return false;
            }

            return true;
        }

        auto index = bucket_index_from_time(m_entity_timeout - time_diff);
        m_items[index].push_back(t);

        return true;
    }

    bool remove_item(const T& t) {
        for(std::size_t i = 0; i < m_timers.size(); ++i) {
            auto& items = m_items[i];
            auto it = std::find(items.begin(), items.end(), t);
            if (it != items.end()) {
                items.erase(it);
                return true;
            }
        }

        return false;
    }

    void stop() {
        m_entity_timeout = 0;
        m_expired_callback = nullptr;
        m_time_getter = nullptr;
        m_clock_getter = nullptr;
        m_timers.clear();
        m_items.clear();
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
                if (m_timers.empty()) { // BacklogWithTimeout object could be stopped in callback
                    return;
                }

                continue;
            }

            auto index = bucket_index_from_time(m_entity_timeout - time_diff);
            if (index != timer_index) {
                m_items[index].push_back(current_bucket[i]);
            } else {
                bucket_replacement.push_back(current_bucket[i]);
            }
        }

        m_items[timer_index].swap(bucket_replacement);
    }

    std::size_t bucket_index_from_time(std::uint64_t time) const {
        assert(!m_timers.empty());

        // Linear search on tiny sets is much more faster than tricks with logarithms or unordered map
        for(std::size_t i = 0; i < m_timers.size(); ++i) {
            if (time >= m_timers[i]->timeout_ms() * 1000000) {
                return i;
            }
        }

        return m_timers.size() - 1;
    }

private:
    std::size_t m_entity_timeout = 0;
    OnItemExpiredCallback m_expired_callback;
    ItemTimeGetter m_time_getter = nullptr;
    MonothonicClockGetterType m_clock_getter = nullptr;
    std::vector<std::unique_ptr<TimerType, typename TimerType::DefaultDelete>> m_timers;
    std::vector<std::vector<T>> m_items;
};

} // namespace io
