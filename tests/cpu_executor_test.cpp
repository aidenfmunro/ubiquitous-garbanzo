#include "pwbench/cpu_executor.hpp"

#include "fixtures.hpp"
#include "test_support.hpp"

#include <atomic>
#include <barrier>
#include <chrono>
#include <memory>
#include <thread>

using namespace std::chrono_literals;
using pwbench::CpuExecutor;
using pwbench::StopReason;

namespace {

void finds_candidate_and_owns_result() {
    const fixtures::TestAlgorithm algorithm;
    const auto expected = fixtures::bytes("beta");
    const auto target = algorithm.make_target(fixtures::view(expected));
    pwbench::RunResult result;
    {
        fixtures::BlockStrategy strategy(
            {fixtures::bytes("alpha"), expected, fixtures::bytes("gamma")});
        result = CpuExecutor::run(algorithm, target, strategy);
    }
    CHECK(result.reason == StopReason::found);
    CHECK(result.candidate == expected);
    CHECK(result.checks == 2);
    CHECK(result.elapsed >= 0ns);
    CHECK(result.error.empty());
    CHECK(algorithm.matches(algorithm.hash(fixtures::view(*result.candidate), target), target));
}

void supports_empty_and_binary_candidates() {
    const fixtures::TestAlgorithm algorithm;
    {
        const pwbench::Candidate expected;
        const auto target = algorithm.make_target(fixtures::view(expected));
        fixtures::BlockStrategy strategy({expected});
        const auto result = CpuExecutor::run(algorithm, target, strategy);
        CHECK(result.reason == StopReason::found);
        CHECK(result.candidate.has_value());
        CHECK(result.candidate->empty());
        CHECK(result.checks == 1);
    }
    {
        const pwbench::Candidate expected{0, 65, 255};
        const auto target = algorithm.make_target(fixtures::view(expected));
        fixtures::BlockStrategy strategy({{1, 2}, expected});
        const auto result = CpuExecutor::run(algorithm, target, strategy);
        CHECK(result.reason == StopReason::found);
        CHECK(result.candidate == expected);
        CHECK(result.checks == 2);
    }
}

void exhausts_stream_across_workers() {
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
    CHECK(result.reason == StopReason::exhausted);
    CHECK(result.checks == 20'003);
    CHECK(observed.load() == result.checks);
    CHECK(!result.candidate);
}

void handles_empty_source_and_empty_work() {
    fixtures::CounterStrategy empty(0);
    const auto exhausted = CpuExecutor::run(fixtures::TestAlgorithm{}, fixtures::absent_target(),
                                            empty, {}, {.threads = 4});
    CHECK(exhausted.reason == StopReason::exhausted);
    CHECK(exhausted.checks == 0);

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
    CHECK(result.reason == StopReason::exhausted);
    CHECK(result.checks == 1);
}

void enforces_exact_check_limit() {
    for (const auto thread_count : {std::size_t{1}, std::size_t{4}}) {
        fixtures::CounterStrategy strategy(20'000, 71);
        const auto result =
            CpuExecutor::run(fixtures::TestAlgorithm{}, fixtures::absent_target(), strategy,
                             {.max_checks = 1'003, .time_limit = {}}, {.threads = thread_count});
        CHECK(result.reason == StopReason::check_limit);
        CHECK(result.checks == 1'003);
        CHECK(!result.candidate);
    }
}

void match_at_check_limit_is_success() {
    const fixtures::TestAlgorithm algorithm;
    const auto expected = fixtures::bytes("b");
    const auto target = algorithm.make_target(fixtures::view(expected));
    fixtures::BlockStrategy strategy({fixtures::bytes("a"), expected, fixtures::bytes("c")}, 1);
    const auto result = CpuExecutor::run(algorithm, target, strategy,
                                         {.max_checks = 2, .time_limit = {}}, {.threads = 4});
    // Parallel completion order can differ from source order.
    CHECK(result.checks <= 2);
    CHECK(result.reason == StopReason::found || result.reason == StopReason::check_limit);

    fixtures::BlockStrategy sequential({fixtures::bytes("a"), expected, fixtures::bytes("c")});
    const auto boundary =
        CpuExecutor::run(algorithm, target, sequential, {.max_checks = 2, .time_limit = {}});
    CHECK(boundary.reason == StopReason::found);
    CHECK(boundary.candidate == expected);
    CHECK(boundary.checks == 2);
}

void zero_limits_do_no_work() {
    struct NeverReadStrategy {
        using work_type = fixtures::CounterStrategy::work_type;
        std::optional<work_type> next_work() {
            throw std::runtime_error("source must not be read");
        }
    } source;
    const auto count = CpuExecutor::run(fixtures::TestAlgorithm{}, fixtures::absent_target(),
                                        source, {.max_checks = 0, .time_limit = {}});
    CHECK(count.reason == StopReason::check_limit);
    CHECK(count.checks == 0);
    const auto time = CpuExecutor::run(fixtures::TestAlgorithm{}, fixtures::absent_target(), source,
                                       {.max_checks = {}, .time_limit = 0ns}, {.threads = 4});
    CHECK(time.reason == StopReason::time_limit);
    CHECK(time.checks == 0);
}

void deadline_is_checked_during_work() {
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
    CHECK(result.reason == StopReason::time_limit);
    CHECK(result.checks <= 1);
    CHECK(result.elapsed >= 1ms);
}

void rejects_invalid_options() {
    fixtures::CounterStrategy strategy(1);
    const auto threads = CpuExecutor::run(fixtures::TestAlgorithm{}, fixtures::absent_target(),
                                          strategy, {}, {.threads = 0});
    CHECK(threads.reason == StopReason::error);
    CHECK(!threads.error.empty());
    const auto interval = CpuExecutor::run(fixtures::TestAlgorithm{}, fixtures::absent_target(),
                                           strategy, {}, {.threads = 1, .poll_interval = 0});
    CHECK(interval.reason == StopReason::error);
    const auto duration = CpuExecutor::run(fixtures::TestAlgorithm{}, fixtures::absent_target(),
                                           strategy, {.max_checks = {}, .time_limit = -1ns});
    CHECK(duration.reason == StopReason::error);
    CHECK(duration.checks == 0);
}

void reports_algorithm_failure() {
    struct ThrowingAlgorithm : fixtures::TestAlgorithm {
        digest_type hash(pwbench::CandidateView, const target_type&) const {
            throw std::runtime_error("test hash failure");
        }
    } algorithm;
    fixtures::CounterStrategy strategy(1'000);
    const auto result =
        CpuExecutor::run(algorithm, fixtures::absent_target(), strategy, {}, {.threads = 4});
    CHECK(result.reason == StopReason::error);
    CHECK(result.error == "test hash failure");
    CHECK(result.checks == 0);
    CHECK(!result.candidate);
}

void preserves_checks_before_hash_failure() {
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
    CHECK(result.reason == StopReason::error);
    CHECK(result.error == "partial hash failure");
    CHECK(result.checks == 2);
}

void preserves_checks_before_comparison_failure() {
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
    CHECK(result.reason == StopReason::error);
    CHECK(result.error == "partial comparison failure");
    CHECK(result.checks == 2);
}

void preserves_checks_before_source_failure() {
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
    CHECK(result.reason == StopReason::error);
    CHECK(result.error == "partial source failure");
    CHECK(result.checks == 2);
}

void failure_takes_precedence_over_concurrent_match() {
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
    CHECK(result.reason == StopReason::error);
    CHECK(result.error == "concurrent failure");
    CHECK(result.checks == 1);
    CHECK(!result.candidate);
}

void reports_source_and_work_failures() {
    struct ThrowingSource {
        using work_type = fixtures::CounterStrategy::work_type;
        std::optional<work_type> next_work() {
            throw std::runtime_error("test source failure");
        }
    } source;
    const auto source_result = CpuExecutor::run(
        fixtures::TestAlgorithm{}, fixtures::absent_target(), source, {}, {.threads = 4});
    CHECK(source_result.reason == StopReason::error);
    CHECK(source_result.error == "test source failure");

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
    CHECK(work_result.reason == StopReason::error);
    CHECK(work_result.error == "unknown execution error");
}

void rejects_invalid_candidate_view() {
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
    CHECK(result.reason == StopReason::error);
    CHECK(result.error == "nonempty candidate has null data");
    CHECK(result.checks == 0);
}

void supports_move_only_work() {
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
    CHECK(result.reason == StopReason::exhausted);
    CHECK(result.checks == 1);
}

}

int main() {
    return run_tests({
        {"match and result ownership", finds_candidate_and_owns_result},
        {"empty and binary candidates", supports_empty_and_binary_candidates},
        {"parallel streaming and exact counts", exhausts_stream_across_workers},
        {"empty source and work", handles_empty_source_and_empty_work},
        {"exact check limit", enforces_exact_check_limit},
        {"match at check limit", match_at_check_limit_is_success},
        {"zero limits", zero_limits_do_no_work},
        {"deadline polling", deadline_is_checked_during_work},
        {"invalid options", rejects_invalid_options},
        {"algorithm failure", reports_algorithm_failure},
        {"partial progress before hash failure", preserves_checks_before_hash_failure},
        {"partial progress before comparison failure", preserves_checks_before_comparison_failure},
        {"partial progress before source failure", preserves_checks_before_source_failure},
        {"error precedence over match", failure_takes_precedence_over_concurrent_match},
        {"source and work failures", reports_source_and_work_failures},
        {"invalid candidate view", rejects_invalid_candidate_view},
        {"move-only work", supports_move_only_work},
    });
}
