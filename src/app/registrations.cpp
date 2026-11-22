#include "registrations.hpp"

#include "pwbench/cpu_module.hpp"
#include "pwbench/modules/argon2id.hpp"
#include "pwbench/modules/md5.hpp"
#include "pwbench/modules/wordlist.hpp"

namespace pwbench::app {

Registry make_registry() {
    Registry registry;
    register_cpu<Md5Module, WordlistModule>(registry);
    register_cpu<Argon2idModule, WordlistModule>(registry);
    return registry;
}

}
