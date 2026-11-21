#include "fixtures.hpp"
#include "test_support.hpp"

#include <memory>
#include <type_traits>

namespace {

struct MissingAlgorithm {};
struct WrongAlgorithm {
    using digest_type = int;
    using target_type = int;
    void hash(pwbench::CandidateView, const int&) const;
    bool matches(const int&, const int&) const;
};
struct WrongWork {
    pwbench::Candidate next_candidate();
};
struct MoveOnlyWork {
    std::unique_ptr<int> storage;
    std::optional<pwbench::CandidateView> next_candidate();
};
struct WrongStrategy {
    using work_type = fixtures::CounterStrategy::work_type;
    work_type next_work();
};

static_assert(std::is_trivially_copyable_v<pwbench::CandidateView>);
static_assert(std::is_standard_layout_v<pwbench::CandidateView>);
static_assert(!pwbench::CpuAlgorithm<MissingAlgorithm>);
static_assert(!pwbench::CpuAlgorithm<WrongAlgorithm>);
static_assert(!pwbench::CpuWork<WrongWork>);
static_assert(pwbench::CpuWork<MoveOnlyWork>);
static_assert(!pwbench::CpuStrategy<WrongStrategy>);

void target_is_algorithm_owned() {
    const fixtures::TestAlgorithm algorithm;
    const auto candidate = fixtures::bytes("abc");
    const auto first = algorithm.make_target(fixtures::view(candidate), 5);
    const auto second = algorithm.make_target(fixtures::view(candidate), 11);
    CHECK(first.digest != second.digest);
    CHECK(algorithm.matches(algorithm.hash(fixtures::view(candidate), first), first));
    CHECK(!algorithm.matches(algorithm.hash(fixtures::view(candidate), first), second));
}

void labels_are_stable() {
    CHECK(pwbench::to_string(pwbench::Backend::cpu) == "cpu");
    CHECK(pwbench::to_string(pwbench::Backend::cuda) == "cuda");
    CHECK(pwbench::to_string(pwbench::StopReason::found) == "found");
    CHECK(pwbench::to_string(pwbench::StopReason::error) == "error");
}

}

int main() {
    return run_tests({
        {"algorithm-owned target", target_is_algorithm_owned},
        {"stable labels", labels_are_stable},
    });
}
