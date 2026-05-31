#ifndef THREAD_POOL_HPP_FE77F15C_8C2D_411A_8D40_2AA77362894A
#define THREAD_POOL_HPP_FE77F15C_8C2D_411A_8D40_2AA77362894A

#include "ThreadPoolFwd.hpp"
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <type_traits>
#include <boost/circular_buffer.hpp>
#include <concepts>

#if HAVE_MOVE_ONLY_FUNCTION
#include <functional>
#else
#include <boost/compat/move_only_function.hpp>
#endif


namespace detail {

// For specializing based on whether the task queue is bounded or not.
template<bool Bounded>
struct ThreadPoolBase {
};

template<>
struct ThreadPoolBase<true> {
    std::size_t max_jobs; // Runtime upper limit of stored tasks.
    std::condition_variable cond_enqueue{}; // Signals that enqueuing threads should continue.
};

} // namespace detail


template<bool Bounded>
class ThreadPool : detail::ThreadPoolBase<Bounded> {
public:

    /**
     * Constructs a bounded thread pool, with an upper limit on task queue size.
     */
    ThreadPool(std::size_t nb_threads, std::size_t max_jobs)
    noexcept requires (Bounded) : detail::ThreadPoolBase<Bounded>{max_jobs}, tasks(max_jobs)
    {
        start_workers(nb_threads);
    }

    /**
     * Constructs an unbounded thread pool, with no upper limit on task queue size.
     */
    ThreadPool(std::size_t nb_threads)
    noexcept requires (!Bounded)
    {
        start_workers(nb_threads);
    }

    /**
     * Initialize workers with a custom callable.
     * `init_fn` must be copyable.
     */
    template<typename F>
    requires std::invocable<F> && std::copy_constructible<F>
    ThreadPool(const F& init_fn, std::size_t nb_threads, std::size_t max_jobs)
    noexcept requires(Bounded) : detail::ThreadPoolBase<Bounded>{max_jobs}, tasks(max_jobs)
    {
        start_workers(init_fn, nb_threads);
    }

    template<typename F>
    requires std::invocable<F> && std::copy_constructible<F>
    ThreadPool(const F& init_fn, std::size_t nb_threads)
    noexcept requires(!Bounded)
    {
        start_workers(init_fn, nb_threads);
    }

    // Not copiable.
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Not movable.
    // If you really need to move me, wrap me inside unique_ptr.

    /**
     * Destructor.
     * Wait for all tasks to finish, then join the threads.
     */
    ~ThreadPool() noexcept
    {
        stop();
        for (auto& worker : workers) {
            worker.join();
        }
    }

    /**
     * Request to stop the pool.
     * Remaining tasks will continue on. Enqueuing is blocked.
     */
    void stop() noexcept
    {
        {
            std::scoped_lock lk(mutex_);
            stop_ = true;
        }
        cond_work.notify_all();
        if constexpr (Bounded) {
            this->cond_enqueue.notify_all();
        }
    }

    /**
     * Request to stop the pool immediately. Discards tasks in the queue.
     */
    void stop_now() noexcept
    {
        {
            std::scoped_lock lk(mutex_);
            stop_ = true;
            tasks.clear();
            if (nb_working == 0) {
                cond_all_done.notify_all();
            }
        }
        cond_work.notify_all();
        if constexpr (Bounded) {
            this->cond_enqueue.notify_all();
        }
    }

    /**
     * Wait for all tasks to finish.
     */
    void wait_all() noexcept
    {
        std::unique_lock lk(mutex_);
        cond_all_done.wait(lk, [&] { return nb_working == 0 && tasks.empty(); });
    }

    /**
     * Enqueue a task
     *
     * @return Returns `false` if pool is stopped, so the task cannot be enqueued.
     */
    template<typename F>
    bool enqueue(F&& f) noexcept
    {
        TaskT task(std::forward<F>(f));
        {
            using LockT = std::conditional_t<Bounded, std::unique_lock<std::mutex>, std::lock_guard<std::mutex>>;
            LockT lk(mutex_);
            if constexpr (Bounded) {
                this->cond_enqueue.wait(lk, [&] { return (tasks.size() < this->max_jobs) || stop_; });
            }
            if (stop_) {
                return false;
            }
            tasks.push_back(std::move(task));
        }
        cond_work.notify_one();
        return true;
    }

    static constexpr auto is_bounded = Bounded;

private:
    std::vector<std::thread> workers;

    std::mutex mutex_;
    std::condition_variable cond_work; // Signals worker threads to continue.
    bool stop_ = false; // Whether workers shall stop.

    std::size_t nb_working = 0; // Number of worker threads that are working on the task.
    std::condition_variable cond_all_done; // Signals that task queue is empty and no worker thread is working.

    // move_only_function is used for type-erased task storage.
    // It has lower overhead, and allows non-copyable tasks.
#if HAVE_MOVE_ONLY_FUNCTION
    using TaskT = std::move_only_function<void() &>;
#else
    using TaskT = boost::compat::move_only_function<void() &>;
#endif
    using QueueT = std::conditional_t<Bounded, boost::circular_buffer<TaskT>, std::deque<TaskT>>;
    QueueT tasks;

    void worker_loop()
    {
        for (;;) {
            TaskT task;

            {
                std::unique_lock lk(mutex_);
                cond_work.wait(lk, [&] { return stop_ || !tasks.empty(); });
                if (tasks.empty())
                    return;
                task = std::move(tasks.front());
                tasks.pop_front();
                nb_working += 1;
            }
            if constexpr (Bounded) {
                this->cond_enqueue.notify_one();
            }

            task();

            {
                std::scoped_lock lk(mutex_);
                if (--nb_working == 0) {
                    cond_all_done.notify_all();
                }
            }
        }
    }

    void start_workers(std::size_t nb_threads) noexcept
    {
        for (std::size_t i = 0; i < nb_threads; ++i) {
            workers.emplace_back([this] { worker_loop(); });
        }
    }

    template<typename F>
    void start_workers(const F& init_fn, std::size_t nb_threads) noexcept
    {
        for (std::size_t i = 0; i < nb_threads; ++i) {
            workers.emplace_back([this, init_fn] { // init_fn must be copyable
                init_fn();
                worker_loop();
            });
        }
    }
};

// CTAD guides
ThreadPool(std::size_t, std::size_t) -> ThreadPool<true>;
ThreadPool(std::size_t) -> ThreadPool<false>;
template<std::invocable F>
ThreadPool(const F&, std::size_t, std::size_t) -> ThreadPool<true>;
template<std::invocable F>
ThreadPool(const F&, std::size_t) -> ThreadPool<false>;

#endif /* THREAD_POOL_HPP_FE77F15C_8C2D_411A_8D40_2AA77362894A */
