#ifndef PWBENCH_MODULES_PBKDF2_SHA256_HPP
#define PWBENCH_MODULES_PBKDF2_SHA256_HPP

#include "pwbench/algorithms/pbkdf2_sha256.impl.hpp"
#include "pwbench/cpu_module.hpp"

namespace pwbench {

struct Pbkdf2Sha256Module {
    using algorithm_type = Pbkdf2Sha256;
    static constexpr std::string_view id = "pbkdf2-sha256";
    static constexpr std::size_t poll_interval = 1;

    static std::vector<OptionSpec> options();
    static PreparedAlgorithm<Pbkdf2Sha256> prepare(CandidateView password,
                                                   const Parameters& parameters);
};

}

#endif
