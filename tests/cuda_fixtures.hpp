#ifndef PWBENCH_TESTS_CUDA_FIXTURES_HPP
#define PWBENCH_TESTS_CUDA_FIXTURES_HPP

#include "pwbench/cuda_module.hpp"

#include <memory>

namespace fixtures {

// Test checksum, not a password hash. One warp per candidate and two kernels.
struct CudaSum {
    struct target_type {
        std::unique_ptr<unsigned> value;
    };

    class Session {
    public:
        Session(unsigned target, std::size_t capacity, unsigned fail_on_launch, bool invalid_match,
                std::chrono::milliseconds completion_delay);
        std::size_t max_batch_size() const noexcept {
            return capacity_;
        }
        void launch(pwbench::CudaDeviceBatch batch, pwbench::CudaMatch* match, cudaStream_t stream);

    private:
        unsigned target_;
        std::size_t capacity_;
        unsigned fail_on_launch_;
        bool invalid_match_;
        std::chrono::milliseconds completion_delay_;
        unsigned launches_{};
        pwbench::CudaBuffer<unsigned> sums_;
    };

    using session_type = Session;
    std::size_t capacity{3};
    unsigned fail_on_launch{};
    bool invalid_match{};
    std::chrono::milliseconds completion_delay{};

    Session create_session(const target_type& target, const pwbench::CudaContext& context,
                           std::size_t requested) const;
};

struct CudaSumModule {
    using algorithm_type = CudaSum;
    static constexpr std::string_view id = "test-cuda-sum";
    static inline unsigned preparations{};

    static std::vector<pwbench::OptionSpec> options() {
        return {};
    }
    static pwbench::PreparedCudaAlgorithm<CudaSum> prepare(pwbench::CandidateView password,
                                                           const pwbench::Parameters&) {
        ++preparations;
        unsigned sum{};
        for (std::size_t index = 0; index < password.size; ++index) {
            sum += password.data[index];
        }
        return {{}, {std::make_unique<unsigned>(sum)}, std::to_string(sum)};
    }
};

static_assert(pwbench::CudaAlgorithm<CudaSum>);
static_assert(!pwbench::CpuAlgorithm<CudaSum>);

}

#endif
