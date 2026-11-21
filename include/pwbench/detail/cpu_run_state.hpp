#ifndef PWBENCH_DETAIL_CPU_RUN_STATE_HPP
#define PWBENCH_DETAIL_CPU_RUN_STATE_HPP

#include "pwbench/types.hpp"

#include <atomic>
#include <exception>
#include <mutex>

namespace pwbench::detail {

class CpuRunState {
public:
    explicit CpuRunState(RunLimits limits);

    bool validate(std::size_t threads, std::size_t poll_interval);
    bool should_stop();
    bool claim_check();
    void record_match(CandidateView candidate);
    void record_failure(std::exception_ptr error);
    RunResult finish(std::uint64_t checks);

    bool has_check_limit() const {
        return limits_.max_checks.has_value();
    }

private:
    using Clock = std::chrono::steady_clock;

    Clock::time_point started_{Clock::now()};
    RunLimits limits_;
    std::atomic_bool stop_{false};
    std::atomic_bool deadline_hit_{false};
    std::atomic_bool check_limit_hit_{false};
    std::atomic<std::uint64_t> claimed_checks_{0};
    std::mutex result_mutex_;
    std::optional<Candidate> found_;
    std::exception_ptr failure_;
};

}

#endif
