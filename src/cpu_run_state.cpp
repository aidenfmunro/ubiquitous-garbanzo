#include "pwbench/detail/cpu_run_state.hpp"

#include <stdexcept>
#include <utility>

namespace pwbench::detail {
namespace {

std::string exception_message(std::exception_ptr error) {
    try {
        std::rethrow_exception(error);
    } catch (const std::exception& exception) {
        return exception.what();
    } catch (...) {
        return "unknown execution error";
    }
}

}

CpuRunState::CpuRunState(RunLimits limits) : limits_(std::move(limits)) {}

bool CpuRunState::validate(std::size_t threads, std::size_t poll_interval) {
    if (threads == 0 || poll_interval == 0) {
        record_failure(std::make_exception_ptr(
            std::invalid_argument("threads and poll_interval must be positive")));
        return false;
    }
    if (limits_.time_limit && *limits_.time_limit < std::chrono::nanoseconds::zero()) {
        record_failure(
            std::make_exception_ptr(std::invalid_argument("time_limit must not be negative")));
        return false;
    }
    if (limits_.max_checks && *limits_.max_checks == 0) {
        check_limit_hit_.store(true, std::memory_order_relaxed);
        stop_.store(true, std::memory_order_relaxed);
        return false;
    }
    return true;
}

bool CpuRunState::should_stop() {
    if (stop_.load(std::memory_order_relaxed)) {
        return true;
    }
    if (limits_.time_limit && Clock::now() - started_ >= *limits_.time_limit) {
        deadline_hit_.store(true, std::memory_order_relaxed);
        stop_.store(true, std::memory_order_relaxed);
        return true;
    }
    return false;
}

bool CpuRunState::claim_check() {
    if (!limits_.max_checks) {
        return true;
    }
    auto claimed = claimed_checks_.load(std::memory_order_relaxed);
    while (claimed < *limits_.max_checks) {
        if (claimed_checks_.compare_exchange_weak(claimed, claimed + 1,
                                                  std::memory_order_relaxed)) {
            return true;
        }
    }
    check_limit_hit_.store(true, std::memory_order_relaxed);
    stop_.store(true, std::memory_order_relaxed);
    return false;
}

void CpuRunState::record_match(CandidateView candidate) {
    {
        std::lock_guard lock(result_mutex_);
        if (!found_) {
            found_.emplace();
            if (candidate.size != 0) {
                found_->assign(candidate.data, candidate.data + candidate.size);
            }
        }
    }
    stop_.store(true, std::memory_order_relaxed);
}

void CpuRunState::record_failure(std::exception_ptr error) {
    {
        std::lock_guard lock(result_mutex_);
        if (!failure_) {
            failure_ = std::move(error);
        }
    }
    stop_.store(true, std::memory_order_relaxed);
}

RunResult CpuRunState::finish(std::uint64_t checks) {
    RunResult result;
    result.checks = checks;
    if (failure_) {
        result.reason = StopReason::error;
        result.error = exception_message(failure_);
    } else if (found_) {
        result.reason = StopReason::found;
        result.candidate = std::move(found_);
    } else if (deadline_hit_.load(std::memory_order_relaxed)) {
        result.reason = StopReason::time_limit;
    } else if (check_limit_hit_.load(std::memory_order_relaxed)) {
        result.reason = StopReason::check_limit;
    }
    result.elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - started_);
    return result;
}

}
