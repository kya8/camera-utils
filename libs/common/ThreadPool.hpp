#ifndef THREAD_POOL_HPP_FE77F15C_8C2D_411A_8D40_2AA77362894A
#define THREAD_POOL_HPP_FE77F15C_8C2D_411A_8D40_2AA77362894A

#include <functional>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "ThreadPoolFwd.hpp"

/**
 * TODO:
 *
 * - Provide a template parameter to select between task storage at compile time.
 *
 * - Use a template parameter to specialize unbounded / bounded task queue.
 *   A ring buffer should be used for the bounded case.
 *   Currently we always use a dequeue, with optional runtime limit on its size.
 */

class ThreadPool {
public:
    ThreadPool(std::size_t nb_threads, std::size_t max_jobs = 0) noexcept;

    /**
     * Initialize workers with a custom callable.
     */
    template<typename F>
    ThreadPool(const F& init_fn, std::size_t nb_threads, std::size_t max_jobs) noexcept;

    ~ThreadPool() noexcept;

    // Not copiable.
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Not movable.
    // If you really need to move me, wrap me inside unique_ptr.

    /**
     * Request to stop the pool.
     *
     * Remaining tasks will continue on. Enqueuing is blocked.
     */
    void stop() noexcept;

    /**
     * Request to stop the pool immediately. Discards tasks in the queue.
     */
    void stop_now() noexcept;

    /**
     * Wait for all tasks to finish.
     */
    void wait_all() noexcept;

    /**
     * Enqueue a task
     * 
     * @return Returns `false` if pool is stopped
     */
    template<typename F>
    bool enqueue(F&& f) noexcept;

private:
    std::vector<std::thread> workers;

    std::mutex mutex_;
    std::condition_variable cond;
    bool stop_ = false;

    std::size_t nb_working = 0;
    std::condition_variable working_cond;

    std::condition_variable cond_enqueue;
    std::size_t max_jobs;

    // move_only_function is used for type-erased task storage.
    // It has lower overhead, and allows non-copyable tasks.
    using TaskT = std::move_only_function<void() &>;
    std::deque<TaskT> tasks;

    void worker_loop()
    {
        for (;;) {
            TaskT task;

            {
                std::unique_lock lk(mutex_);
                cond.wait(lk, [&] { return stop_ || !tasks.empty(); });
                if (tasks.empty())
                    return;
                task = std::move(tasks.front());
                tasks.pop_front();
                nb_working += 1;
            }
            if (max_jobs) {
                cond_enqueue.notify_one();
            }

            task();

            {
                std::scoped_lock lk(mutex_);
                if (--nb_working == 0) {
                    working_cond.notify_all();
                }
            }
        }
    }
};

inline ThreadPool::ThreadPool(std::size_t N, std::size_t max_jobs_) noexcept : max_jobs(max_jobs_)
{
    for (std::size_t i = 0; i < N; ++i) {
        workers.emplace_back( [this] {
            worker_loop();
        });
    }
}

template<typename F>
inline ThreadPool::ThreadPool(const F& init_fn, std::size_t N, std::size_t max_jobs_) noexcept : max_jobs(max_jobs_)
{
    for (std::size_t i = 0; i < N; ++i) {
        workers.emplace_back( [this, init_fn] { // init_fn must be copyable
            init_fn();
            worker_loop();
        });
    }
}

inline void ThreadPool::stop() noexcept
{
    {
        std::scoped_lock lk(mutex_);
        stop_ = true;
    }
    cond.notify_all();
    if (max_jobs) cond_enqueue.notify_all();
}

inline void ThreadPool::stop_now() noexcept
{
    {
        std::scoped_lock lk(mutex_);
        stop_ = true;
        tasks.clear();
        if (nb_working == 0) {
            working_cond.notify_all();
        }
    }
    cond.notify_all();
    if (max_jobs) cond_enqueue.notify_all();
}

inline ThreadPool::~ThreadPool() noexcept
{
    stop();
    for (auto& worker : workers) {
        worker.join();
    }
}

inline void ThreadPool::wait_all() noexcept
{
    std::unique_lock lk(mutex_);
    working_cond.wait(lk, [&] { return nb_working == 0 && tasks.empty(); });
}

template<typename F>
inline bool ThreadPool::enqueue(F&& f) noexcept
{
    TaskT task(std::forward<F>(f));
    {
        std::unique_lock lk(mutex_);
        if (max_jobs) {
            cond_enqueue.wait(lk, [&] { return (tasks.size() < max_jobs) || stop_; });
        }
        if (stop_) {
            return false;
        }
        tasks.emplace_back(std::move(task));
    }
    cond.notify_one();
    return true;
}

#endif /* THREAD_POOL_HPP_FE77F15C_8C2D_411A_8D40_2AA77362894A */
