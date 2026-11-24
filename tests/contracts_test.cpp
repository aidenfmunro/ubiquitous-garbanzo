#include "fixtures.hpp"

#include <gtest/gtest.h>

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
static_assert(!pwbench::HostWork<WrongWork>);
static_assert(pwbench::HostWork<MoveOnlyWork>);
static_assert(!pwbench::HostStrategy<WrongStrategy>);

TEST(ContractsTest, TargetIsAlgorithmOwned) {
    const fixtures::TestAlgorithm algorithm;
    const auto candidate = fixtures::bytes("abc");
    const auto first = algorithm.make_target(fixtures::view(candidate), 5);
    const auto second = algorithm.make_target(fixtures::view(candidate), 11);
    EXPECT_NE(first.digest, second.digest);
    EXPECT_TRUE(algorithm.matches(algorithm.hash(fixtures::view(candidate), first), first));
    EXPECT_FALSE(algorithm.matches(algorithm.hash(fixtures::view(candidate), first), second));
}

TEST(ContractsTest, LabelsAreStable) {
    EXPECT_EQ(pwbench::to_string(pwbench::Backend::cpu), "cpu");
    EXPECT_EQ(pwbench::to_string(pwbench::Backend::cuda), "cuda");
    EXPECT_EQ(pwbench::to_string(pwbench::StopReason::found), "found");
    EXPECT_EQ(pwbench::to_string(pwbench::StopReason::error), "error");
}

}
