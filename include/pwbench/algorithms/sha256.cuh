#ifndef PWBENCH_ALGORITHMS_SHA256_CUH
#define PWBENCH_ALGORITHMS_SHA256_CUH

#include "pwbench/algorithms/sha256.hpp"
#include "pwbench/cuda/contracts.hpp"

namespace pwbench {

class Sha256Cuda {
public:
    using target_type = Sha256::target_type;

    class Session {
    public:
        Session(const target_type& target, const CudaContext& context, std::size_t batch_size);
        std::size_t max_batch_size() const noexcept {
            return batch_size_;
        }
        void launch(CudaDeviceBatch batch, CudaMatch* match, cudaStream_t stream) const;

    private:
        std::uint32_t digest_[8]{};
        std::size_t batch_size_;
        unsigned threads_;
        unsigned max_grid_;
    };

    using session_type = Session;
    Session create_session(const target_type& target, const CudaContext& context,
                           std::size_t batch_size) const;
};

static_assert(CudaAlgorithm<Sha256Cuda>);

}

#endif
