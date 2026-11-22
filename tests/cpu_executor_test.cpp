#include "pwbench/cpu_executor.hpp"

#include "fixtures.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <barrier>
#include <chrono>
#include <memory>
#include <thread>

using namespace std::chrono_literals;
using pwbench::CpuExecutor;
using pwbench::StopReason;

namespace {

TEST(CpuExecutorTest, FindsCandidateAndOwnsResult) {
    const fixtures::TestAlgorithm algorithm;
    const auto expected = fixtures::bytes("beta");
    const auto target = algorithm.make_target(fixtures::view(expected));
    pwbench::RunResult result;
    {
        fixtures::BlockStrategy strategy(
            {fixtures::bytes("alpha"), expected, fixtures::bytes("gamma")});
        result = CpuExecutor::run(algorithm, target, strategy);
    }
    EXPECT_EQ(result.reason, StopReason::found);
    ASSERT_EQ(result.candidate, expected);
    EXPECT_EQ(result.checks, 2);
    EXPECT_GE(result.elapsed, 0ns);
    EXPECT_TRUE(result.error.empty());
    EXPECT_TRUE(
        algorithm.matches(algorithm.hash(fixtures::view(*result.candidate), target), target));
}

TEST(CpuExecutorTest, SupportsEmptyAndBinaryCandidates) {
    const fixtures::TestAlgorithm algorithm;
    {
        const pwbench::Candidate expected;
        const auto target = algorithm.make_target(fixtures::view(expected));
        fixtures::BlockStrategy strategy({expected});
        const auto result = CpuExecutor::run(algorithm, target, strategy);
        EXPECT_EQ(result.reason, StopReason::found);
        ASSERT_TRUE(result.candidate.has_value());
        EXPECT_TRUE(result.candidate->empty());
        EXPECT_EQ(result.checks, 1);
    }
    {
        const pwbench::Candidate expected{0, 65, 255};
        const auto target = algorithm.make_target(fixtures::view(expected));
        fixtures::BlockStrategy strategy({{1, 2}, expected});
        const auto result = CpuExecutor::run(algorithm, target, strategy);
        EXPECT_EQ(result.reason, StopReason::found);
        EXPECT_EQ(result.candidate, expected);
        EXPECT_EQ(result.checks, 2);
    }
}

TEST(CpuExecutorTest, ExhaustsStreamAcrossWorkers) {
    struct CountingAlgorithm : fixtures::TestAlgorithm {
        std::atomic<std::uint64_t>* observed;
        digest_type hash(pwbench::CandidateView candidate, const target_type& target) const {
            observed->fetch_add(1, std::memory_order_relaxed);
            return TestAlgorithm::hash(candidate, target);
        }
    };
    std::atomic<std::uint64_t> observed{0};
    const CountingAlgorithm algorithm{{}, &observed};
    fixtures::CounterStrategy strategy(20'003, 31);
    const auto result =
        CpuExecutor::run(algorithm, fixtures::absent_target(), strategy, {}, {.threads = 4});
    EXPECT_EQ(result.reason, StopReason::exhausted);
    EXPECT_EQ(result.checks, 20'003);
    EXPECT_EQ(observed.load(), result.checks);
    EXPECT_FALSE(result.candidate);
}

TEST(CpuExecutorTest, HandlesEmptySourceAndEmptyWork) {
    fixtures::CounterStrategy empty(0);
    const auto exhausted = CpuExecutor::run(fixtures::TestAlgorithm{}, fixtures::absent_target(),
                                            empty, {}, {.threads = 4});
    EXPECT_EQ(exhausted.reason, StopReason::exhausted);
    EXPECT_EQ(exhausted.checks, 0);

    struct EmptyFirstStrategy {
        using work_type = fixtures::CounterStrategy::work_type;
        int call{};
        std::optional<work_type> next_work() {
            switch (call++) {
            case 0:
                return work_type{0, 0};
            case 1:
                return work_type{0, 1};
            default:
                return std::nullopt;
            }
        }
    } strategy;
    const auto result =
        CpuExecutor::run(fixtures::TestAlgorithm{}, fixtures::absent_target(), strategy);
    EXPECT_EQ(result.reason, StopReason::exhausted);
    EXPECT_EQ(result.checks, 1);
}

TEST(CpuExecutorTest, EnforcesExactCheckLimit) {
    for (const auto thread_count : {std::size_t{1}, std::size_t{4}}) {
        SCOPED_TRACE(thread_count);
        fixtures::CounterStrategy strategy(20'000, 71);
        const auto result =
            CpuExecutor::run(fixtures::TestAlgorithm{}, fixtures::absent_target(), strategy,
                             {.max_checks = 1'003, .time_limit = {}}, {.threads = thread_count});
        EXPECT_EQ(result.reason, StopReason::check_limit);
        EXPECT_EQ(result.checks, 1'003);
        EXPECT_FALSE(result.candidate);
    }
}

TEST(CpuExecutorTest, MatchAtCheckLimitIsSuccess) {
    const fixtures::TestAlgorithm algorithm;
    const auto expected = fixtures::bytes("b");
    const auto target = algorithm.make_target(fixtures::view(expected));
    fixtures::BlockStrategy strategy({fixtures::bytes("a"), expected, fixtures::bytes("c")}, 1);
    const auto result = CpuExecutor::run(algorithm, target, strategy,
                                         {.max_checks = 2, .time_limit = {}}, {.threads = 4});
    // Parallel completion order can differ from source order.
    EXPECT_LE(result.checks, 2U);
    EXPECT_TRUE(result.reason == StopReason::found || result.reason == StopReason::check_limit);

    fixtures::BlockStrategy sequential({fixtures::bytes("a"), expected, fixtures::bytes("c")});
    const auto boundary =
        CpuExecutor::run(algorithm, target, sequential, {.max_checks = 2, .time_limit = {}});
    EXPECT_EQ(boundary.reason, StopReason::found);
    EXPECT_EQ(boundary.candidate, expected);
    EXPECT_EQ(boundary.checks, 2);
}

TEST(CpuExecutorTest, ZeroLimitsDoNoWork) {
    struct NeverReadStrategy {
        using work_type = fixtures::CounterStrategy::work_type;
        std::optional<work_type> next_work() {
            throw std::runtime_error("source must not be read");
        }
    } source;
    const auto count = CpuExecutor::run(fixtures::TestAlgorithm{}, fixtures::absent_target(),
                                        source, {.max_checks = 0, .time_limit = {}});
    EXPECT_EQ(count.reason, StopReason::check_limit);
    EXPECT_EQ(count.checks, 0);
    const auto time = CpuExecutor::run(fixtures::TestAlgorithm{}, fixtures::absent_target(), source,
                                       {.max_checks = {}, .time_limit = 0ns}, {.threads = 4});
    EXPECT_EQ(time.reason, StopReason::time_limit);
    EXPECT_EQ(time.checks, 0);
}

TEST(CpuExecutorTest, DeadlineIsCheckedDuringWork) {
    struct SlowAlgorithm : fixtures::TestAlgorithm {
        digest_type hash(pwbench::CandidateView candidate, const target_type& target) const {
            std::this_thread::sleep_for(10ms);
            return TestAlgorithm::hash(candidate, target);
        }
    } algorithm;
    fixtures::CounterStrategy strategy(100);
    const auto result =
        CpuExecutor::run(algorithm, fixtures::absent_target(), strategy,
                         {.max_checks = {}, .time_limit = 1ms}, {.threads = 1, .poll_interval = 1});
    EXPECT_EQ(result.reason, StopReason::time_limit);
    EXPECT_LE(result.checks, 1U);
    EXPECT_GE(result.elapsed, 1ms);
}

TEST(CpuExecutorTest, RejectsInvalidOptions) {
    fixtures::CounterStrategy strategy(1);
    const auto threads = CpuExecutor::run(fixtures::TestAlgorithm{}, fixtures::absent_target(),
                                          strategy, {}, {.threads = 0});
    EXPECT_EQ(threads.reason, StopReason::error);
    EXPECT_FALSE(threads.error.empty());
    const auto interval = CpuExecutor::run(fixtures::TestAlgorithm{}, fixtures::absent_target(),
                                           strategy, {}, {.threads = 1, .poll_interval = 0});
    EXPECT_EQ(interval.reason, StopReason::error);
    const auto duration = CpuExecutor::run(fixtures::TestAlgorithm{}, fixtures::absent_target(),
                                           strategy, {.max_checks = {}, .time_limit = -1ns});
    EXPECT_EQ(duration.reason, StopReason::error);
    EXPECT_EQ(duration.checks, 0);
}

TEST(CpuExecutorTest, ReportsAlgorithmFailure) {
    struct ThrowingAlgorithm : fixtures::TestAlgorithm {
        digest_type hash(pwbench::CandidateView, const target_type&) const {
            throw std::runtime_error("test hash failure");
        }
    } algorithm;
    fixtures::CounterStrategy strategy(1'000);
    const auto result =
        CpuExecutor::run(algorithm, fixtures::absent_target(), strategy, {}, {.threads = 4});
    EXPECT_EQ(result.reason, StopReason::error);
    EXPECT_EQ(result.error, "test hash failure");
    EXPECT_EQ(result.checks, 0);
    EXPECT_FALSE(result.candidate);
}

TEST(CpuExecutorTest, PreservesChecksBeforeHashFailure) {
    struct Algorithm : fixtures::TestAlgorithm {
        digest_type hash(pwbench::CandidateView candidate, const target_type& target) const {
            if (candidate.data[0] == 2) {
                throw std::runtime_error("partial hash failure");
            }
            return TestAlgorithm::hash(candidate, target);
        }
    } algorithm;
    fixtures::CounterStrategy strategy(10);
    const auto result = CpuExecutor::run(algorithm, fixtures::absent_target(), strategy);
    EXPECT_EQ(result.reason, StopReason::error);
    EXPECT_EQ(result.error, "partial hash failure");
    EXPECT_EQ(result.checks, 2);
}

TEST(CpuExecutorTest, PreservesChecksBeforeComparisonFailure) {
    struct Algorithm : fixtures::TestAlgorithm {
        bool matches(digest_type digest, const target_type& target) const {
            if (digest == 2) {
                throw std::runtime_error("partial comparison failure");
            }
            return TestAlgorithm::matches(digest, target);
        }
    } algorithm;
    fixtures::CounterStrategy strategy(10);
    const auto result = CpuExecutor::run(algorithm, fixtures::absent_target(), strategy);
    EXPECT_EQ(result.reason, StopReason::error);
    EXPECT_EQ(result.error, "partial comparison failure");
    EXPECT_EQ(result.checks, 2);
}

TEST(CpuExecutorTest, PreservesChecksBeforeSourceFailure) {
    struct Strategy {
        using work_type = fixtures::CounterStrategy::work_type;
        bool consumed{};
        std::optional<work_type> next_work() {
            if (consumed) {
                throw std::runtime_error("partial source failure");
            }
            consumed = true;
            return work_type{0, 2};
        }
    } strategy;
    const auto result =
        CpuExecutor::run(fixtures::TestAlgorithm{}, fixtures::absent_target(), strategy);
    EXPECT_EQ(result.reason, StopReason::error);
    EXPECT_EQ(result.error, "partial source failure");
    EXPECT_EQ(result.checks, 2);
}

TEST(CpuExecutorTest, FailureTakesPrecedenceOverConcurrentMatch) {
    struct MixedAlgorithm : fixtures::TestAlgorithm {
        std::barrier<>* rendezvous;
        digest_type hash(pwbench::CandidateView candidate, const target_type& target) const {
            rendezvous->arrive_and_wait();
            if (candidate.data[0] == 0) {
                throw std::runtime_error("concurrent failure");
            }
            return TestAlgorithm::hash(candidate, target);
        }
    };
    std::barrier rendezvous{2};
    const MixedAlgorithm algorithm{{}, &rendezvous};
    fixtures::CounterStrategy strategy(2, 1);
    const auto result = CpuExecutor::run(algorithm, {1, 0}, strategy, {}, {.threads = 2});
    EXPECT_EQ(result.reason, StopReason::error);
    EXPECT_EQ(result.error, "concurrent failure");
    EXPECT_EQ(result.checks, 1);
    EXPECT_FALSE(result.candidate);
}

TEST(CpuExecutorTest, ReportsSourceAndWorkFailures) {
    struct ThrowingSource {
        using work_type = fixtures::CounterStrategy::work_type;
        std::optional<work_type> next_work() {
            throw std::runtime_error("test source failure");
        }
    } source;
    const auto source_result = CpuExecutor::run(
        fixtures::TestAlgorithm{}, fixtures::absent_target(), source, {}, {.threads = 4});
    EXPECT_EQ(source_result.reason, StopReason::error);
    EXPECT_EQ(source_result.error, "test source failure");

    struct ThrowingWorkSource {
        struct work_type {
            std::optional<pwbench::CandidateView> next_candidate() {
                throw 42;
            }
        };
        std::optional<work_type> next_work() {
            return work_type{};
        }
    } work_source;
    const auto work_result = CpuExecutor::run(fixtures::TestAlgorithm{}, fixtures::absent_target(),
                                              work_source, {}, {.threads = 4});
    EXPECT_EQ(work_result.reason, StopReason::error);
    EXPECT_EQ(work_result.error, "unknown execution error");
}

TEST(CpuExecutorTest, RejectsInvalidCandidateView) {
    struct InvalidViewSource {
        struct work_type {
            std::optional<pwbench::CandidateView> next_candidate() {
                return pwbench::CandidateView{nullptr, 1};
            }
        };
        std::optional<work_type> next_work() {
            return work_type{};
        }
    } strategy;
    const auto result =
        CpuExecutor::run(fixtures::TestAlgorithm{}, fixtures::absent_target(), strategy);
    EXPECT_EQ(result.reason, StopReason::error);
    EXPECT_EQ(result.error, "nonempty candidate has null data");
    EXPECT_EQ(result.checks, 0);
}

TEST(CpuExecutorTest, SupportsMoveOnlyWork) {
    struct MoveOnlyStrategy {
        struct work_type {
            std::unique_ptr<pwbench::Byte> byte{std::make_unique<pwbench::Byte>(9)};
            bool consumed{};
            std::optional<pwbench::CandidateView> next_candidate() {
                if (consumed) {
                    return std::nullopt;
                }
                consumed = true;
                return pwbench::CandidateView{byte.get(), 1};
            }
        };
        bool consumed{};
        std::optional<work_type> next_work() {
            if (consumed) {
                return std::nullopt;
            }
            consumed = true;
            return work_type{};
        }
    } strategy;
    const auto result = CpuExecutor::run(fixtures::TestAlgorithm{}, fixtures::absent_target(),
                                         strategy, {}, {.threads = 2});
    EXPECT_EQ(result.reason, StopReason::exhausted);
    EXPECT_EQ(result.checks, 1);
}

}
