#ifndef PWBENCH_ALGORITHMS_SHA256_HPP
#define PWBENCH_ALGORITHMS_SHA256_HPP

#include "pwbench/types.hpp"

#include <array>

namespace pwbench {

class Sha256 {
public:
    using digest_type = std::array<Byte, 32>;
    struct target_type {
        digest_type digest;
    };

    digest_type hash(CandidateView candidate, const target_type& target) const;
    target_type make_target(CandidateView candidate) const;
    static std::string hex(const digest_type& digest);

    bool matches(const digest_type& digest, const target_type& target) const {
        return digest == target.digest;
    }
};

}

#endif
