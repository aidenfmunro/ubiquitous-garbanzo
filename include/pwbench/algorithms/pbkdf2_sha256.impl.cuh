#ifndef PWBENCH_ALGORITHMS_PBKDF2_SHA256_CUH
#define PWBENCH_ALGORITHMS_PBKDF2_SHA256_CUH

#include "pwbench/algorithms/pbkdf2_sha256.impl.hpp"
#include "pwbench/cuda/contracts.hpp"

namespace pwbench {

class Pbkdf2Sha256Cuda {
public:
    using target_type = Pbkdf2Sha256::target_type;

    class Session {
    public:
        Session(const target_type& target, const CudaContext& context, std::size_t batch_size);
        std::size_t max_batch_size() const noexcept {
            return batch_size_;
        }
        void launch(CudaDeviceBatch batch, CudaMatch* match, cudaStream_t stream) const;

    private:
        std::uint32_t digest_[8]{};
        std::uint32_t salt_[4]{};
        std::uint32_t iterations_{};
        std::size_t batch_size_{};
        unsigned threads_{};
        unsigned max_grid_{};
    };

    using session_type = Session;
    Session create_session(const target_type& target, const CudaContext& context,
                           std::size_t batch_size) const;
};

static_assert(CudaAlgorithm<Pbkdf2Sha256Cuda>);

}

#endif
