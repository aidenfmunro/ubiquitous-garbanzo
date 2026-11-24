#include "pwbench/cuda_module.hpp"

namespace pwbench::detail {

CudaOptions parse_cuda_options(const Parameters& parameters) {
    const auto& text = parameter(parameters, "device");
    int device{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), device);
    if (error != std::errc{} || end != text.data() + text.size() || device < 0) {
        throw std::invalid_argument("--device must be a nonnegative integer in range");
    }
    return {device, positive_parameter<std::size_t>(parameters, "batch-size"),
            positive_parameter<std::size_t>(parameters, "batch-bytes")};
}

std::vector<OptionSpec> cuda_option_specs() {
    constexpr CudaOptions defaults;
    return {{"device", "N", "CUDA device index", std::to_string(defaults.device)},
            {"batch-size", "N", "Maximum candidates per CUDA batch",
             std::to_string(defaults.batch_size)},
            {"batch-bytes", "N", "CUDA batch byte budget; an oversized candidate runs alone",
             std::to_string(defaults.batch_bytes)}};
}

}
