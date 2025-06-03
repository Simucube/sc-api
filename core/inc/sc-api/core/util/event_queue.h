/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_UTIL_EVENT_QUEUE_H_
#define SC_API_UTIL_EVENT_QUEUE_H_
#include <cassert>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <vector>

namespace sc_api::core::util {

template <typename EventT>
class EventProducer;

/** Thread-safe event queue
 */
template <typename EventT>
class EventQueue {
    friend class EventProducer<EventT>;

public:
    EventQueue() {}
    ~EventQueue();

    /** Try to take first event in the queue, immediately return std::nullopt if queue is empty */
    std::optional<EventT> tryPop() {
        std::lock_guard lock(m_);
        if (queue_.empty()) return std::nullopt;
        std::optional<EventT> e = std::move(queue_.front());
        queue_.pop();
        return e;
    }

    /** Try to take first event in the queue, and block for duration if queue is empty */
    template <class Rep, class Period>
    std::optional<EventT> tryPopFor(std::chrono::duration<Rep, Period> duration) {
        std::unique_lock lock(m_);
        if (queue_.empty()) {
            if (!cv_.wait_for(lock, duration, [&]() { return !queue_.empty() || !open_; })) {
                return std::nullopt;
            }

            if (!open_) return std::nullopt;
        }
        std::optional<EventT> e = std::move(queue_.front());
        queue_.pop();
        return e;
    }

    /** Try to take first event in the queue, and block until time_point if queue is empty */
    template <class Clock, class Duration>
    std::optional<EventT> tryPopUntil(std::chrono::time_point<Clock, Duration> time_point) {
        std::unique_lock lock(m_);
        if (queue_.empty()) {
            bool not_timeout = cv_.wait_until(lock, time_point, [&]() { return !queue_.empty() || !open_; });
            if (!not_timeout || !open_) return std::nullopt;
        }
        std::optional<EventT> e = std::move(queue_.front());
        queue_.pop();
        return e;
    }

    /** Block until event arrives or queue is closed. Returns default constructed EventT if queue is closed */
    EventT pop() {
        std::unique_lock lock(m_);
        if (queue_.empty()) {
            cv_.wait(lock, [&]() { return !queue_.empty() || !open_; });

            if (!open_) return {};
        }
        EventT e = std::move(queue_.front());
        queue_.pop();
        return e;
    }

    /** Closes queue from new events
     *
     * No new events will arrive and pop functions will return std::nullopt when all queued events are
     * popped and will not block.
     *
     * thread-safe
     */
    void close() {
        std::lock_guard lock(m_);
        if (!open_) return;
        open_ = false;
        cv_.notify_all();
    }

private:
    void push(EventT e) {
        std::lock_guard lock(m_);
        if (!open_) return;

        queue_.push(std::move(e));
        cv_.notify_one();
    }

    std::mutex                              m_;
    std::condition_variable                 cv_;
    std::queue<EventT>                      queue_;
    std::weak_ptr<EventProducer<EventT>>    producer_;
    bool                                    open_     = false;
};

template <typename EventT>
class EventProducer : public std::enable_shared_from_this<EventProducer<EventT>> {
    friend class EventQueue<EventT>;

public:
    EventProducer() {}
    ~EventProducer() {
        for (EventQueue<EventT>* q : event_queues_) {
            q->close();
        }
    }

    void notifyEvent(EventT e) {
        std::lock_guard lock(m_);
        for (EventQueue<EventT>* q : event_queues_) {
            q->push(e);
        }
    }

    void pushInitialEvent(EventQueue<EventT>* q, EventT e) { q->queue_.push(std::move(e)); }

    void addEventQueue(EventQueue<EventT>* q) {
        std::lock_guard lock(m_);
        assert(q->open_ == false);
        q->producer_ = this->shared_from_this();
        q->open_     = true;
        event_queues_.push_back(q);
    }

protected:
    void removeEventQueue(EventQueue<EventT>* q) {
        std::lock_guard lock(m_);
        for (auto it = event_queues_.begin(); it != event_queues_.end(); ++it) {
            if (*it == q) {
                event_queues_.erase(it);
                return;
            }
        }
    }

private:
    std::mutex                       m_;
    std::vector<EventQueue<EventT>*> event_queues_;
};

template <typename EventT>
EventQueue<EventT>::~EventQueue() {
    if (auto producer = producer_.lock()) {
        producer->removeEventQueue(this);
    }
}

}  // namespace sc_api::core::util

#endif  // SC_API_UTIL_EVENT_QUEUE_H_
