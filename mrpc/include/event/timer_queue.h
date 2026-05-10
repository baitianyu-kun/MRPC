//
// Created by baitianyu on 2/15/25.
//

#ifndef RPCFRAME_TIMER_QUEUE_H
#define RPCFRAME_TIMER_QUEUE_H

#include <vector>
#include <set>
#include <coroutine>
#include "event/timer.h"
#include "event/fd_event.h"

namespace mrpc {

    class TimerQueue : public FDEvent {
    public:
        using ptr = std::shared_ptr<TimerQueue>;
        using TimerCallback = std::function<void()>;
        using Entry = std::pair<Timestamp, Timer::ptr>; // 以时间戳作为键值获取定时器
        using TimerList = std::set<Entry>;          // 底层使用红黑树管理，自动按照时间戳进行排序
        using ActiveTimer = std::pair<Timer::ptr, int64_t>; // 仅用作删除Timer使用
        using ActiveTimerSet = std::set<ActiveTimer>; // 仅用作删除Timer使用

        TimerQueue();

        ~TimerQueue();

        TimerId addTimer(TimerCallback cb, Timestamp when, double interval);

        void deleteTimer(TimerId timerId);

        // Coroutine support
        struct TimerAwaiter {
            TimerQueue* m_timer_queue;
            Timestamp m_when;
            TimerId m_timer_id;

            bool await_ready() const noexcept { return false; }
            
            void await_suspend(std::coroutine_handle<> handle) noexcept {
                m_timer_id = m_timer_queue->addTimer([handle]() mutable {
                    if (handle && !handle.done()) {
                        handle.resume();
                    }
                }, m_when, 0.0);
            }
            
            void await_resume() noexcept {
                // If it resumed, the timer naturally expired and was handled/removed.
                // Or we could proactively delete it if cancellation was supported, but 
                // for simple sleep it naturally completes.
            }
        };

        TimerAwaiter sleep_until(Timestamp when) {
            return {this, when, TimerId(nullptr, -1)};
        }

    private:

        void handleRead();

        void resetTimerfd(int timerfd, Timestamp expiration);

        std::vector<Entry> getExpired(Timestamp now);

        void reset(const std::vector<Entry> &expired, Timestamp now);

        bool insert(Timer::ptr timer);

        TimerList m_timers;          // 定时器队列（内部实现是红黑树）
        bool m_callingExpiredTimers; // 标明正在获取超时定时器

        ActiveTimerSet m_activeTimers;
        ActiveTimerSet m_cancelingTimers;
    };
}

#endif //RPCFRAME_TIMER_QUEUE_H
