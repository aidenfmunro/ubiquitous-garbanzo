#include "pwbench/modules/argon2id.hpp"

#include <utility>

namespace pwbench {

std::vector<OptionSpec> Argon2idModule::options() {
    constexpr Argon2idParameters defaults;
    return {
        {"memory-kib", "N", "Argon2id memory per hash in KiB", std::to_string(defaults.memory_kib)},
        {"iterations", "N", "Argon2id passes", std::to_string(defaults.iterations)},
        {"lanes", "N", "Argon2id lanes", std::to_string(defaults.lanes)}};
}

PreparedAlgorithm<Argon2id> Argon2idModule::prepare(CandidateView password,
                                                    const Parameters& parameters) {
    constexpr Argon2id::salt_type salt{'p', 'w', 'b', 'e', 'n', 'c', 'h', '-',
                                       'a', 'r', 'g', 'o', 'n', '2', 'i', 'd'};
    const Argon2idParameters costs{positive_parameter<std::uint32_t>(parameters, "memory-kib"),
                                   positive_parameter<std::uint32_t>(parameters, "iterations"),
                                   positive_parameter<std::uint32_t>(parameters, "lanes")};
    const Argon2id algorithm;
    auto prepared = algorithm.make_target(password, salt, costs);
    return {algorithm, std::move(prepared.target), std::move(prepared.encoded)};
}

}
