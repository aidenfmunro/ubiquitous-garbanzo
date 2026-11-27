#include "pwbench/algorithms/argon2id.impl.hpp"
#include "pwbench/contracts.hpp"

#include <gtest/gtest.h>

namespace {

using pwbench::Argon2id;

static_assert(pwbench::CpuAlgorithm<Argon2id>);

constexpr Argon2id::salt_type salt{'p', 'w', 'b', 'e', 'n', 'c', 'h', '-',
                                   'a', 'r', 'g', 'o', 'n', '2', 'i', 'd'};

TEST(Argon2idTest, MatchesKnownVectorWithBinaryPassword) {
    const Argon2id algorithm;
    const pwbench::Byte password[]{0x61, 0x00, 0x62, 0xff};
    const pwbench::CandidateView input{password, sizeof(password)};
    const auto prepared =
        algorithm.make_target(input, salt, {.memory_kib = 32, .iterations = 2, .lanes = 2});
    // Checked independently using the OpenSSL ARGON2ID KDF.
    EXPECT_EQ(prepared.encoded, "$argon2id$v=19$m=32,t=2,p=2$cHdiZW5jaC1hcmdvbjJpZA"
                                "$WKWh3r4a2MPyqUBaUdM2uzNpeTbE/bj6fEpxvKsWMDY");
    EXPECT_TRUE(algorithm.matches(algorithm.hash(input, prepared.target), prepared.target));

    const pwbench::Byte different_password[]{0x61, 0x00, 0x62, 0xfe};
    const pwbench::CandidateView different{different_password, sizeof(different_password)};
    EXPECT_FALSE(algorithm.matches(algorithm.hash(different, prepared.target), prepared.target));
}

TEST(Argon2idTest, HandlesEmptyPasswordAndValidatesCosts) {
    const Argon2id algorithm;
    const auto prepared = algorithm.make_target({}, salt, {.memory_kib = 8, .iterations = 1});
    EXPECT_TRUE(algorithm.matches(algorithm.hash({}, prepared.target), prepared.target));

    EXPECT_THROW(algorithm.hash({nullptr, 1}, prepared.target), std::invalid_argument);
    EXPECT_THROW(algorithm.make_target({}, salt, {.memory_kib = 8, .iterations = 1, .lanes = 2}),
                 std::runtime_error);
}

}
