#ifndef PWBENCH_DETAIL_CUDA_SOURCE_HPP
#define PWBENCH_DETAIL_CUDA_SOURCE_HPP

#include "pwbench/contracts.hpp"

#include <stdexcept>

namespace pwbench::detail {

template <HostStrategy Strategy>
class CudaSource {
public:
    explicit CudaSource(Strategy& strategy) : strategy_(strategy) {}

    const std::optional<CandidateView>& peek() {
        while (!candidate_ && !exhausted_) {
            if (!work_) {
                work_ = strategy_.next_work();
                if (!work_) {
                    exhausted_ = true;
                    break;
                }
            }
            candidate_ = work_->next_candidate();
            if (!candidate_) {
                work_.reset();
            } else if (candidate_->size != 0 && candidate_->data == nullptr) {
                throw std::runtime_error("nonempty candidate has null data");
            }
        }
        return candidate_;
    }

    void consume() noexcept {
        candidate_.reset();
    }

private:
    Strategy& strategy_;
    std::optional<typename Strategy::work_type> work_;
    std::optional<CandidateView> candidate_;
    bool exhausted_{};
};

}

#endif
