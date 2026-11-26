#include "pwbench/algorithms/sha256.hpp"
#include "pwbench/cpu_executor.hpp"
#include "pwbench/strategies/wordlist.hpp"

#include "fixtures.hpp"
#include "temp_directory.hpp"

#include <gtest/gtest.h>

#include <array>
#include <string_view>

namespace {

static_assert(pwbench::CpuAlgorithm<pwbench::Sha256>);

std::string digest(std::string_view input) {
    const pwbench::Sha256 algorithm;
    const pwbench::CandidateView candidate{reinterpret_cast<const pwbench::Byte*>(input.data()),
                                           input.size()};
    return pwbench::Sha256::hex(algorithm.make_target(candidate).digest);
}

TEST(Sha256Test, MatchesKnownAnswerVectors) {
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
    for (const auto& [input, expected] : vectors) {
        EXPECT_EQ(digest(input), expected) << "Input: " << input;
    }
}

TEST(Sha256Test, HandlesPaddingBoundaries) {
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
    };
    for (const auto& [length, expected] : vectors) {
        EXPECT_EQ(digest(std::string(length, 'a')), expected) << "Input length: " << length;
    }
}

TEST(Sha256Test, HashesLargeInput) {
    EXPECT_EQ(digest(std::string(1'000'000, 'a')),
              "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST(Sha256Test, HandlesBinaryAndUnalignedInput) {
    std::array<pwbench::Byte, 257> bytes{};
    for (std::size_t index = 0; index < 256; ++index) {
        bytes[index + 1] = static_cast<pwbench::Byte>(index);
    }
    const pwbench::Sha256 algorithm;
    const pwbench::CandidateView input{bytes.data() + 1, 256};
    const auto target = algorithm.make_target(input);
    EXPECT_EQ(pwbench::Sha256::hex(target.digest),
              "40aff2e9d2d8922e47afd4648e6967497158785fbd1da870e7110266bf944880");
    EXPECT_TRUE(algorithm.matches(algorithm.hash(input, target), target));
    auto wrong = target;
    wrong.digest[0] ^= 1;
    EXPECT_FALSE(algorithm.matches(algorithm.hash(input, wrong), wrong));
}

TEST(Sha256Test, ValidatesInputViews) {
    const pwbench::Sha256 algorithm;
    EXPECT_EQ(pwbench::Sha256::hex(algorithm.make_target({nullptr, 0}).digest),
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_THROW(algorithm.make_target({nullptr, 1}), std::invalid_argument);
    EXPECT_THROW(algorithm.hash({nullptr, 1}, {}), std::invalid_argument);
}

TEST(Sha256Test, FindsAWordThroughTheSharedPipeline) {
    TempDirectory directory;
    const auto path = directory.write("words.txt", "alpha\nbeta\ngamma\n");
    const pwbench::Sha256 algorithm;
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

TEST(Sha256Test, ReportsAnAbsentPassword) {
    TempDirectory directory;
    const auto path = directory.write("words.txt", "alpha\nbeta\ngamma\n");
    const pwbench::Sha256 algorithm;
    const auto password = fixtures::bytes("absent");
    const auto target = algorithm.make_target(fixtures::view(password));
    pwbench::WordlistStrategy strategy(path, 2);
    const auto result = pwbench::CpuExecutor::run(algorithm, target, strategy, {}, {.threads = 4});
    EXPECT_EQ(result.reason, pwbench::StopReason::exhausted);
    EXPECT_EQ(result.checks, 3);
    EXPECT_FALSE(result.candidate);
}

}
