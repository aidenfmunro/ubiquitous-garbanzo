#include "pwbench/algorithms/sha256.cuh"
#include "pwbench/cuda_executor.hpp"
#include "pwbench/strategies/wordlist.hpp"

#include "cuda_test_support.hpp"
#include "fixtures.hpp"
#include "temp_directory.hpp"

#include <charconv>
#include <random>

namespace {

using Sha256CudaTest = fixtures::CudaTest;

pwbench::Sha256::target_type known_target(std::string_view hex) {
    pwbench::Sha256::target_type target;
    if (hex.size() != 64) {
        throw std::invalid_argument("invalid test digest length");
    }
    for (std::size_t index = 0; index < 32; ++index) {
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
        pwbench::CudaExecutor::run(pwbench::Sha256Cuda{}, known_target(expected), source);
    EXPECT_EQ(result.reason, pwbench::StopReason::found) << result.error;
    EXPECT_EQ(result.candidate, candidate);
    EXPECT_EQ(result.checks, 2U);
}

TEST_F(Sha256CudaTest, MatchesKnownAnswerVectors) {
    const std::pair<std::string_view, std::string_view> vectors[]{
        {"", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
        {"a", "ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb"},
        {"abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
        {"message digest", "f7846f55cf23e14eebeab5b4e1550cad5b509e3348fbc4efa3a1413d393cb650"},
        {"abcdefghijklmnopqrstuvwxyz",
         "71c480df93d6ae2f1efad1447c66c9525e316218cf51fc8d9ed832f2daf18b73"},
        {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
         "db4bfcbd4da0cd85a60c3c37d3fbd8805c77f15fc6b1fdfe614ee0a7c8fdb4c0"},
        {"12345678901234567890123456789012345678901234567890123456789012345678901234567890",
         "f371bc4a311f2b009eef952dd83ca80e2b60026c8e935592d0f9c308453c813e"},
    };
    for (const auto& [input, hash] : vectors) {
        SCOPED_TRACE(input);
        expects_hash(fixtures::bytes(input), hash);
    }
}

TEST_F(Sha256CudaTest, HandlesPaddingBoundariesAndLargeInputs) {
    const std::pair<std::size_t, std::string_view> vectors[]{
        {55, "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318"},
        {56, "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"},
        {63, "7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34"},
        {64, "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb"},
        {65, "635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0"},
        {119, "31eba51c313a5c08226adf18d4a359cfdfd8d2e816b13f4af952f7ea6584dcfb"},
        {120, "2f3d335432c70b580af0e8e1b3674a7c020d683aa5f73aaaedfdc55af904c21c"},
        {127, "c57e9278af78fa3cab38667bef4ce29d783787a2f731d4e12200270f0c32320a"},
        {128, "6836cf13bac400e9105071cd6af47084dfacad4e5e302c94bfed24e013afb73e"},
        {1'000'000, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"},
    };
    for (const auto& [size, hash] : vectors) {
        SCOPED_TRACE(size);
        expects_hash(pwbench::Candidate(size, 'a'), hash);
    }
}

TEST_F(Sha256CudaTest, HandlesBinaryAndUnalignedDeviceInputs) {
    pwbench::Candidate candidate(256);
    for (std::size_t index = 0; index < candidate.size(); ++index) {
        candidate[index] = static_cast<pwbench::Byte>(index);
    }
    fixtures::BlockStrategy source({{0}, candidate, {}}, 1);
    const auto result = pwbench::CudaExecutor::run(
        pwbench::Sha256Cuda{},
        known_target("40aff2e9d2d8922e47afd4648e6967497158785fbd1da870e7110266bf944880"), source);
    EXPECT_EQ(result.reason, pwbench::StopReason::found) << result.error;
    EXPECT_EQ(result.candidate, candidate);
    EXPECT_EQ(result.checks, 3U);
}

TEST_F(Sha256CudaTest, ChecksEveryDigestWord) {
    const auto candidate = fixtures::bytes("expected");
    for (std::size_t byte = 0; byte < 32; ++byte) {
        auto target = pwbench::Sha256{}.make_target(fixtures::view(candidate));
        target.digest[byte] ^= 1;
        fixtures::BlockStrategy source({candidate});
        const auto result = pwbench::CudaExecutor::run(pwbench::Sha256Cuda{}, target, source);
        EXPECT_EQ(result.reason, pwbench::StopReason::exhausted) << result.error;
        EXPECT_EQ(result.checks, 1U);
        EXPECT_FALSE(result.candidate);
    }
}

TEST_F(Sha256CudaTest, MatchesCpuForRandomBinaryInputsAndDifferentBatchSizes) {
    std::mt19937 random(0x256a);
    std::vector<pwbench::Candidate> candidates;
    for (std::size_t index = 0; index < 301; ++index) {
        auto& candidate = candidates.emplace_back(random() % 257);
        for (auto& byte : candidate) {
            byte = static_cast<pwbench::Byte>(random());
        }
    }
    const pwbench::Sha256 cpu;
    for (const auto selected : {0U, 1U, 54U, 55U, 63U, 127U, 255U, 300U}) {
        const auto target = cpu.make_target(fixtures::view(candidates[selected]));
        for (const auto batch_size : {1U, 7U, 256U, 512U}) {
            fixtures::BlockStrategy source(candidates, 11);
            const auto result = pwbench::CudaExecutor::run(pwbench::Sha256Cuda{}, target, source,
                                                           {}, {.batch_size = batch_size});
            EXPECT_EQ(result.reason, pwbench::StopReason::found) << result.error;
            ASSERT_TRUE(result.candidate);
            EXPECT_TRUE(cpu.matches(cpu.hash(fixtures::view(*result.candidate), target), target));
            EXPECT_LE(result.checks, candidates.size());
        }
    }
}

TEST_F(Sha256CudaTest, RunsStreamingWordlistWithByteAndWorkBoundaries) {
    TempDirectory directory;
    const auto path = directory.write("words.txt", "alpha\r\n\n beta \nlast\r");
    const auto password = fixtures::bytes("last\r");
    const auto target = pwbench::Sha256{}.make_target(fixtures::view(password));
    pwbench::WordlistStrategy source(path, 2);
    const auto result = pwbench::CudaExecutor::run(pwbench::Sha256Cuda{}, target, source, {},
                                                   {.batch_size = 2, .batch_bytes = 3});
    EXPECT_EQ(result.reason, pwbench::StopReason::found) << result.error;
    EXPECT_EQ(result.candidate, password);
    EXPECT_EQ(result.checks, 4U);
}

}
