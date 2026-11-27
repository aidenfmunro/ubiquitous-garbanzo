#ifndef PWBENCH_MODULES_MD5_HPP
#define PWBENCH_MODULES_MD5_HPP

#include "pwbench/algorithms/md5.impl.hpp"
#include "pwbench/cpu_module.hpp"

namespace pwbench {

struct Md5Module {
    using algorithm_type = Md5;
    static constexpr std::string_view id = "md5";
    static constexpr std::size_t poll_interval = 256;

    static std::vector<OptionSpec> options();
    static PreparedAlgorithm<Md5> prepare(CandidateView password, const Parameters& parameters);
};

}

#endif
