#ifndef PWBENCH_CUDA_CONTRACTS_HPP
#define PWBENCH_CUDA_CONTRACTS_HPP

#include "pwbench/cuda/runtime.hpp"

#include <concepts>

namespace pwbench {

template <class T>
concept CudaSession =
    std::move_constructible<T> && requires(T& session, const T& immutable, CudaDeviceBatch batch,
                                           CudaMatch* match, cudaStream_t stream) {
        { immutable.max_batch_size() } -> std::same_as<std::size_t>;
        { session.launch(batch, match, stream) } -> std::same_as<void>;
    };

template <class T>
concept CudaAlgorithm = requires(const T& algorithm, const typename T::target_type& target,
                                 const CudaContext& context, std::size_t batch_size) {
    requires CudaSession<typename T::session_type>;
    {
        algorithm.create_session(target, context, batch_size)
    } -> std::same_as<typename T::session_type>;
};

}

#endif
