#include "pwbench/algorithms/argon2id.hpp"

#include <argon2.h>

#include <span>
#include <stdexcept>
#include <utility>

namespace pwbench {
namespace {

Argon2id::digest_type calculate_digest(CandidateView candidate, const Argon2id::salt_type& salt,
                                       const Argon2idParameters& parameters,
                                       std::span<char> encoded = {}) {
    if (candidate.size != 0 && candidate.data == nullptr) {
        throw std::invalid_argument("nonempty Argon2id input has null data");
    }
    Argon2id::digest_type digest;
    const int status =
        argon2_hash(parameters.iterations, parameters.memory_kib, parameters.lanes, candidate.data,
                    candidate.size, salt.data(), salt.size(), digest.data(), digest.size(),
                    encoded.data(), encoded.size(), Argon2_id, ARGON2_VERSION_13);
    if (status != ARGON2_OK) {
        throw std::runtime_error(std::string("Argon2id: ") + argon2_error_message(status));
    }
    return digest;
}

}

Argon2id::PreparedTarget Argon2id::make_target(CandidateView candidate, const salt_type& salt,
                                               Argon2idParameters parameters) const {
    const auto encoded_size =
        argon2_encodedlen(parameters.iterations, parameters.memory_kib, parameters.lanes,
                          salt.size(), digest_type{}.size(), Argon2_id);
    std::string encoded(encoded_size, '\0');
    const auto digest = calculate_digest(candidate, salt, parameters, encoded);
    encoded.pop_back(); // argon2_encodedlen includes the NUL terminator.
    return {{digest, salt, parameters}, std::move(encoded)};
}

Argon2id::digest_type Argon2id::hash(CandidateView candidate, const target_type& target) const {
    return calculate_digest(candidate, target.salt, target.parameters);
}

}
