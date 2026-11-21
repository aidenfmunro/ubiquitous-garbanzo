#include "pwbench/app/cli.hpp"
#include "md5_experiment.hpp"

#include <iostream>

int main(int argc, char** argv) {
    return pwbench::app::run(argc, argv, pwbench::app::prepare_md5, std::cout, std::cerr);
}
