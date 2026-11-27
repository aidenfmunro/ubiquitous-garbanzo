#include "pwbench/algorithms/sha256.impl.hpp"

#include <algorithm>
#include <bit>
#include <stdexcept>

namespace pwbench {
namespace {

using State = std::array<std::uint32_t, 8>;

constexpr std::array<std::uint32_t, 64> constants{
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

std::uint32_t read_word(const Byte* bytes) {
    return (static_cast<std::uint32_t>(bytes[0]) << 24) |
           (static_cast<std::uint32_t>(bytes[1]) << 16) |
           (static_cast<std::uint32_t>(bytes[2]) << 8) | static_cast<std::uint32_t>(bytes[3]);
}

void compress(State& state, const Byte* block) {
    std::array<std::uint32_t, 64> words;
    for (std::size_t index = 0; index < 16; ++index) {
        words[index] = read_word(block + 4 * index);
    }
    for (std::size_t index = 16; index < words.size(); ++index) {
        const auto previous = words[index - 15];
        const auto recent = words[index - 2];
        const auto sigma0 = std::rotr(previous, 7) ^ std::rotr(previous, 18) ^ (previous >> 3);
        const auto sigma1 = std::rotr(recent, 17) ^ std::rotr(recent, 19) ^ (recent >> 10);
        words[index] = words[index - 16] + sigma0 + words[index - 7] + sigma1;
    }

    auto [a, b, c, d, e, f, g, h] = state;
    for (std::size_t round = 0; round < 64; ++round) {
        const auto sum1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
        const auto choose = (e & f) ^ (~e & g);
        const auto temporary1 = h + sum1 + choose + constants[round] + words[round];
        const auto sum0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
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
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void finish(State& state, CandidateView remainder, std::uint64_t bit_length) {
    std::array<Byte, 128> tail{};
    if (remainder.size != 0) {
        std::copy_n(remainder.data, remainder.size, tail.data());
    }
    tail[remainder.size] = 0x80;
    const auto padded_size = remainder.size < 56 ? 64 : 128;
    for (int index = 0; index < 8; ++index) {
        tail[padded_size - 8 + index] = static_cast<Byte>(bit_length >> (56 - 8 * index));
    }
    compress(state, tail.data());
    if (padded_size == 128) {
        compress(state, tail.data() + 64);
    }
}

Sha256::digest_type encode(const State& state) {
    Sha256::digest_type digest;
    for (std::size_t word = 0; word < state.size(); ++word) {
        for (std::size_t byte = 0; byte < 4; ++byte) {
            digest[4 * word + byte] = static_cast<Byte>(state[word] >> (24 - 8 * byte));
        }
    }
    return digest;
}

Sha256::digest_type calculate_digest(CandidateView candidate) {
    if (candidate.size != 0 && candidate.data == nullptr) {
        throw std::invalid_argument("nonempty SHA-256 input has null data");
    }
    State state{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::size_t offset = 0;
    while (candidate.size - offset >= 64) {
        compress(state, candidate.data + offset);
        offset += 64;
    }
    const CandidateView remainder{candidate.data == nullptr ? nullptr : candidate.data + offset,
                                  candidate.size - offset};
    finish(state, remainder, static_cast<std::uint64_t>(candidate.size) * 8);
    return encode(state);
}

}

Sha256::digest_type Sha256::hash(CandidateView candidate, const target_type&) const {
    return calculate_digest(candidate);
}

Sha256::target_type Sha256::make_target(CandidateView candidate) const {
    return {calculate_digest(candidate)};
}

std::string Sha256::hex(const digest_type& digest) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result(digest.size() * 2, '0');
    for (std::size_t index = 0; index < digest.size(); ++index) {
        result[2 * index] = digits[digest[index] >> 4];
        result[2 * index + 1] = digits[digest[index] & 0x0f];
    }
    return result;
}

}
