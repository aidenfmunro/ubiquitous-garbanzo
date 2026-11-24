#ifndef PWBENCH_ALGORITHMS_ARGON2ID_CUH
#define PWBENCH_ALGORITHMS_ARGON2ID_CUH

#include "pwbench/algorithms/argon2id.hpp"

#include <span>

namespace pwbench {

Argon2id::digest_type argon2id_cuda_hash(std::span<const Byte> input,
                                         const Argon2id::salt_type& salt,
                                         Argon2idParameters costs = {}, int device = 0);

}

#endif
