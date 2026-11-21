#include "pwbench/algorithms/argon2id.hpp"
#include "pwbench/contracts.hpp"

#include "test_support.hpp"

namespace {

using pwbench::Argon2id;

static_assert(pwbench::CpuAlgorithm<Argon2id>);

constexpr Argon2id::salt_type salt{'p', 'w', 'b', 'e', 'n', 'c', 'h', '-',
                                   'a', 'r', 'g', 'o', 'n', '2', 'i', 'd'};

void matches_known_vector_with_binary_password() {
    const Argon2id algorithm;
    const pwbench::Byte password[]{0x61, 0x00, 0x62, 0xff};
    const pwbench::CandidateView input{password, sizeof(password)};
    const auto prepared =
        algorithm.make_target(input, salt, {.memory_kib = 32, .iterations = 2, .lanes = 2});
    // Checked independently using the OpenSSL ARGON2ID KDF.
    CHECK(prepared.encoded == "$argon2id$v=19$m=32,t=2,p=2$cHdiZW5jaC1hcmdvbjJpZA"
                              "$WKWh3r4a2MPyqUBaUdM2uzNpeTbE/bj6fEpxvKsWMDY");
    CHECK(algorithm.matches(algorithm.hash(input, prepared.target), prepared.target));

    const pwbench::Byte different_password[]{0x61, 0x00, 0x62, 0xfe};
    const pwbench::CandidateView different{different_password, sizeof(different_password)};
    CHECK(!algorithm.matches(algorithm.hash(different, prepared.target), prepared.target));
}

void handles_empty_password_and_validates_costs() {
    const Argon2id algorithm;
    const auto prepared = algorithm.make_target({}, salt, {.memory_kib = 8, .iterations = 1});
    CHECK(algorithm.matches(algorithm.hash({}, prepared.target), prepared.target));

    check_throws<std::invalid_argument>([&] { algorithm.hash({nullptr, 1}, prepared.target); });
    check_throws<std::runtime_error>(
        [&] { algorithm.make_target({}, salt, {.memory_kib = 8, .iterations = 1, .lanes = 2}); });
}

}

int main() {
    return run_tests({
        {"known Argon2id vector with binary input", matches_known_vector_with_binary_password},
        {"empty Argon2id input and invalid costs", handles_empty_password_and_validates_costs},
    });
}
