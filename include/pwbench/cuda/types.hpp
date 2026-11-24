#ifndef PWBENCH_CUDA_TYPES_HPP
#define PWBENCH_CUDA_TYPES_HPP

#include "pwbench/types.hpp"

namespace pwbench {

struct CudaDeviceBatch {
    const Byte* bytes;
    const std::uint64_t* offsets;
    std::size_t count;
};

struct CudaMatch {
    static constexpr unsigned long long none = ~0ULL;
    unsigned long long index{none};
};

}

#endif
