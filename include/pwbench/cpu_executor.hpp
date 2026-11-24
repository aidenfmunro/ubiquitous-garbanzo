#ifndef PWBENCH_CPU_EXECUTOR_HPP
#define PWBENCH_CPU_EXECUTOR_HPP

#include "pwbench/contracts.hpp"
#include "pwbench/detail/cpu_run_state.hpp"

#include <mutex>
#include <numeric>
#include <stdexcept>
#include <thread>
#include <vector>

namespace pwbench {

struct CpuOptions {
    std::size_t threads{1};
    std::size_t poll_interval{256};
};

namespace detail {

template <CpuAlgorithm Algorithm, HostStrategy Strategy>
class CpuRun {
public:
    CpuRun(const Algorithm& algorithm, const typename Algorithm::target_type& target,
           Strategy& strategy, const RunLimits& limits, const CpuOptions& options)
        : algorithm_(algorithm), target_(target), strategy_(strategy), options_(options),
          state_(limits) {}

    RunResult execute() {
        if (!state_.validate(options_.threads, options_.poll_interval)) {
            return state_.finish(0);
        }
        try {
            counts_.resize(options_.threads);
            start_workers();
            run_worker(0);
        } catch (...) {
            state_.record_failure(std::current_exception());
        }
        join_workers();
        return state_.finish(std::accumulate(counts_.begin(), counts_.end(), std::uint64_t{0}));
    }

private:
    using Work = typename Strategy::work_type;

    void start_workers() {
        workers_.reserve(options_.threads - 1);
        for (std::size_t index = 1; index < options_.threads; ++index) {
            workers_.emplace_back([this, index] { run_worker(index); });
        }
    }

    void join_workers() {
        workers_.clear();
    }

    void run_worker(std::size_t index) {
        std::uint64_t checks = 0;
        try {
            while (!state_.should_stop()) {
                auto work = next_work();
                if (!work) {
                    break;
                }
                process_work(*work, checks);
            }
        } catch (...) {
            state_.record_failure(std::current_exception());
        }
        counts_[index] = checks;
    }

    std::optional<Work> next_work() {
        std::lock_guard lock(source_mutex_);
        if (source_exhausted_ || state_.should_stop()) {
            return std::nullopt;
        }
        auto work = strategy_.next_work();
        source_exhausted_ = !work.has_value();
        return work;
    }

    void process_work(Work& work, std::uint64_t& checks) {
        std::size_t until_poll = 0;
        while (true) {
            if (until_poll == 0) {
                if (state_.should_stop()) {
                    return;
                }
                until_poll = options_.poll_interval;
            }
            const auto candidate = work.next_candidate();
            if (!candidate || !check_candidate(*candidate, checks)) {
                return;
            }
            --until_poll;
        }
    }

    bool check_candidate(CandidateView candidate, std::uint64_t& checks) {
        if (candidate.size != 0 && candidate.data == nullptr) {
            throw std::runtime_error("nonempty candidate has null data");
        }
        if (state_.has_check_limit() && !state_.claim_check()) {
            return false;
        }
        const auto digest = algorithm_.hash(candidate, target_);
        const bool matched = algorithm_.matches(digest, target_);
        ++checks;
        if (matched) {
            state_.record_match(candidate);
        }
        return !matched;
    }

    const Algorithm& algorithm_;
    const typename Algorithm::target_type& target_;
    Strategy& strategy_;
    CpuOptions options_;
    CpuRunState state_;
    std::mutex source_mutex_;
    bool source_exhausted_{};
    std::vector<std::uint64_t> counts_;
    std::vector<std::jthread> workers_;
};

}

class CpuExecutor {
public:
    template <CpuAlgorithm Algorithm, HostStrategy Strategy>
    static RunResult run(const Algorithm& algorithm, const typename Algorithm::target_type& target,
                         Strategy& strategy, const RunLimits& limits = {},
                         const CpuOptions& options = {}) {
        detail::CpuRun<Algorithm, Strategy> execution(algorithm, target, strategy, limits, options);
        return execution.execute();
    }
};

}

#endif
