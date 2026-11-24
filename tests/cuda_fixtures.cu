#include "cuda_fixtures.hpp"

#include <cuda_runtime.h>

#include <algorithm>
#include <thread>

namespace fixtures {
namespace {

__global__ void sum_candidates(pwbench::CudaDeviceBatch batch, unsigned* sums) {
    const auto index = blockIdx.x;
    const auto begin = batch.offsets[index];
    const auto size = batch.offsets[index + 1] - begin;
    unsigned sum{};
    for (std::uint64_t offset = threadIdx.x; offset < size; offset += blockDim.x) {
        sum += batch.bytes[begin + offset];
    }
    __shared__ unsigned partials[32];
    partials[threadIdx.x] = sum;
    __syncthreads();
    if (threadIdx.x == 0) {
        unsigned total{};
        for (unsigned lane = 0; lane < 32; ++lane) {
            total += partials[lane];
        }
        sums[index] = total;
    }
}

__global__ void compare_sums(const unsigned* sums, std::size_t count, unsigned target,
                             pwbench::CudaMatch* match, bool invalid_match) {
    if (invalid_match) {
        match->index = count;
        return;
    }
    for (std::size_t index = 0; index < count; ++index) {
        if (sums[index] == target) {
            match->index = index;
            return;
        }
    }
}

}

CudaSum::Session::Session(unsigned target, std::size_t capacity, unsigned fail_on_launch,
                          bool invalid_match, std::chrono::milliseconds completion_delay)
    : target_(target), capacity_(capacity), fail_on_launch_(fail_on_launch),
      invalid_match_(invalid_match), completion_delay_(completion_delay), sums_(capacity) {}

void CudaSum::Session::launch(pwbench::CudaDeviceBatch batch, pwbench::CudaMatch* match,
                              cudaStream_t stream) {
    sum_candidates<<<static_cast<unsigned>(batch.count), 32, 0, stream>>>(batch, sums_.data());
    pwbench::check_cuda(cudaGetLastError(), "test checksum kernel launch");
    compare_sums<<<1, 1, 0, stream>>>(sums_.data(), batch.count, target_, match, invalid_match_);
    pwbench::check_cuda(cudaGetLastError(), "test comparison kernel launch");
    if (++launches_ == fail_on_launch_) {
        throw std::runtime_error("test CUDA launch failure");
    }
    std::this_thread::sleep_for(completion_delay_);
}

CudaSum::Session CudaSum::create_session(const target_type& target,
                                         const pwbench::CudaContext& context,
                                         std::size_t requested) const {
    const auto capacity_limit =
        std::min(capacity, static_cast<std::size_t>(context.properties().maxGridSize[0]));
    return Session(*target.value, std::min(capacity_limit, requested), fail_on_launch,
                   invalid_match, completion_delay);
}

}
