#include "pwbench/app/cli.hpp"
#include "registrations.hpp"

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        const auto registry = pwbench::app::make_registry();
        return pwbench::app::run(argc, argv, registry, std::cout, std::cerr);
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
    } catch (...) {
        std::cerr << "error: unknown registration error\n";
    }
    return 2;
}
