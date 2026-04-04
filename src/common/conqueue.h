#ifndef CONQUEUE_H_B016712B_58E3_4A87_8721_F61EDCBFA0E3
#define CONQUEUE_H_B016712B_58E3_4A87_8721_F61EDCBFA0E3

#include <deque>
#include <mutex>
#include <condition_variable>
#include <optional>

template<class T>
class ConQueue : protected std::deque<T> {
public:
    using Container = std::deque<T>;
    using typename Container::value_type;
    using typename Container::size_type;
    using typename Container::reference;
    using typename Container::const_reference;
private:
    std::mutex mtx_;
    std::condition_variable cond_emplace_;
    std::condition_variable cond_pop_;
    const size_type max_ = 0;
    bool closed_ = false;
public:
    ConQueue(size_type max_items) : max_(max_items) {};

    // copy / move...

    // methods that return references are disallowed!

    bool empty()
    {
        std::scoped_lock lk(mtx_);
        return Container::empty();
    }
    size_type size()
    {
        std::scoped_lock lk(mtx_);
        return Container::size();
    }

    template<typename ...Ts>
    bool emplace(Ts&& ...Args)
    {
        {
            std::unique_lock lk(mtx_);
            if (max_) {
                cond_emplace_.wait(lk, [&] { return Container::size() < max_ || closed_; });
            }
            if (closed_) return false;
            Container::emplace_back(std::forward<Ts>(Args)...);
        }
        cond_pop_.notify_one();

        return true;
    }

    void close()
    {
        {
            std::scoped_lock lk(mtx_);
            closed_ = true;
        }
        cond_pop_.notify_all();
        if (max_) cond_emplace_.notify_all();
    }

    bool pop(value_type& dst) // pop to destination
    {
        {
            std::unique_lock lk(mtx_);
            cond_pop_.wait(lk, [&] { return closed_ || !Container::empty(); });
            if (Container::empty()) { // continue if closed but not empty
                return false;
            }
            dst = std::move(Container::front());
            Container::pop_front();
        }
        if (max_) {
            cond_emplace_.notify_one();
        }

        return true;
    }

    std::optional<value_type> pop()
    {
        std::optional<value_type> ret;
        {
            std::unique_lock lk(mtx_);
            cond_pop_.wait(lk, [&] { return closed_ || !Container::empty(); });
            if (Container::empty()) {
                return ret;
            }
            ret = std::move(Container::front());
            Container::pop_front();
        }
        if (max_) {
            cond_emplace_.notify_one();
        }

        return ret;
    }

    void clear()
    {
        {
            std::scoped_lock lk(mtx_);
            Container::clear();
        }
        if (max_) cond_emplace_.notify_all();
    }
};

#endif /* CONQUEUE_H_B016712B_58E3_4A87_8721_F61EDCBFA0E3 */
