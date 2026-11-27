#include "pwbench/algorithms/sha256.impl.cuh"

#include <cuda_runtime.h>

#include <algorithm>

namespace pwbench {
namespace {

struct DeviceDigest {
    std::uint32_t words[8];
};

__device__ std::uint32_t read_word(const Byte* bytes) {
    return (static_cast<std::uint32_t>(bytes[0]) << 24) |
           (static_cast<std::uint32_t>(bytes[1]) << 16) |
           (static_cast<std::uint32_t>(bytes[2]) << 8) | static_cast<std::uint32_t>(bytes[3]);
}

__device__ std::uint32_t rotate_right(std::uint32_t value, unsigned rotation) {
    return __funnelshift_r(value, value, rotation);
}

__device__ void compress(DeviceDigest& state, const std::uint32_t* input) {
    constexpr std::uint32_t constants[64]{
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
        0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
        0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
        0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
        0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
        0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
        0xc67178f2,
    };
    std::uint32_t words[64];
#pragma unroll
    for (unsigned index = 0; index < 16; ++index) {
        words[index] = input[index];
    }
#pragma unroll
    for (unsigned index = 16; index < 64; ++index) {
        const auto previous = words[index - 15];
        const auto recent = words[index - 2];
        const auto sigma0 =
            rotate_right(previous, 7) ^ rotate_right(previous, 18) ^ (previous >> 3);
        const auto sigma1 = rotate_right(recent, 17) ^ rotate_right(recent, 19) ^ (recent >> 10);
        words[index] = words[index - 16] + sigma0 + words[index - 7] + sigma1;
    }

    auto a = state.words[0];
    auto b = state.words[1];
    auto c = state.words[2];
    auto d = state.words[3];
    auto e = state.words[4];
    auto f = state.words[5];
    auto g = state.words[6];
    auto h = state.words[7];
#pragma unroll
    for (unsigned round = 0; round < 64; ++round) {
        const auto sum1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
        const auto choose = (e & f) ^ (~e & g);
        const auto temporary1 = h + sum1 + choose + constants[round] + words[round];
        const auto sum0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
        const auto majority = (a & b) ^ (a & c) ^ (b & c);
        const auto temporary2 = sum0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temporary1;
        d = c;
        c = b;
        b = a;
        a = temporary1 + temporary2;
    }
    state.words[0] += a;
    state.words[1] += b;
    state.words[2] += c;
    state.words[3] += d;
    state.words[4] += e;
    state.words[5] += f;
    state.words[6] += g;
    state.words[7] += h;
}

__device__ DeviceDigest calculate(const Byte* bytes, std::uint64_t size) {
    DeviceDigest state{{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c,
                        0x1f83d9ab, 0x5be0cd19}};
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
        words[index / 4] |= static_cast<std::uint32_t>(bytes[offset + index])
                            << (24 - 8 * (index % 4));
    }
    words[remainder / 4] |= std::uint32_t{0x80} << (24 - 8 * (remainder % 4));
    if (remainder >= 56) {
        compress(state, words);
#pragma unroll
        for (unsigned index = 0; index < 16; ++index) {
            words[index] = 0;
        }
    }
    const auto bit_length = size * 8;
    words[14] = static_cast<std::uint32_t>(bit_length >> 32);
    words[15] = static_cast<std::uint32_t>(bit_length);
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
        bool matches = true;
#pragma unroll
        for (unsigned word = 0; word < 8; ++word) {
            matches &= digest.words[word] == target.words[word];
        }
        if (matches) {
            atomicMin(&match->index, static_cast<unsigned long long>(index));
        }
    }
}

}

Sha256Cuda::Session::Session(const target_type& target, const CudaContext& context,
                             std::size_t batch_size)
    : batch_size_(batch_size),
      threads_(static_cast<unsigned>(std::min(256, context.properties().maxThreadsPerBlock))),
      max_grid_(static_cast<unsigned>(context.properties().maxGridSize[0])) {
    for (std::size_t word = 0; word < 8; ++word) {
        for (std::size_t byte = 0; byte < 4; ++byte) {
            digest_[word] |= static_cast<std::uint32_t>(target.digest[4 * word + byte])
                             << (24 - 8 * byte);
        }
    }
}

void Sha256Cuda::Session::launch(CudaDeviceBatch batch, CudaMatch* match,
                                 cudaStream_t stream) const {
    if (batch.count == 0) {
        return;
    }
    const auto blocks = std::min(batch.count / threads_ + (batch.count % threads_ != 0),
                                 static_cast<std::size_t>(max_grid_));
    const DeviceDigest target{{digest_[0], digest_[1], digest_[2], digest_[3], digest_[4],
                               digest_[5], digest_[6], digest_[7]}};
    check_candidates<<<static_cast<unsigned>(blocks), threads_, 0, stream>>>(batch, target, match);
    check_cuda(cudaGetLastError(), "SHA-256 kernel launch");
}

Sha256Cuda::Session Sha256Cuda::create_session(const target_type& target,
                                               const CudaContext& context,
                                               std::size_t batch_size) const {
    return Session(target, context, batch_size);
}

}
