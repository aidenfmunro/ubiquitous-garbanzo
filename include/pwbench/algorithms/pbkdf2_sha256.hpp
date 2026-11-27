#ifndef PWBENCH_ALGORITHMS_PBKDF2_SHA256_HPP
#define PWBENCH_ALGORITHMS_PBKDF2_SHA256_HPP

#include "pwbench/types.hpp"

#include <array>
#include <cstdint>

namespace pwbench {

class Pbkdf2Sha256 {
public:
    using digest_type = std::array<Byte, 32>;
    using salt_type = std::array<Byte, 16>;

    struct target_type {
        digest_type digest;
        salt_type salt;
        std::uint32_t iterations;
    };

    digest_type hash(CandidateView candidate, const target_type& target) const;
    target_type make_target(CandidateView candidate, const salt_type& salt,
                            std::uint32_t iterations) const;
    static std::string hex(const Byte* bytes, std::size_t size);

    bool matches(const digest_type& digest, const target_type& target) const {
        return digest == target.digest;
    }
};

}

#endif
