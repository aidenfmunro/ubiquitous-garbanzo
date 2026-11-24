#include "pwbench/algorithms/md5.cuh"

#include <cuda_runtime.h>

#include <algorithm>

namespace pwbench {
namespace {

struct DeviceDigest {
    std::uint32_t words[4];
};

__device__ std::uint32_t read_word(const Byte* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

__device__ void compress(DeviceDigest& state, const std::uint32_t* words) {
    constexpr std::uint32_t constants[64]{
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613,
        0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193,
        0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d,
        0x02441453, 0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
        0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122,
        0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
        0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665, 0xf4292244,
        0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb,
        0xeb86d391,
    };
    constexpr unsigned rotations[64]{
        7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 5,  9,  14, 20, 5,  9,
        14, 20, 5,  9,  14, 20, 5,  9,  14, 20, 4,  11, 16, 23, 4,  11, 16, 23, 4,  11, 16, 23,
        4,  11, 16, 23, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21,
    };
    auto a = state.words[0];
    auto b = state.words[1];
    auto c = state.words[2];
    auto d = state.words[3];
#pragma unroll
    for (unsigned round = 0; round < 64; ++round) {
        std::uint32_t function;
        unsigned word;
        if (round < 16) {
            function = (b & c) | (~b & d);
            word = round;
        } else if (round < 32) {
            function = (d & b) | (~d & c);
            word = (5 * round + 1) % 16;
        } else if (round < 48) {
            function = b ^ c ^ d;
            word = (3 * round + 5) % 16;
        } else {
            function = c ^ (b | ~d);
            word = (7 * round) % 16;
        }
        const auto value = a + function + constants[round] + words[word];
        const auto next = b + __funnelshift_l(value, value, rotations[round]);
        a = d;
        d = c;
        c = b;
        b = next;
    }
    state.words[0] += a;
    state.words[1] += b;
    state.words[2] += c;
    state.words[3] += d;
}

__device__ DeviceDigest calculate(const Byte* bytes, std::uint64_t size) {
    DeviceDigest state{{0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476}};
    std::uint32_t words[16];
    std::uint64_t offset = 0;
    while (size - offset >= 64) {
#pragma unroll
        for (unsigned index = 0; index < 16; ++index) {
            words[index] = read_word(bytes + offset + 4 * index);
        }
        compress(state, words);
        offset += 64;
    }
#pragma unroll
    for (unsigned index = 0; index < 16; ++index) {
        words[index] = 0;
    }
    const auto remainder = static_cast<unsigned>(size - offset);
    for (unsigned index = 0; index < remainder; ++index) {
        words[index / 4] |= static_cast<std::uint32_t>(bytes[offset + index]) << (8 * (index % 4));
    }
    words[remainder / 4] |= std::uint32_t{0x80} << (8 * (remainder % 4));
    if (remainder >= 56) {
        compress(state, words);
#pragma unroll
        for (unsigned index = 0; index < 16; ++index) {
            words[index] = 0;
        }
    }
    const auto bit_length = size * 8;
    words[14] = static_cast<std::uint32_t>(bit_length);
    words[15] = static_cast<std::uint32_t>(bit_length >> 32);
    compress(state, words);
    return state;
}

__global__ void check_candidates(CudaDeviceBatch batch, DeviceDigest target, CudaMatch* match) {
    const auto stride = static_cast<std::size_t>(blockDim.x) * gridDim.x;
    for (auto index = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
         index < batch.count; index += stride) {
        const auto begin = batch.offsets[index];
        const auto size = batch.offsets[index + 1] - begin;
        const auto digest = calculate(batch.bytes + begin, size);
        if (digest.words[0] == target.words[0] && digest.words[1] == target.words[1] &&
            digest.words[2] == target.words[2] && digest.words[3] == target.words[3]) {
            atomicMin(&match->index, static_cast<unsigned long long>(index));
        }
    }
}

}

Md5Cuda::Session::Session(const target_type& target, const CudaContext& context,
                          std::size_t batch_size)
    : batch_size_(batch_size),
      threads_(static_cast<unsigned>(std::min(256, context.properties().maxThreadsPerBlock))),
      max_grid_(static_cast<unsigned>(context.properties().maxGridSize[0])) {
    for (std::size_t word = 0; word < 4; ++word) {
        for (std::size_t byte = 0; byte < 4; ++byte) {
            digest_[word] |= static_cast<std::uint32_t>(target.digest[4 * word + byte])
                             << (8 * byte);
        }
    }
}

void Md5Cuda::Session::launch(CudaDeviceBatch batch, CudaMatch* match, cudaStream_t stream) const {
    if (batch.count == 0) {
        return;
    }
    const auto blocks = std::min(batch.count / threads_ + (batch.count % threads_ != 0),
                                 static_cast<std::size_t>(max_grid_));
    const DeviceDigest target{{digest_[0], digest_[1], digest_[2], digest_[3]}};
    check_candidates<<<static_cast<unsigned>(blocks), threads_, 0, stream>>>(batch, target, match);
    check_cuda(cudaGetLastError(), "MD5 kernel launch");
}

Md5Cuda::Session Md5Cuda::create_session(const target_type& target, const CudaContext& context,
                                         std::size_t batch_size) const {
    return Session(target, context, batch_size);
}

}
