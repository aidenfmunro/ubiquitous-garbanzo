#include "pwbench/modules/pbkdf2_sha256.hpp"

#ifdef PWBENCH_ENABLE_CUDA
#include "pwbench/modules/pbkdf2_sha256.cuh"
#endif

#include <utility>

namespace pwbench {

std::vector<OptionSpec> Pbkdf2Sha256Module::options() {
    return {{"iterations", "N", "PBKDF2-HMAC-SHA256 iterations", "100000"}};
}

PreparedAlgorithm<Pbkdf2Sha256> Pbkdf2Sha256Module::prepare(CandidateView password,
                                                            const Parameters& parameters) {
    constexpr Pbkdf2Sha256::salt_type salt{'p', 'w', 'b', 'e', 'n', 'c', 'h', '-',
                                           'p', 'b', 'k', 'd', 'f', '2', '!', '!'};
    const auto iterations = positive_parameter<std::uint32_t>(parameters, "iterations");
    const Pbkdf2Sha256 algorithm;
    auto target = algorithm.make_target(password, salt, iterations);
    const auto encoded = "$pbkdf2-sha256$i=" + std::to_string(iterations) + "$" +
                         Pbkdf2Sha256::hex(salt.data(), salt.size()) + "$" +
                         Pbkdf2Sha256::hex(target.digest.data(), target.digest.size());
    return {algorithm, std::move(target), encoded};
}

#ifdef PWBENCH_ENABLE_CUDA
std::vector<OptionSpec> Pbkdf2Sha256CudaModule::options() {
    return Pbkdf2Sha256Module::options();
}

PreparedCudaAlgorithm<Pbkdf2Sha256Cuda>
Pbkdf2Sha256CudaModule::prepare(CandidateView password, const Parameters& parameters) {
    auto prepared = Pbkdf2Sha256Module::prepare(password, parameters);
    return {{}, std::move(prepared.target), std::move(prepared.target_hash)};
}
#endif

}
