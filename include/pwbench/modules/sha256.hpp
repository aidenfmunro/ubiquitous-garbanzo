#ifndef PWBENCH_MODULES_SHA256_HPP
#define PWBENCH_MODULES_SHA256_HPP

#include "pwbench/algorithms/sha256.impl.hpp"
#include "pwbench/cpu_module.hpp"

namespace pwbench {

struct Sha256Module {
    using algorithm_type = Sha256;
    static constexpr std::string_view id = "sha256";
    static constexpr std::size_t poll_interval = 256;

    static std::vector<OptionSpec> options();
    static PreparedAlgorithm<Sha256> prepare(CandidateView password, const Parameters& parameters);
};

}

#endif
