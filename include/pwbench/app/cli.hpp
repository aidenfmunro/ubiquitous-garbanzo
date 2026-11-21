#ifndef PWBENCH_APP_CLI_HPP
#define PWBENCH_APP_CLI_HPP

#include "pwbench/registry.hpp"

#include <filesystem>
#include <iosfwd>

namespace pwbench::app {

struct Options {
    std::string password;
    std::filesystem::path wordlist;
    std::size_t threads{1};
};

struct PreparedExperiment {
    Selection selection;
    std::string target_hash;
    Runner runner;
};

using PrepareExperiment = PreparedExperiment (*)(const Options&);

int run(int argc, char** argv, PrepareExperiment prepare, std::ostream& output,
        std::ostream& errors);

}

#endif
