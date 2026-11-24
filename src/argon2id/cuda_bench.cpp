#include "pwbench/algorithms/argon2id.cuh"
#include "pwbench/parameters.hpp"

#include <chrono>
#include <iostream>

namespace {

void print_usage() {
    std::cout
        << "Usage: argon2id_cuda_bench [--memory-kib N] [--iterations N] "
           "[--lanes N] [--device N]\n"
           "Hashes a fixed test input with Argon2id on CPU and CUDA and compares the digests.\n"
           "Defaults: --memory-kib 65536 --iterations 3 --lanes 1 --device 0\n";
}

int parse_device(const std::string& value) {
    int device{};
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), device);
    if (error != std::errc{} || end != value.data() + value.size() || device < 0) {
        throw std::invalid_argument("--device must be a nonnegative integer in range");
    }
    return device;
}

}

int main(int argc, char** argv) {
    using namespace pwbench;
    using Clock = std::chrono::steady_clock;

    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        print_usage();
        return 0;
    }
    try {
        constexpr Argon2idParameters defaults;
        Parameters options{{"memory-kib", std::to_string(defaults.memory_kib)},
                           {"iterations", std::to_string(defaults.iterations)},
                           {"lanes", std::to_string(defaults.lanes)},
                           {"device", "0"}};
        for (int index = 1; index < argc; index += 2) {
            const std::string_view argument(argv[index]);
            if (!argument.starts_with("--") || !options.contains(argument.substr(2))) {
                throw std::invalid_argument("unknown option: " + std::string(argument));
            }
            if (index + 1 == argc) {
                throw std::invalid_argument("missing value for " + std::string(argument));
            }
            options.at(std::string(argument.substr(2))) = argv[index + 1];
        }

        const Argon2idParameters costs{positive_parameter<std::uint32_t>(options, "memory-kib"),
                                       positive_parameter<std::uint32_t>(options, "iterations"),
                                       positive_parameter<std::uint32_t>(options, "lanes")};
        const auto device = parse_device(parameter(options, "device"));
        constexpr Byte input[]{'b', 'e', 'n', 'c', 'h', '-', 'f', 'i', 'x', 't', 'u', 'r', 'e'};
        constexpr Argon2id::salt_type salt{'p', 'w', 'b', 'e', 'n', 'c', 'h', '-',
                                           'a', 'r', 'g', 'o', 'n', '2', 'i', 'd'};

        const auto cuda_start = Clock::now();
        const auto gpu_digest = argon2id_cuda_hash(input, salt, costs, device);
        const auto cuda_time = Clock::now() - cuda_start;
        const auto cpu_start = Clock::now();
        const auto cpu_digest = Argon2id{}.hash({input, sizeof(input)}, {{}, salt, costs});
        const auto cpu_time = Clock::now() - cpu_start;
        if (gpu_digest != cpu_digest) {
            std::cerr << "Argon2id CPU and CUDA digests differ\n";
            return 1;
        }
        std::cout << "match=true\n"
                  << "cpu_ns="
                  << std::chrono::duration_cast<std::chrono::nanoseconds>(cpu_time).count()
                  << "\ncuda_ns="
                  << std::chrono::duration_cast<std::chrono::nanoseconds>(cuda_time).count()
                  << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
