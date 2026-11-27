#include "pwbench/algorithms/md5.impl.hpp"
#include "pwbench/cpu_executor.hpp"
#include "pwbench/strategies/wordlist.hpp"

#include "fixtures.hpp"
#include "temp_directory.hpp"

#include <gtest/gtest.h>

#include <array>
#include <string_view>

namespace {

static_assert(pwbench::CpuAlgorithm<pwbench::Md5>);

std::string digest(std::string_view input) {
    const pwbench::Md5 algorithm;
    const pwbench::CandidateView candidate{reinterpret_cast<const pwbench::Byte*>(input.data()),
                                           input.size()};
    return pwbench::Md5::hex(algorithm.make_target(candidate).digest);
}

TEST(Md5Test, MatchesRfc1321Vectors) {
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
    for (const auto& [input, expected] : vectors) {
        EXPECT_EQ(digest(input), expected) << "Input: " << input;
    }
}

TEST(Md5Test, HandlesPaddingBoundaries) {
    const std::pair<std::size_t, std::string_view> vectors[]{
        {55, "ef1772b6dff9a122358552954ad0df65"},  {56, "3b0c8ac703f828b04c6c197006d17218"},
        {63, "b06521f39153d618550606be297466d5"},  {64, "014842d480b571495a4a0363793f7367"},
        {65, "c743a45e0d2e6a95cb859adae0248435"},  {119, "8a7bd0732ed6a28ce75f6dabc90e1613"},
        {120, "5f61c0ccad4cac44c75ff505e1f1e537"}, {127, "020406e1d05cdc2aa287641f7ae2cc39"},
        {128, "e510683b3f5ffe4093d021808bc6ff70"},
    };
    for (const auto& [length, expected] : vectors) {
        EXPECT_EQ(digest(std::string(length, 'a')), expected) << "Input length: " << length;
    }
}

TEST(Md5Test, HashesLargeInput) {
    EXPECT_EQ(digest(std::string(1'000'000, 'a')), "7707d6ae4e027c70eea2a935c2296f21");
}

TEST(Md5Test, HandlesBinaryAndUnalignedInput) {
    std::array<pwbench::Byte, 257> bytes{};
    for (std::size_t index = 0; index < 256; ++index) {
        bytes[index + 1] = static_cast<pwbench::Byte>(index);
    }
    const pwbench::Md5 algorithm;
    const pwbench::CandidateView input{bytes.data() + 1, 256};
    const auto target = algorithm.make_target(input);
    EXPECT_EQ(pwbench::Md5::hex(target.digest), "e2c865db4162bed963bfaa9ef6ac18f0");
    EXPECT_TRUE(algorithm.matches(algorithm.hash(input, target), target));
    auto wrong = target;
    wrong.digest[0] ^= 1;
    EXPECT_FALSE(algorithm.matches(algorithm.hash(input, wrong), wrong));
}

TEST(Md5Test, ValidatesInputViews) {
    const pwbench::Md5 algorithm;
    EXPECT_EQ(pwbench::Md5::hex(algorithm.make_target({nullptr, 0}).digest),
              "d41d8cd98f00b204e9800998ecf8427e");
    EXPECT_THROW(algorithm.make_target({nullptr, 1}), std::invalid_argument);
    EXPECT_THROW(algorithm.hash({nullptr, 1}, {}), std::invalid_argument);
}

TEST(Md5Test, FindsAWordThroughTheSharedPipeline) {
    TempDirectory directory;
    const auto path = directory.write("words.txt", "alpha\nbeta\ngamma\n");
    const pwbench::Md5 algorithm;
    const auto password = fixtures::bytes("beta");
    const auto target = algorithm.make_target(fixtures::view(password));
    for (const auto threads : {std::size_t{1}, std::size_t{4}}) {
        pwbench::WordlistStrategy strategy(path, 2);
        const auto result =
            pwbench::CpuExecutor::run(algorithm, target, strategy, {}, {.threads = threads});
        EXPECT_EQ(result.reason, pwbench::StopReason::found);
        ASSERT_EQ(result.candidate, password);
        EXPECT_GE(result.checks, 1U);
        EXPECT_LE(result.checks, 3U);
        EXPECT_TRUE(
            algorithm.matches(algorithm.hash(fixtures::view(*result.candidate), target), target));
    }
}

TEST(Md5Test, ReportsAnAbsentPassword) {
    TempDirectory directory;
    const auto path = directory.write("words.txt", "alpha\nbeta\ngamma\n");
    const pwbench::Md5 algorithm;
    const auto password = fixtures::bytes("absent");
    const auto target = algorithm.make_target(fixtures::view(password));
    pwbench::WordlistStrategy strategy(path, 2);
    const auto result = pwbench::CpuExecutor::run(algorithm, target, strategy, {}, {.threads = 4});
    EXPECT_EQ(result.reason, pwbench::StopReason::exhausted);
    EXPECT_EQ(result.checks, 3);
    EXPECT_FALSE(result.candidate);
}

}
