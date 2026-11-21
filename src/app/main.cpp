#include "pwbench/app/cli.hpp"
#include "md5_experiment.hpp"

#ifdef PWBENCH_ENABLE_ARGON2ID
#include "argon2id_experiment.hpp"
#endif

#include <iostream>
#include <stdexcept>

namespace {

pwbench::app::PreparedExperiment prepare(const pwbench::app::Options& options) {
    if (options.algorithm == "md5") {
        return pwbench::app::prepare_md5(options);
    }
    if (options.algorithm == "argon2id") {
#ifdef PWBENCH_ENABLE_ARGON2ID
        return pwbench::app::prepare_argon2id(options);
#else
        throw std::invalid_argument("Argon2id support is disabled. Configure with "
                                    "-DPWBENCH_ENABLE_ARGON2ID=ON.");
#endif
    }
    throw std::invalid_argument("unknown algorithm: " + options.algorithm);
}

}

int main(int argc, char** argv) {
    return pwbench::app::run(argc, argv, prepare, std::cout, std::cerr);
}
