#pragma once

#include <coroutine>
#include <exception>
#include <utility>

namespace mrpc {

template <typename T = void>
class Task {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        T value_;
        std::exception_ptr exception_;
        std::coroutine_handle<> continuation_;

        Task get_return_object() {
            return Task(handle_type::from_promise(*this));
        }

        std::suspend_always initial_suspend() noexcept { return {}; }

        struct FinalAwaiter {
            bool await_ready() const noexcept { return false; }
            std::coroutine_handle<> await_suspend(handle_type h) noexcept {
                if (auto continuation = h.promise().continuation_) {
                    return continuation;
                }
                return std::noop_coroutine();
            }
            void await_resume() noexcept {}
        };

        FinalAwaiter final_suspend() noexcept { return {}; }

        template <typename U>
        void return_value(U&& value) {
            value_ = std::forward<U>(value);
        }

        void unhandled_exception() {
            exception_ = std::current_exception();
        }
    };

    Task(handle_type h) : handle_(h) {}
    Task(Task&& t) noexcept : handle_(t.handle_) { t.handle_ = nullptr; }
    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }
    
    Task& operator=(Task&& t) noexcept {
        if (this != &t) {
            if (handle_) handle_.destroy();
            handle_ = t.handle_;
            t.handle_ = nullptr;
        }
        return *this;
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    bool await_ready() const noexcept { 
        return !handle_ || handle_.done(); 
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept {
        handle_.promise().continuation_ = awaiting_coroutine;
        return handle_;
    }

    T await_resume() {
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
        return std::move(handle_.promise().value_);
    }

    void resume() {
        if (handle_ && !handle_.done()) {
            handle_.resume();
        }
    }

    handle_type handle_;
};

// Void specialization
template <>
class Task<void> {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        std::exception_ptr exception_;
        std::coroutine_handle<> continuation_;

        Task get_return_object() {
            return Task(handle_type::from_promise(*this));
        }

        std::suspend_always initial_suspend() noexcept { return {}; }

        struct FinalAwaiter {
            bool await_ready() const noexcept { return false; }
            std::coroutine_handle<> await_suspend(handle_type h) noexcept {
                if (auto continuation = h.promise().continuation_) {
                    return continuation;
                }
                return std::noop_coroutine();
            }
            void await_resume() noexcept {}
        };

        FinalAwaiter final_suspend() noexcept { return {}; }

        void return_void() {}

        void unhandled_exception() {
            exception_ = std::current_exception();
        }
    };

    Task(handle_type h) : handle_(h) {}
    Task(Task&& t) noexcept : handle_(t.handle_) { t.handle_ = nullptr; }
    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }

    Task& operator=(Task&& t) noexcept {
        if (this != &t) {
            if (handle_) handle_.destroy();
            handle_ = t.handle_;
            t.handle_ = nullptr;
        }
        return *this;
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    bool await_ready() const noexcept { 
        return !handle_ || handle_.done(); 
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept {
        handle_.promise().continuation_ = awaiting_coroutine;
        return handle_;
    }

    void await_resume() {
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
    }

    void resume() {
        if (handle_ && !handle_.done()) {
            handle_.resume();
        }
    }

    handle_type handle_;
};

} // namespace mrpc