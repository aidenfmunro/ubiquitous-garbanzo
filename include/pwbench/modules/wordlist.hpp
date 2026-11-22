#ifndef PWBENCH_MODULES_WORDLIST_HPP
#define PWBENCH_MODULES_WORDLIST_HPP

#include "pwbench/registry.hpp"
#include "pwbench/strategies/wordlist.hpp"

namespace pwbench {

struct WordlistModule {
    using strategy_type = WordlistStrategy;
    using config_type = std::filesystem::path;
    static constexpr std::string_view id = "wordlist";

    static std::vector<OptionSpec> options();
    static config_type parse(const Parameters& parameters);
    static WordlistStrategy create(const config_type& config);
};

}

#endif
