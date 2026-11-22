#include "pwbench/modules/md5.hpp"

namespace pwbench {

std::vector<OptionSpec> Md5Module::options() {
    return {};
}

PreparedAlgorithm<Md5> Md5Module::prepare(CandidateView password, const Parameters&) {
    const Md5 algorithm;
    const auto target = algorithm.make_target(password);
    return {algorithm, target, Md5::hex(target.digest)};
}

}
