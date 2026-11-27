#include "pwbench/algorithms/argon2id.impl.cuh"
#include "pwbench/cuda/runtime.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <stdexcept>
#include <vector>

namespace pwbench {
namespace {

using Word = std::uint64_t;
constexpr unsigned words_per_block = 128;
constexpr unsigned slices = 4;
constexpr unsigned threads_per_lane = 32;
constexpr unsigned argon2id_type = 2;
constexpr unsigned argon2_version = 0x13;

struct Block {
    Word words[words_per_block];
};

static_assert(sizeof(Block) == 1024);

constexpr Word blake2b_iv[]{
    0x6a09e667f3bcc908, 0xbb67ae8584caa73b, 0x3c6ef372fe94f82b, 0xa54ff53a5f1d36f1,
    0x510e527fade682d1, 0x9b05688c2b3e6c1f, 0x1f83d9abfb41bd6b, 0x5be0cd19137e2179,
};

constexpr unsigned blake2b_schedule[10][16]{
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    {14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3},
    {11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4},
    {7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8},
    {9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13},
    {2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9},
    {12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11},
    {13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10},
    {6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5},
    {10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0},
};

template <class Integer>
void store_le(Byte* destination, Integer value) {
    for (unsigned i = 0; i < sizeof(Integer); ++i) {
        destination[i] = static_cast<Byte>(value >> (8 * i));
    }
}

void append_le32(std::vector<Byte>& bytes, std::uint32_t value) {
    const auto offset = bytes.size();
    bytes.resize(offset + sizeof(value));
    store_le(bytes.data() + offset, value);
}

void blake2b_mix(Word& a, Word& b, Word& c, Word& d, Word x, Word y) {
    a += b + x;
    d = std::rotr(d ^ a, 32);
    c += d;
    b = std::rotr(b ^ c, 24);
    a += b + y;
    d = std::rotr(d ^ a, 16);
    c += d;
    b = std::rotr(b ^ c, 63);
}

void blake2b(std::span<const Byte> input, std::span<Byte> output) {
    std::array<Word, 8> state;
    std::ranges::copy(blake2b_iv, state.begin());
    state[0] ^= 0x01010000 | output.size();
    Word processed{};
    do {
        Word message[16]{};
        const auto block = input.first(std::min(input.size(), sizeof(message)));
        for (std::size_t i = 0; i < block.size(); ++i) {
            message[i / 8] |= static_cast<Word>(block[i]) << (8 * (i % 8));
        }
        processed += block.size();
        input = input.subspan(block.size());

        Word v[16];
        std::ranges::copy(state, v);
        std::ranges::copy(blake2b_iv, v + state.size());
        v[12] ^= processed;
        if (input.empty()) {
            v[14] = ~v[14];
        }
        for (unsigned round = 0; round < 12; ++round) {
            const auto& s = blake2b_schedule[round % 10];
            for (unsigned column = 0; column < 4; ++column) {
                blake2b_mix(v[column], v[4 + column], v[8 + column], v[12 + column],
                            message[s[2 * column]], message[s[2 * column + 1]]);
            }
            for (unsigned column = 0; column < 4; ++column) {
                blake2b_mix(v[column], v[4 + (column + 1) % 4], v[8 + (column + 2) % 4],
                            v[12 + (column + 3) % 4], message[s[8 + 2 * column]],
                            message[s[9 + 2 * column]]);
            }
        }
        for (unsigned i = 0; i < state.size(); ++i) {
            state[i] ^= v[i] ^ v[i + state.size()];
        }
    } while (!input.empty());

    for (std::size_t i = 0; i < output.size(); ++i) {
        output[i] = static_cast<Byte>(state[i / 8] >> (8 * (i % 8)));
    }
}

// RFC 9106 H' gets the output length as the first four bytes of its input.
void blake2b_long(std::span<const Byte> prefixed, std::span<Byte> output) {
    if (output.size() <= 64) {
        blake2b(prefixed, output);
        return;
    }
    std::array<Byte, 64> intermediate;
    blake2b(prefixed, intermediate);
    while (output.size() > intermediate.size()) {
        std::ranges::copy_n(intermediate.begin(), 32, output.begin());
        output = output.subspan(32);
        if (output.size() > intermediate.size()) {
            blake2b(intermediate, intermediate);
        }
    }
    blake2b(intermediate, output);
}

std::vector<Block> initial_blocks(std::span<const Byte> input, const Argon2id::salt_type& salt,
                                  Argon2idParameters costs) {
    std::vector<Byte> prehash;
    prehash.reserve(10 * sizeof(std::uint32_t) + input.size() + salt.size());
    for (const auto field :
         {costs.lanes, static_cast<std::uint32_t>(Argon2id::digest_type{}.size()), costs.memory_kib,
          costs.iterations, argon2_version, argon2id_type,
          static_cast<std::uint32_t>(input.size())}) {
        append_le32(prehash, field);
    }
    if (!input.empty()) {
        prehash.insert(prehash.end(), input.begin(), input.end());
    }
    append_le32(prehash, static_cast<std::uint32_t>(salt.size()));
    prehash.insert(prehash.end(), salt.begin(), salt.end());
    append_le32(prehash, 0); // Secret length.
    append_le32(prehash, 0); // Associated data length.

    std::array<Byte, 76> seed{};
    store_le(seed.data(), static_cast<std::uint32_t>(sizeof(Block)));
    blake2b(prehash, std::span{seed}.subspan<4, 64>());

    // CUDA device words are little-endian, as are the host words on CUDA platforms.
    static_assert(std::endian::native == std::endian::little);
    std::vector<Block> initial(static_cast<std::size_t>(costs.lanes) * 2);
    for (std::uint32_t lane = 0; lane < costs.lanes; ++lane) {
        store_le(seed.data() + 72, lane);
        for (std::uint32_t index = 0; index < 2; ++index) {
            store_le(seed.data() + 68, index);
            auto& block = initial[static_cast<std::size_t>(lane) * 2 + index];
            blake2b_long(seed, {reinterpret_cast<Byte*>(&block), sizeof(block)});
        }
    }
    return initial;
}

__device__ Word rotate_right(Word value, unsigned bits) {
    return (value >> bits) | (value << (64 - bits));
}

__device__ Word blamka(Word a, Word b) {
    return a + b +
           2 * static_cast<Word>(static_cast<std::uint32_t>(a)) * static_cast<std::uint32_t>(b);
}

__device__ void argon2_mix(Word& a, Word& b, Word& c, Word& d) {
    a = blamka(a, b);
    d = rotate_right(d ^ a, 32);
    c = blamka(c, d);
    b = rotate_right(b ^ c, 24);
    a = blamka(a, b);
    d = rotate_right(d ^ a, 16);
    c = blamka(c, d);
    b = rotate_right(b ^ c, 63);
}

__device__ unsigned matrix_word(unsigned line, unsigned element, bool column) {
    // A matrix entry holds two consecutive words.
    return column ? (element / 2) * 16 + 2 * line + element % 2 : 16 * line + element;
}

__device__ void mix_matrix(Word* matrix, bool column, bool diagonal) {
    const auto line = threadIdx.x / 4;
    const auto group = threadIdx.x % 4;
    const auto step = static_cast<unsigned>(diagonal);
    argon2_mix(matrix[matrix_word(line, group, column)],
               matrix[matrix_word(line, 4 + (group + step) % 4, column)],
               matrix[matrix_word(line, 8 + (group + 2 * step) % 4, column)],
               matrix[matrix_word(line, 12 + (group + 3 * step) % 4, column)]);
    __syncthreads();
}

__device__ void compress(Block* output, const Block* left, const Block* right, Word* matrix,
                         bool xor_old) {
    Word feedforward[words_per_block / threads_per_lane];
    for (unsigned i = 0; i < words_per_block / threads_per_lane; ++i) {
        const auto word = threadIdx.x + i * threads_per_lane;
        const auto mixed = (left ? left->words[word] : 0) ^ right->words[word];
        matrix[word] = mixed;
        feedforward[i] = mixed ^ (xor_old ? output->words[word] : 0);
    }
    __syncthreads();

    mix_matrix(matrix, false, false);
    mix_matrix(matrix, false, true);
    mix_matrix(matrix, true, false);
    mix_matrix(matrix, true, true);

    for (unsigned i = 0; i < words_per_block / threads_per_lane; ++i) {
        const auto word = threadIdx.x + i * threads_per_lane;
        output->words[word] = matrix[word] ^ feedforward[i];
    }
    __syncthreads();
}

__global__ void initialize_lanes(Block* memory, const Block* seeds, unsigned lane_length) {
    const auto lane_start = static_cast<std::size_t>(blockIdx.x) * lane_length;
    const auto seed_start = static_cast<std::size_t>(blockIdx.x) * 2;
    memory[lane_start].words[threadIdx.x] = seeds[seed_start].words[threadIdx.x];
    memory[lane_start + 1].words[threadIdx.x] = seeds[seed_start + 1].words[threadIdx.x];
}

__global__ void fill_slice(Block* memory, unsigned segment_length, unsigned iterations,
                           unsigned pass, unsigned slice) {
    __shared__ Word matrix[words_per_block];
    __shared__ Block address_input;
    __shared__ Block addresses;

    const auto lane = blockIdx.x;
    const auto lane_length = slices * segment_length;
    const auto slice_start = slice * segment_length;
    const auto segment_start = pass == 0 && slice == 0 ? 2U : 0U;
    const auto lane_start = static_cast<std::size_t>(lane) * lane_length;
    const bool independent = pass == 0 && slice < 2;
    const auto reference_start =
        pass != 0 && slice != slices - 1 ? slice_start + segment_length : 0;

    if (independent) {
        const Word fields[]{pass,       lane,
                            slice,      static_cast<Word>(gridDim.x) * lane_length,
                            iterations, argon2id_type};
        for (unsigned word = threadIdx.x; word < words_per_block; word += blockDim.x) {
            address_input.words[word] = word < 6 ? fields[word] : 0;
        }
        __syncthreads();
    }

    for (unsigned index = segment_start; index < segment_length; ++index) {
        const auto position = slice_start + index;
        const auto previous_position = position == 0 ? lane_length - 1 : position - 1;
        const auto* previous = memory + lane_start + previous_position;

        if (independent && (index == segment_start || index % words_per_block == 0)) {
            if (threadIdx.x == 0) {
                address_input.words[6] = index / words_per_block + 1;
            }
            __syncthreads();
            compress(&addresses, nullptr, &address_input, matrix, false);
            compress(&addresses, nullptr, &addresses, matrix, false);
        }
        const auto random =
            independent ? addresses.words[index % words_per_block] : previous->words[0];
        const auto reference_lane =
            pass == 0 && slice == 0 ? lane : static_cast<unsigned>(random >> 32) % gridDim.x;

        // Other lanes cannot reference the segment currently being written.
        const auto completed = pass == 0 ? slice_start : lane_length - segment_length;
        const auto area = reference_lane == lane ? completed + index - 1 : completed - (index == 0);
        const Word low = static_cast<std::uint32_t>(random);
        const auto scaled = (low * low) >> 32;
        const auto relative = area - 1 - ((area * scaled) >> 32);
        const auto reference_position = (reference_start + relative) % lane_length;
        const auto* reference =
            memory + static_cast<std::size_t>(reference_lane) * lane_length + reference_position;
        compress(memory + lane_start + position, previous, reference, matrix, pass != 0);
    }
}

__global__ void reduce_lanes(const Block* memory, Block* output, unsigned lanes,
                             unsigned lane_length) {
    Word result{};
    for (unsigned lane = 0; lane < lanes; ++lane) {
        const auto end = (static_cast<std::size_t>(lane) + 1) * lane_length - 1;
        result ^= memory[end].words[threadIdx.x];
    }
    output->words[threadIdx.x] = result;
}

}

Argon2id::digest_type argon2id_cuda_hash(std::span<const Byte> input,
                                         const Argon2id::salt_type& salt, Argon2idParameters costs,
                                         int device) {
    if (costs.lanes == 0 || costs.lanes > 0x00ffffff || costs.memory_kib < 8 * costs.lanes ||
        costs.iterations == 0 || input.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("invalid Argon2id input length or costs");
    }

    const auto segment_length = costs.memory_kib / (slices * costs.lanes);
    const auto lane_length = segment_length * slices;
    const CudaContext context(device);
    const auto stream = context.stream();
    if (costs.lanes > static_cast<unsigned>(context.properties().maxGridSize[0])) {
        throw std::invalid_argument("Argon2id lanes exceed the CUDA grid limit");
    }

    CudaBuffer<Block> matrix(static_cast<std::size_t>(lane_length) * costs.lanes);
    CudaBuffer<Block> workspace(static_cast<std::size_t>(costs.lanes) * 2);
    const auto seeds = initial_blocks(input, salt, costs);
    Block last;
    try {
        check_cuda(cudaMemcpyAsync(workspace.data(), seeds.data(), seeds.size() * sizeof(Block),
                                   cudaMemcpyHostToDevice, stream),
                   "cudaMemcpyAsync(Argon2id seeds)");
        initialize_lanes<<<costs.lanes, words_per_block, 0, stream>>>(
            matrix.data(), workspace.data(), lane_length);
        check_cuda(cudaGetLastError(), "initialize_lanes");
        // Separate launches synchronize slices across all lanes on the same stream.
        for (unsigned pass = 0; pass < costs.iterations; ++pass) {
            for (unsigned slice = 0; slice < slices; ++slice) {
                fill_slice<<<costs.lanes, threads_per_lane, 0, stream>>>(
                    matrix.data(), segment_length, costs.iterations, pass, slice);
                check_cuda(cudaGetLastError(), "fill_slice");
            }
        }
        reduce_lanes<<<1, words_per_block, 0, stream>>>(matrix.data(), workspace.data(),
                                                        costs.lanes, lane_length);
        check_cuda(cudaGetLastError(), "reduce_lanes");
        check_cuda(
            cudaMemcpyAsync(&last, workspace.data(), sizeof(last), cudaMemcpyDeviceToHost, stream),
            "cudaMemcpyAsync(Argon2id final block)");
        context.synchronize();
    } catch (...) {
        context.drain();
        throw;
    }

    Argon2id::digest_type digest;
    std::array<Byte, sizeof(std::uint32_t) + sizeof(last)> prefixed;
    store_le(prefixed.data(), static_cast<std::uint32_t>(digest.size()));
    std::ranges::copy_n(reinterpret_cast<const Byte*>(&last), sizeof(last), prefixed.begin() + 4);
    blake2b_long(prefixed, digest);
    return digest;
}

}
