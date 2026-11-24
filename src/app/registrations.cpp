#include "registrations.hpp"

#include "pwbench/cpu_module.hpp"
#include "pwbench/modules/argon2id.hpp"
#include "pwbench/modules/md5.hpp"
#include "pwbench/modules/wordlist.hpp"

#ifdef PWBENCH_ENABLE_CUDA
#include "pwbench/modules/md5.cuh"
#endif

namespace pwbench::app {

Registry make_registry() {
    Registry registry;
    register_cpu<Md5Module, WordlistModule>(registry);
    register_cpu<Argon2idModule, WordlistModule>(registry);
#ifdef PWBENCH_ENABLE_CUDA
    register_cuda<Md5CudaModule, WordlistModule>(registry);
#endif
    return registry;
}

}
