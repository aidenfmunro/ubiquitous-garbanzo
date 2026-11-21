#ifndef PWBENCH_ALGORITHMS_ARGON2ID_HPP
#define PWBENCH_ALGORITHMS_ARGON2ID_HPP

#include "pwbench/types.hpp"

#include <array>

namespace pwbench {

struct Argon2idParameters {
    std::uint32_t memory_kib{64 * 1024};
    std::uint32_t iterations{3};
    std::uint32_t lanes{1};
};

class Argon2id {
public:
    using digest_type = std::array<Byte, 32>;
    using salt_type = std::array<Byte, 16>;

    struct target_type {
        digest_type digest;
        salt_type salt;
        Argon2idParameters parameters;
    };

    struct PreparedTarget {
        target_type target;
        std::string encoded;
    };

    PreparedTarget make_target(CandidateView candidate, const salt_type& salt,
                               Argon2idParameters parameters = {}) const;
    digest_type hash(CandidateView candidate, const target_type& target) const;

    bool matches(const digest_type& digest, const target_type& target) const {
        return digest == target.digest;
    }
};

}

#endif
