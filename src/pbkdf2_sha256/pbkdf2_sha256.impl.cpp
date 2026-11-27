#include "pwbench/algorithms/pbkdf2_sha256.impl.hpp"

#include <openssl/evp.h>

#include <limits>
#include <stdexcept>

namespace pwbench {
namespace {

Pbkdf2Sha256::digest_type derive(CandidateView candidate, const Pbkdf2Sha256::salt_type& salt,
                                 std::uint32_t iterations) {
    if (candidate.size != 0 && candidate.data == nullptr) {
        throw std::invalid_argument("nonempty PBKDF2 input has null data");
    }
    if (candidate.size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("PBKDF2 input is too long");
    }
    if (iterations == 0 ||
        iterations > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("PBKDF2 iterations must be in range 1..INT_MAX");
    }
    Pbkdf2Sha256::digest_type digest{};
    const char* password = candidate.size == 0 ? "" : reinterpret_cast<const char*>(candidate.data);
    if (PKCS5_PBKDF2_HMAC(password, static_cast<int>(candidate.size), salt.data(),
                          static_cast<int>(salt.size()), static_cast<int>(iterations), EVP_sha256(),
                          static_cast<int>(digest.size()), digest.data()) != 1) {
        throw std::runtime_error("PBKDF2-HMAC-SHA256 derivation failed");
    }
    return digest;
}

}

Pbkdf2Sha256::digest_type Pbkdf2Sha256::hash(CandidateView candidate,
                                             const target_type& target) const {
    return derive(candidate, target.salt, target.iterations);
}

Pbkdf2Sha256::target_type Pbkdf2Sha256::make_target(CandidateView candidate, const salt_type& salt,
                                                    std::uint32_t iterations) const {
    return {derive(candidate, salt, iterations), salt, iterations};
}

std::string Pbkdf2Sha256::hex(const Byte* bytes, std::size_t size) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result(size * 2, '0');
    for (std::size_t index = 0; index < size; ++index) {
        result[2 * index] = digits[bytes[index] >> 4];
        result[2 * index + 1] = digits[bytes[index] & 0x0f];
    }
    return result;
}

}
