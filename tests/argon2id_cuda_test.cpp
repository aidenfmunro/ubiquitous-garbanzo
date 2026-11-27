#include "cuda_test_support.hpp"
#include "pwbench/algorithms/argon2id.impl.cuh"

namespace {

using pwbench::Argon2id;
using pwbench::argon2id_cuda_hash;
using pwbench::Argon2idParameters;
using pwbench::Byte;
using Argon2idCudaTest = fixtures::CudaTest;

constexpr Argon2id::salt_type salt{'p', 'w', 'b', 'e', 'n', 'c', 'h', '-',
                                   'a', 'r', 'g', 'o', 'n', '2', 'i', 'd'};

TEST_F(Argon2idCudaTest, MatchesKnownBinaryVector) {
    constexpr Byte input[]{0x61, 0x00, 0x62, 0xff};
    constexpr Argon2id::digest_type expected{0x58, 0xa5, 0xa1, 0xde, 0xbe, 0x1a, 0xd8, 0xc3,
                                             0xf2, 0xa9, 0x40, 0x5a, 0x51, 0xd3, 0x36, 0xbb,
                                             0x33, 0x69, 0x79, 0x36, 0xc4, 0xfd, 0xb8, 0xfa,
                                             0x7c, 0x4a, 0x71, 0xbc, 0xab, 0x16, 0x30, 0x36};
    EXPECT_EQ(argon2id_cuda_hash(input, salt, {32, 2, 2}), expected);
}

TEST_F(Argon2idCudaTest, MatchesCpuAcrossMemorySizesAndLanes) {
    constexpr Argon2idParameters costs[]{{8, 1, 1},    {27, 3, 3},   {32, 1, 4}, {512, 1, 1},
                                         {1559, 2, 3}, {4096, 3, 4}, {}};
    std::array<Byte, 256> input;
    for (std::size_t i = 0; i < input.size(); ++i) {
        input[i] = static_cast<Byte>(i);
    }
    for (const auto cost : costs) {
        SCOPED_TRACE(::testing::Message()
                     << "m=" << cost.memory_kib << " t=" << cost.iterations << " p=" << cost.lanes);
        for (const std::size_t length : {0, 72, 73, 256}) {
            SCOPED_TRACE(length);
            EXPECT_EQ(argon2id_cuda_hash(std::span{input}.first(length), salt, cost),
                      Argon2id{}.hash({input.data(), length}, {{}, salt, cost}));
        }
    }
}

TEST(Argon2idCudaValidationTest, RejectsInvalidCostsBeforeCreatingCudaContext) {
    EXPECT_THROW(argon2id_cuda_hash({}, salt, {8, 1, 2}, -1), std::invalid_argument);
    EXPECT_THROW(argon2id_cuda_hash({}, salt, {8, 0, 1}, -1), std::invalid_argument);
    EXPECT_THROW(argon2id_cuda_hash({}, salt, {8, 1, 0}, -1), std::invalid_argument);
    EXPECT_THROW(argon2id_cuda_hash({}, salt, {8, 1, 0x01000000}, -1), std::invalid_argument);
}

}
