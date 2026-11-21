#include "pwbench/algorithms/md5.hpp"
#include "pwbench/cpu_executor.hpp"
#include "pwbench/strategies/wordlist.hpp"

#include "fixtures.hpp"
#include "temp_directory.hpp"
#include "test_support.hpp"

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

void matches_rfc_1321_vectors() {
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
        CHECK(digest(input) == expected);
    }
}

void handles_padding_boundaries() {
    const std::pair<std::size_t, std::string_view> vectors[]{
        {55, "ef1772b6dff9a122358552954ad0df65"},  {56, "3b0c8ac703f828b04c6c197006d17218"},
        {63, "b06521f39153d618550606be297466d5"},  {64, "014842d480b571495a4a0363793f7367"},
        {65, "c743a45e0d2e6a95cb859adae0248435"},  {119, "8a7bd0732ed6a28ce75f6dabc90e1613"},
        {120, "5f61c0ccad4cac44c75ff505e1f1e537"}, {127, "020406e1d05cdc2aa287641f7ae2cc39"},
        {128, "e510683b3f5ffe4093d021808bc6ff70"},
    };
    for (const auto& [length, expected] : vectors) {
        CHECK(digest(std::string(length, 'a')) == expected);
    }
}

void hashes_large_input() {
    CHECK(digest(std::string(1'000'000, 'a')) == "7707d6ae4e027c70eea2a935c2296f21");
}

void handles_binary_and_unaligned_input() {
    std::array<pwbench::Byte, 257> bytes{};
    for (std::size_t index = 0; index < 256; ++index) {
        bytes[index + 1] = static_cast<pwbench::Byte>(index);
    }
    const pwbench::Md5 algorithm;
    const pwbench::CandidateView input{bytes.data() + 1, 256};
    const auto target = algorithm.make_target(input);
    CHECK(pwbench::Md5::hex(target.digest) == "e2c865db4162bed963bfaa9ef6ac18f0");
    CHECK(algorithm.matches(algorithm.hash(input, target), target));
    auto wrong = target;
    wrong.digest[0] ^= 1;
    CHECK(!algorithm.matches(algorithm.hash(input, wrong), wrong));
}

void validates_input_views() {
    const pwbench::Md5 algorithm;
    CHECK(pwbench::Md5::hex(algorithm.make_target({nullptr, 0}).digest) ==
          "d41d8cd98f00b204e9800998ecf8427e");
    check_throws<std::invalid_argument>([&] { algorithm.make_target({nullptr, 1}); });
    check_throws<std::invalid_argument>([&] { algorithm.hash({nullptr, 1}, {}); });
}

void finds_a_word_through_the_shared_pipeline() {
    TempDirectory directory;
    const auto path = directory.write("words.txt", "alpha\nbeta\ngamma\n");
    const pwbench::Md5 algorithm;
    const auto password = fixtures::bytes("beta");
    const auto target = algorithm.make_target(fixtures::view(password));
    for (const auto threads : {std::size_t{1}, std::size_t{4}}) {
        pwbench::WordlistStrategy strategy(path, 2);
        const auto result =
            pwbench::CpuExecutor::run(algorithm, target, strategy, {}, {.threads = threads});
        CHECK(result.reason == pwbench::StopReason::found);
        CHECK(result.candidate == password);
        CHECK(result.checks >= 1 && result.checks <= 3);
        CHECK(algorithm.matches(algorithm.hash(fixtures::view(*result.candidate), target), target));
    }
}

void reports_an_absent_password() {
    TempDirectory directory;
    const auto path = directory.write("words.txt", "alpha\nbeta\ngamma\n");
    const pwbench::Md5 algorithm;
    const auto password = fixtures::bytes("absent");
    const auto target = algorithm.make_target(fixtures::view(password));
    pwbench::WordlistStrategy strategy(path, 2);
    const auto result = pwbench::CpuExecutor::run(algorithm, target, strategy, {}, {.threads = 4});
    CHECK(result.reason == pwbench::StopReason::exhausted);
    CHECK(result.checks == 3);
    CHECK(!result.candidate);
}

}

int main() {
    return run_tests({
        {"RFC 1321 vectors", matches_rfc_1321_vectors},
        {"MD5 padding boundaries", handles_padding_boundaries},
        {"million-byte input", hashes_large_input},
        {"binary and unaligned input", handles_binary_and_unaligned_input},
        {"MD5 input validation", validates_input_views},
        {"MD5 wordlist pipeline", finds_a_word_through_the_shared_pipeline},
        {"absent MD5 password", reports_an_absent_password},
    });
}
