#include "pwbench/modules/wordlist.hpp"

namespace pwbench {

std::vector<OptionSpec> WordlistModule::options() {
    return {{"wordlist", "PATH", "Dictionary file (downloaded automatically during the build)",
             PWBENCH_DEFAULT_WORDLIST}};
}

WordlistModule::config_type WordlistModule::parse(const Parameters& parameters) {
    return parameter(parameters, "wordlist");
}

WordlistStrategy WordlistModule::create(const config_type& config) {
    return WordlistStrategy(config);
}

}
