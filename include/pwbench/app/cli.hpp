#ifndef PWBENCH_APP_CLI_HPP
#define PWBENCH_APP_CLI_HPP

#include "pwbench/registry.hpp"

#include <iosfwd>

namespace pwbench::app {

int run(int argc, char** argv, const Registry& registry, std::ostream& output,
        std::ostream& errors);

}

#endif
