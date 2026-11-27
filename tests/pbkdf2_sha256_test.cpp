#include "pwbench/algorithms/pbkdf2_sha256.hpp"
#include "pwbench/contracts.hpp"
#include "pwbench/cpu_executor.hpp"
#include "pwbench/strategies/wordlist.hpp"

#include "fixtures.hpp"
#include "temp_directory.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <string_view>

namespace {

using pwbench::Pbkdf2Sha256;

static_assert(pwbench::CpuAlgorithm<Pbkdf2Sha256>);

constexpr Pbkdf2Sha256::salt_type salt{'p', 'w', 'b', 'e', 'n', 'c', 'h', '-',
                                       'p', 'b', 'k', 'd', 'f', '2', '!', '!'};

pwbench::CandidateView view(std::string_view text) {
    return {reinterpret_cast<const pwbench::Byte*>(text.data()), text.size()};
}

TEST(Pbkdf2Sha256Test, MatchesIndependentKnownVectors) {
    const Pbkdf2Sha256 algorithm;
    for (const auto& [iterations, expected] :
         {std::pair{1U, "02aead9f81b9f6ce1beeb068023bd14e432c742753f917a7fd66c812d2cdaa09"},
          std::pair{2U, "d5fa144b262585c0185bbaebab8b1c109d2bace13a9370c99dd11e220d45cbd6"},
          std::pair{100000U, "4def41202a1de48d497326cdacfbaf6b221ebb98a8b710a933aa8257d0a128b9"}}) {
        const auto target = algorithm.make_target(view("password"), salt, iterations);
        EXPECT_EQ(Pbkdf2Sha256::hex(target.digest.data(), target.digest.size()), expected);
        EXPECT_TRUE(algorithm.matches(algorithm.hash(view("password"), target), target));
        EXPECT_FALSE(algorithm.matches(algorithm.hash(view("other"), target), target));
    }
}

TEST(Pbkdf2Sha256Test, HandlesEmptyAndBinaryPasswords) {
    const Pbkdf2Sha256 algorithm;
    const auto empty = algorithm.make_target({}, salt, 2);
    EXPECT_EQ(Pbkdf2Sha256::hex(empty.digest.data(), empty.digest.size()),
              "e3cff9c8a11565f1661a9054593796792964a5b000a99a7e0125aa661fb08c35");
    const pwbench::Byte password[]{'a', 0, 'b', 0xff};
    const auto binary = algorithm.make_target({password, sizeof(password)}, salt, 17);
    EXPECT_EQ(Pbkdf2Sha256::hex(binary.digest.data(), binary.digest.size()),
              "31c275b029b3ac3b0c07e27f5494208847481d6a0b194d0fa30e585d4f40567f");
}

TEST(Pbkdf2Sha256Test, RejectsInvalidInputs) {
    const Pbkdf2Sha256 algorithm;
    EXPECT_THROW(algorithm.make_target({nullptr, 1}, salt, 1), std::invalid_argument);
    EXPECT_THROW(algorithm.make_target({}, salt, 0), std::invalid_argument);
    EXPECT_THROW(algorithm.make_target({}, salt, std::numeric_limits<std::uint32_t>::max()),
                 std::invalid_argument);
}

TEST(Pbkdf2Sha256Test, FindsWordThroughSharedExecutor) {
    TempDirectory directory;
    const auto path = directory.write("words.txt", "alpha\nbeta\ngamma\n");
    const Pbkdf2Sha256 algorithm;
    const auto target = algorithm.make_target(view("beta"), salt, 1000);
    pwbench::WordlistStrategy strategy(path, 2);
    const auto result = pwbench::CpuExecutor::run(algorithm, target, strategy, {},
                                                  {.threads = 1, .poll_interval = 1});
    EXPECT_EQ(result.reason, pwbench::StopReason::found);
    EXPECT_EQ(result.candidate, fixtures::bytes("beta"));
    EXPECT_EQ(result.checks, 2);
}

}
