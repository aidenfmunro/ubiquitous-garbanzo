#include "pwbench/algorithms/md5.cuh"
#include "pwbench/cuda_executor.hpp"
#include "pwbench/strategies/wordlist.hpp"

#include "cuda_test_support.hpp"
#include "fixtures.hpp"
#include "temp_directory.hpp"

#include <charconv>
#include <random>

namespace {

using Md5CudaTest = fixtures::CudaTest;

pwbench::Md5::target_type known_target(std::string_view hex) {
    pwbench::Md5::target_type target;
    if (hex.size() != 32) {
        throw std::invalid_argument("invalid test digest length");
    }
    for (std::size_t index = 0; index < 16; ++index) {
        unsigned value{};
        const auto* begin = hex.data() + 2 * index;
        const auto [end, error] = std::from_chars(begin, begin + 2, value, 16);
        if (error != std::errc{} || end != begin + 2) {
            throw std::invalid_argument("invalid test digest");
        }
        target.digest[index] = static_cast<pwbench::Byte>(value);
    }
    return target;
}

void expects_hash(const pwbench::Candidate& candidate, std::string_view expected) {
    fixtures::BlockStrategy source({fixtures::bytes("different candidate"), candidate}, 1);
    const auto result =
        pwbench::CudaExecutor::run(pwbench::Md5Cuda{}, known_target(expected), source);
    EXPECT_EQ(result.reason, pwbench::StopReason::found) << result.error;
    EXPECT_EQ(result.candidate, candidate);
    EXPECT_EQ(result.checks, 2U);
}

TEST_F(Md5CudaTest, MatchesRfc1321Vectors) {
    const std::pair<std::string_view, std::string_view> vectors[]{
        {"", "d41d8cd98f00b204e9800998ecf8427e"},
        {"a", "0cc175b9c0f1b6a831c399e269772661"},
        {"abc", "900150983cd24fb0d6963f7d28e17f72"},
        {"message digest", "f96b697d7cb7938d525a2f31aaf161d0"},
        {"abcdefghijklmnopqrstuvwxyz", "c3fcd3d76192e4007dfb496cca67e13b"},
        {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
         "d174ab98d277d9f5a5611c2c9f419d9f"},
        {"12345678901234567890123456789012345678901234567890123456789012345678901234567890",
         "57edf4a22be3c955ac49da2e2107b67a"},
    };
    for (const auto& [input, hash] : vectors) {
        SCOPED_TRACE(input);
        expects_hash(fixtures::bytes(input), hash);
    }
}

TEST_F(Md5CudaTest, HandlesPaddingBoundariesAndLargeInputs) {
    const std::pair<std::size_t, std::string_view> vectors[]{
        {55, "ef1772b6dff9a122358552954ad0df65"},  {56, "3b0c8ac703f828b04c6c197006d17218"},
        {63, "b06521f39153d618550606be297466d5"},  {64, "014842d480b571495a4a0363793f7367"},
        {65, "c743a45e0d2e6a95cb859adae0248435"},  {119, "8a7bd0732ed6a28ce75f6dabc90e1613"},
        {120, "5f61c0ccad4cac44c75ff505e1f1e537"}, {127, "020406e1d05cdc2aa287641f7ae2cc39"},
        {128, "e510683b3f5ffe4093d021808bc6ff70"}, {1'000'000, "7707d6ae4e027c70eea2a935c2296f21"},
    };
    for (const auto& [size, hash] : vectors) {
        SCOPED_TRACE(size);
        expects_hash(pwbench::Candidate(size, 'a'), hash);
    }
}

TEST_F(Md5CudaTest, HandlesBinaryAndUnalignedDeviceInputs) {
    pwbench::Candidate candidate(256);
    for (std::size_t index = 0; index < candidate.size(); ++index) {
        candidate[index] = static_cast<pwbench::Byte>(index);
    }
    fixtures::BlockStrategy source({{0}, candidate, {}}, 1);
    const auto result = pwbench::CudaExecutor::run(
        pwbench::Md5Cuda{}, known_target("e2c865db4162bed963bfaa9ef6ac18f0"), source);
    EXPECT_EQ(result.reason, pwbench::StopReason::found) << result.error;
    EXPECT_EQ(result.candidate, candidate);
    EXPECT_EQ(result.checks, 3U);
}

TEST_F(Md5CudaTest, ChecksEveryDigestWord) {
    const auto candidate = fixtures::bytes("expected");
    for (std::size_t byte = 0; byte < 16; ++byte) {
        auto target = pwbench::Md5{}.make_target(fixtures::view(candidate));
        target.digest[byte] ^= 1;
        fixtures::BlockStrategy source({candidate});
        const auto result = pwbench::CudaExecutor::run(pwbench::Md5Cuda{}, target, source);
        EXPECT_EQ(result.reason, pwbench::StopReason::exhausted) << result.error;
        EXPECT_EQ(result.checks, 1U);
        EXPECT_FALSE(result.candidate);
    }
}

TEST_F(Md5CudaTest, MatchesCpuForRandomBinaryInputsAndDifferentBatchSizes) {
    std::mt19937 random(0x51da);
    std::vector<pwbench::Candidate> candidates;
    for (std::size_t index = 0; index < 301; ++index) {
        auto& candidate = candidates.emplace_back(random() % 257);
        for (auto& byte : candidate) {
            byte = static_cast<pwbench::Byte>(random());
        }
    }
    const pwbench::Md5 cpu;
    for (const auto selected : {0U, 1U, 54U, 55U, 63U, 127U, 255U, 300U}) {
        const auto target = cpu.make_target(fixtures::view(candidates[selected]));
        for (const auto batch_size : {1U, 7U, 256U, 512U}) {
            fixtures::BlockStrategy source(candidates, 11);
            const auto result = pwbench::CudaExecutor::run(pwbench::Md5Cuda{}, target, source, {},
                                                           {.batch_size = batch_size});
            EXPECT_EQ(result.reason, pwbench::StopReason::found) << result.error;
            ASSERT_TRUE(result.candidate);
            EXPECT_TRUE(cpu.matches(cpu.hash(fixtures::view(*result.candidate), target), target));
            EXPECT_LE(result.checks, candidates.size());
        }
    }
}

TEST_F(Md5CudaTest, RunsStreamingWordlistWithByteAndWorkBoundaries) {
    TempDirectory directory;
    const auto path = directory.write("words.txt", "alpha\r\n\n beta \nlast\r");
    const auto password = fixtures::bytes("last\r");
    const auto target = pwbench::Md5{}.make_target(fixtures::view(password));
    pwbench::WordlistStrategy source(path, 2);
    const auto result = pwbench::CudaExecutor::run(pwbench::Md5Cuda{}, target, source, {},
                                                   {.batch_size = 2, .batch_bytes = 3});
    EXPECT_EQ(result.reason, pwbench::StopReason::found) << result.error;
    EXPECT_EQ(result.candidate, password);
    EXPECT_EQ(result.checks, 4U);
}

}
