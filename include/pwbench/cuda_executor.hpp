#ifndef PWBENCH_CUDA_EXECUTOR_HPP
#define PWBENCH_CUDA_EXECUTOR_HPP

#include "pwbench/cuda/batch.hpp"
#include "pwbench/cuda/contracts.hpp"
#include "pwbench/detail/cuda_source.hpp"

#include <algorithm>
#include <limits>

namespace pwbench {

struct CudaOptions {
    int device{};
    std::size_t batch_size{65536};
    std::size_t batch_bytes{8 * 1024 * 1024};
};

namespace detail {

template <CudaAlgorithm Algorithm, HostStrategy Strategy>
class CudaRun {
public:
    CudaRun(const Algorithm& algorithm, const typename Algorithm::target_type& target,
            Strategy& strategy, RunLimits limits, CudaOptions options)
        : algorithm_(algorithm), target_(target), source_(strategy), limits_(std::move(limits)),
          options_(options) {}

    RunResult execute() {
        try {
            validate();
            if (limits_.max_checks && *limits_.max_checks == 0) {
                result_.reason = StopReason::check_limit;
            } else if (!deadline_reached()) {
                run_on_device();
            }
        } catch (const std::exception& error) {
            result_.reason = StopReason::error;
            result_.candidate.reset();
            result_.error = error.what();
        } catch (...) {
            result_.reason = StopReason::error;
            result_.candidate.reset();
            result_.error = "unknown CUDA execution error";
        }
        result_.elapsed =
            std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - started_);
        return std::move(result_);
    }

private:
    using Clock = std::chrono::steady_clock;

    void validate() const {
        if (options_.device < 0 || options_.batch_size == 0 || options_.batch_bytes == 0) {
            throw std::invalid_argument(
                "CUDA device must be nonnegative and batch sizes must be positive");
        }
        if (limits_.time_limit && *limits_.time_limit < std::chrono::nanoseconds::zero()) {
            throw std::invalid_argument("time_limit must not be negative");
        }
    }

    bool deadline_reached() {
        if (limits_.time_limit && Clock::now() - started_ >= *limits_.time_limit) {
            result_.reason = StopReason::time_limit;
            return true;
        }
        return false;
    }

    void run_on_device() {
        CudaWorkspace workspace(options_.device);
        auto session = algorithm_.create_session(target_, workspace.context(), options_.batch_size);
        try {
            const auto capacity = std::min(options_.batch_size, session.max_batch_size());
            if (capacity == 0) {
                throw std::invalid_argument("CUDA algorithm returned zero batch capacity");
            }
            while (fill_batch(capacity)) {
                const auto device_batch = workspace.upload(batch_);
                if (deadline_reached()) {
                    break;
                }
                session.launch(device_batch, workspace.match(), workspace.context().stream());
                check_cuda(cudaGetLastError(), "CUDA batch launch");
                const auto match = workspace.complete();
                if (record_batch(match)) {
                    break;
                }
            }
            workspace.context().synchronize();
        } catch (...) {
            workspace.context().drain();
            throw;
        }
    }

    bool fill_batch(std::size_t capacity) {
        batch_.clear();
        if (deadline_reached()) {
            return false;
        }
        if (!source_.peek()) {
            return false;
        }
        if (limits_.max_checks) {
            const auto remaining = *limits_.max_checks - result_.checks;
            if (remaining == 0) {
                result_.reason = StopReason::check_limit;
                return false;
            }
            if (remaining < capacity) {
                capacity = static_cast<std::size_t>(remaining);
            }
        }
        while (batch_.count() < capacity) {
            if (deadline_reached()) {
                return false;
            }
            const auto& candidate = source_.peek();
            if (!candidate) {
                break;
            }
            const auto bytes = batch_.bytes().size();
            if (batch_.count() != 0 &&
                (bytes >= options_.batch_bytes || candidate->size > options_.batch_bytes - bytes)) {
                break;
            }
            batch_.append(*candidate);
            source_.consume();
        }
        return !deadline_reached() && batch_.count() != 0;
    }

    bool record_batch(unsigned long long match) {
        if (batch_.count() > std::numeric_limits<std::uint64_t>::max() - result_.checks) {
            throw std::overflow_error("CUDA check count overflow");
        }
        result_.checks += batch_.count();
        if (match == CudaMatch::none) {
            return false;
        }
        if (match >= batch_.count()) {
            throw std::out_of_range("CUDA algorithm returned an invalid candidate index");
        }
        const auto candidate = batch_.candidate(static_cast<std::size_t>(match));
        result_.candidate.emplace();
        if (candidate.size != 0) {
            result_.candidate->assign(candidate.data, candidate.data + candidate.size);
        }
        result_.reason = StopReason::found;
        return true;
    }

    Clock::time_point started_{Clock::now()};
    const Algorithm& algorithm_;
    const typename Algorithm::target_type& target_;
    CudaSource<Strategy> source_;
    RunLimits limits_;
    CudaOptions options_;
    CudaBatch batch_;
    RunResult result_;
};

}

class CudaExecutor {
public:
    template <CudaAlgorithm Algorithm, HostStrategy Strategy>
    static RunResult run(const Algorithm& algorithm, const typename Algorithm::target_type& target,
                         Strategy& strategy, const RunLimits& limits = {},
                         const CudaOptions& options = {}) {
        detail::CudaRun<Algorithm, Strategy> execution(algorithm, target, strategy, limits,
                                                       options);
        return execution.execute();
    }
};

}

#endif
