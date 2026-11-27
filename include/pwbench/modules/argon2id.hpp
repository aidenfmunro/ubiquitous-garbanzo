#ifndef PWBENCH_MODULES_ARGON2ID_HPP
#define PWBENCH_MODULES_ARGON2ID_HPP

#include "pwbench/algorithms/argon2id.impl.hpp"
#include "pwbench/cpu_module.hpp"

namespace pwbench {

struct Argon2idModule {
    using algorithm_type = Argon2id;
    static constexpr std::string_view id = "argon2id";
    static constexpr std::size_t poll_interval = 1;

    static std::vector<OptionSpec> options();
    static PreparedAlgorithm<Argon2id> prepare(CandidateView password,
                                               const Parameters& parameters);
};

}

#endif
