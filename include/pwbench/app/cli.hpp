#ifndef PWBENCH_APP_CLI_HPP
#define PWBENCH_APP_CLI_HPP

#include "pwbench/registry.hpp"

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>

namespace pwbench::app {

struct Options {
    std::string password;
    std::filesystem::path wordlist;
    std::size_t threads{1};
    std::string algorithm{"md5"};
    std::optional<std::uint32_t> memory_kib;
    std::optional<std::uint32_t> iterations;
    std::optional<std::uint32_t> lanes;
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
