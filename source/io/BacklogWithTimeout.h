/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "CommonMacros.h"
#include "Timer.h"
#include "EventLoop.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

#include <assert.h>

namespace tarm {
namespace io {

template<typename T, typename LoopType = ::tarm::io::EventLoop, typename TimerType = ::tarm::io::Timer>
class BacklogWithTimeout {
public:
    using OnItemExpiredCallback = std::function<void(BacklogWithTimeout<T, LoopType, TimerType>&, const T& item)>;
    using ItemTimeGetter = std::function<std::uint64_t(const T&)>;
    using MonothonicClockGetterType = std::uint64_t(*)();

    TARM_IO_FORBID_COPY(BacklogWithTimeout);

    BacklogWithTimeout(BacklogWithTimeout&& other) :
        m_entity_timeout(other.m_entity_timeout),
        m_expired_callback(std::move(other.m_expired_callback)),
        m_time_getter(std::move(other.m_time_getter)),
        m_clock_getter(std::move(other.m_clock_getter)),
        m_timers(std::move(other.m_timers)),
        m_items(std::move(other.m_items)) {

        refresh_timer_backlog_pointers();
    }

    BacklogWithTimeout& operator=(BacklogWithTimeout&& other) {
        m_entity_timeout = other.m_entity_timeout;
        m_expired_callback = std::move(other.m_expired_callback);
        m_time_getter = std::move(other.m_time_getter);
        m_clock_getter = std::move(other.m_clock_getter);
        m_timers = std::move(other.m_timers);
        m_items = std::move(other.m_items);

        refresh_timer_backlog_pointers();
        return *this;
    }

    void refresh_timer_backlog_pointers() {
        for (auto& t : m_timers) {
            auto& data = t->template user_data_as_ref<TimerData>();
            data.backlog = this;
        }
    }

    struct TimerData {
        using BacklogType = BacklogWithTimeout<T, LoopType, TimerType>;

        TimerData(std::size_t i, BacklogType* b) :
            index(i),
            backlog(b) {
        }

        std::size_t index = 0;
        BacklogType* backlog = nullptr;
    };

    BacklogWithTimeout(LoopType& loop, std::size_t entity_timeout, OnItemExpiredCallback expired_callback, ItemTimeGetter time_getter, MonothonicClockGetterType clock_getter) :
        m_entity_timeout(entity_timeout * std::size_t(1000000)),
        m_expired_callback(expired_callback),
        m_time_getter(time_getter),
        m_clock_getter(clock_getter) {

        std::size_t buckets_count = 0;
        std::size_t current_timeout = entity_timeout;
        do {
            auto timer = loop.template allocate<TimerType>();
            // TODO: check return value (timer)
            m_timers.emplace_back(timer, [](TimerType* timer) {
                delete timer->template user_data_as_ptr<TimerData>();
                timer->default_delete()(timer);
            });
            m_timers.back().get()->start(current_timeout, current_timeout, &on_timer);
            m_timers.back().get()->set_user_data(new TimerData{m_timers.size() - 1, this});
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
    static void on_timer(TimerType& timer) {
        auto& data = timer.template user_data_as_ref<TimerData>();
        const std::size_t timer_index = data.index;
        auto& this_ = *data.backlog;

        const std::vector<T>& current_bucket = this_.m_items[timer_index];

        std::vector<T> bucket_replacement;
        bucket_replacement.reserve(current_bucket.size());

        for (std::size_t i = 0; i < current_bucket.size(); ++i) {
            const std::uint64_t current_time = this_.m_clock_getter();
            const std::uint64_t item_time = this_.m_time_getter(current_bucket[i]);

            const auto time_diff = current_time - item_time;
            if (time_diff >= this_.m_entity_timeout) {
                this_.m_expired_callback(this_, current_bucket[i]);
                if (this_.m_timers.empty()) { // BacklogWithTimeout object could be stopped in callback
                    return;
                }

                continue;
            }

            auto index = this_.bucket_index_from_time(this_.m_entity_timeout - time_diff);
            if (index != timer_index) {
                this_.m_items[index].push_back(current_bucket[i]);
            } else {
                bucket_replacement.push_back(current_bucket[i]);
            }
        }

        this_.m_items[timer_index].swap(bucket_replacement);
    }

    std::size_t bucket_index_from_time(std::uint64_t time) const {
        assert(!m_timers.empty());

        if (m_timers.front()->timeout_ms() - (time / 1000000) <= m_timers.back()->timeout_ms()) {
            return 0;
        }

        // Linear search on tiny sets is much more faster than tricks with logarithms or unordered map
        for(std::size_t i = 1; i < m_timers.size(); ++i) {
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
    std::vector<std::unique_ptr<TimerType, std::function<void(TimerType*)> >> m_timers;
    std::vector<std::vector<T>> m_items;
};

} // namespace io
} // namespace tarm
