#include "pwbench/algorithms/pbkdf2_sha256.impl.cuh"
#include "pwbench/cuda_executor.hpp"

#include "cuda_test_support.hpp"
#include "fixtures.hpp"

#include <array>

namespace {

using pwbench::Pbkdf2Sha256;
using pwbench::Pbkdf2Sha256Cuda;
using Pbkdf2Sha256CudaTest = fixtures::CudaTest;

constexpr Pbkdf2Sha256::salt_type salt{'p', 'w', 'b', 'e', 'n', 'c', 'h', '-',
                                       'p', 'b', 'k', 'd', 'f', '2', '!', '!'};

TEST_F(Pbkdf2Sha256CudaTest, MatchesIndependentKnownVectors) {
    const Pbkdf2Sha256 cpu;
    const auto password = fixtures::bytes("password");
    for (const auto iterations : {1U, 2U, 1000U}) {
        const auto target = cpu.make_target(fixtures::view(password), salt, iterations);
        if (iterations == 2) {
            EXPECT_EQ(Pbkdf2Sha256::hex(target.digest.data(), target.digest.size()),
                      "d5fa144b262585c0185bbaebab8b1c109d2bace13a9370c99dd11e220d45cbd6");
        }
        fixtures::BlockStrategy source({fixtures::bytes("other"), password}, 1);
        const auto result =
            pwbench::CudaExecutor::run(Pbkdf2Sha256Cuda{}, target, source, {}, {.batch_size = 2});
        EXPECT_EQ(result.reason, pwbench::StopReason::found) << result.error;
        EXPECT_EQ(result.candidate, password);
        EXPECT_EQ(result.checks, 2);
    }
}

TEST_F(Pbkdf2Sha256CudaTest, HandlesEmptyBinaryAndLongKeys) {
    const Pbkdf2Sha256 cpu;
    pwbench::Candidate binary{'a', 0, 'b', 0xff};
    pwbench::Candidate long_key(129);
    for (std::size_t index = 0; index < long_key.size(); ++index) {
        long_key[index] = static_cast<pwbench::Byte>(index);
    }
    for (const auto& password : {pwbench::Candidate{}, binary, long_key}) {
        const auto target = cpu.make_target(fixtures::view(password), salt, 17);
        fixtures::BlockStrategy source({fixtures::bytes("wrong"), password});
        const auto result = pwbench::CudaExecutor::run(Pbkdf2Sha256Cuda{}, target, source, {},
                                                       {.batch_size = 2, .batch_bytes = 5});
        EXPECT_EQ(result.reason, pwbench::StopReason::found) << result.error;
        EXPECT_EQ(result.candidate, password);
        EXPECT_EQ(result.checks, 2);
    }
}

TEST_F(Pbkdf2Sha256CudaTest, MatchesCpuAtHmacKeyBoundaries) {
    const Pbkdf2Sha256 cpu;
    for (const auto size : {63U, 64U, 65U, 119U, 120U}) {
        pwbench::Candidate password(size);
        for (std::size_t index = 0; index < password.size(); ++index) {
            password[index] = static_cast<pwbench::Byte>(index);
        }
        const auto target = cpu.make_target(fixtures::view(password), salt, 2);
        fixtures::BlockStrategy source({password});
        const auto result = pwbench::CudaExecutor::run(Pbkdf2Sha256Cuda{}, target, source);
        EXPECT_EQ(result.reason, pwbench::StopReason::found) << result.error;
        EXPECT_EQ(result.candidate, password);
    }
}

TEST_F(Pbkdf2Sha256CudaTest, RejectsWrongDigestAndExhaustsSource) {
    const Pbkdf2Sha256 cpu;
    const auto password = fixtures::bytes("password");
    auto target = cpu.make_target(fixtures::view(password), salt, 2);
    target.digest[31] ^= 1;
    fixtures::BlockStrategy source({password});
    const auto result = pwbench::CudaExecutor::run(Pbkdf2Sha256Cuda{}, target, source);
    EXPECT_EQ(result.reason, pwbench::StopReason::exhausted) << result.error;
    EXPECT_EQ(result.checks, 1);
}

}
