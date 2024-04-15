#ifndef TIMER_HPP_EFE0EEFD_E5DE_4F21_B06A_78D5C5CD57A4
#define TIMER_HPP_EFE0EEFD_E5DE_4F21_B06A_78D5C5CD57A4

#include <chrono>

namespace util {

template<typename Duration = std::chrono::duration<double, std::ratio<1>>, typename Clock = std::chrono::steady_clock>
class Timer {
	typename Clock::time_point m_tic;

public:
	Timer() : m_tic(Clock::now()) {}

	void tic() {
		m_tic = Clock::now();
	}
	auto toc() const {
		return std::chrono::duration_cast<Duration>(Clock::now() - m_tic).count();
	}
	static auto now() {
		return std::chrono::duration_cast<Duration>(Clock::now().time_since_epoch()).count();
	}

	using DurationType = Duration;
	using ClockType = Clock;
};

} // namespace util

#endif /* TIMER_HPP_EFE0EEFD_E5DE_4F21_B06A_78D5C5CD57A4 */
