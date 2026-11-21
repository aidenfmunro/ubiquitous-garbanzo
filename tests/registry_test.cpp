#include "pwbench/cpu_executor.hpp"
#include "pwbench/registry.hpp"

#include "fixtures.hpp"
#include "test_support.hpp"

namespace {

const pwbench::Selection selection{
    std::string(fixtures::TestAlgorithm::id),
    std::string(fixtures::BlockStrategy::id),
    pwbench::Backend::cpu,
};

void register_fixture(pwbench::Registry& registry) {
    const fixtures::TestAlgorithm algorithm;
    const auto original = fixtures::bytes("beta");
    const auto target = algorithm.make_target(fixtures::view(original));
    registry.add(selection, [algorithm, target](const pwbench::RunLimits& limits) {
        fixtures::BlockStrategy strategy(
            {fixtures::bytes("alpha"), fixtures::bytes("beta"), fixtures::bytes("gamma")});
        return pwbench::CpuExecutor::run(algorithm, target, strategy, limits);
    });
}

void registered_pipeline_is_repeatable() {
    pwbench::Registry registry;
    register_fixture(registry);
    CHECK(registry.supports(selection));
    CHECK(registry.selections() == std::vector<pwbench::Selection>{selection});
    for (int repeat = 0; repeat < 3; ++repeat) {
        const auto result = registry.run(selection);
        CHECK(result.reason == pwbench::StopReason::found);
        CHECK(result.candidate == fixtures::bytes("beta"));
        CHECK(result.checks == 2);
    }
    const auto limited = registry.run(selection, {.max_checks = 1, .time_limit = {}});
    CHECK(limited.reason == pwbench::StopReason::check_limit);
    CHECK(limited.checks == 1);
}

void rejects_unsupported_combination_without_fallback() {
    pwbench::Registry registry;
    int calls = 0;
    registry.add(selection, [&](const pwbench::RunLimits&) {
        ++calls;
        return pwbench::RunResult{};
    });
    auto cuda = selection;
    cuda.backend = pwbench::Backend::cuda;
    CHECK(!registry.supports(cuda));
    const auto result = registry.run(cuda);
    CHECK(result.reason == pwbench::StopReason::error);
    CHECK(result.error == "unsupported combination: algorithm=test-sum, "
                          "strategy=test-blocks, backend=cuda");
    auto other_strategy = selection;
    other_strategy.strategy = "not-registered";
    CHECK(registry.run(other_strategy).reason == pwbench::StopReason::error);
    auto other_algorithm = selection;
    other_algorithm.algorithm = "not-registered";
    CHECK(registry.run(other_algorithm).reason == pwbench::StopReason::error);
    CHECK(calls == 0);
}

void rejects_invalid_registrations() {
    pwbench::Registry registry;
    const pwbench::Runner runner = [](const pwbench::RunLimits&) { return pwbench::RunResult{}; };
    registry.add(selection, runner);
    check_throws<std::invalid_argument>([&] { registry.add(selection, runner); });
    check_throws<std::invalid_argument>(
        [&] { registry.add({"", "strategy", pwbench::Backend::cpu}, runner); });
    check_throws<std::invalid_argument>(
        [&] { registry.add({"algorithm", "", pwbench::Backend::cpu}, runner); });
    check_throws<std::invalid_argument>(
        [&] { registry.add({"algorithm", "strategy", static_cast<pwbench::Backend>(9)}, runner); });
    check_throws<std::invalid_argument>(
        [&] { registry.add({"algorithm", "strategy", pwbench::Backend::cpu}, {}); });
}

void reports_preparation_errors() {
    pwbench::Registry registry;
    registry.add(selection, [](const pwbench::RunLimits&) -> pwbench::RunResult {
        throw std::runtime_error("cannot prepare test data");
    });
    const auto result = registry.run(selection);
    CHECK(result.reason == pwbench::StopReason::error);
    CHECK(result.error == "cannot prepare test data");
    CHECK(result.checks == 0);

    registry.add({"other", "other", pwbench::Backend::cpu},
                 [](const pwbench::RunLimits&) -> pwbench::RunResult { throw 42; });
    CHECK(registry.run({"other", "other", pwbench::Backend::cpu}).error ==
          "unknown preparation error");
}

void algorithms_and_strategies_are_independent() {
    pwbench::Registry registry;
    register_fixture(registry);
    const pwbench::Selection counter{
        std::string(fixtures::TestAlgorithm::id),
        std::string(fixtures::CounterStrategy::id),
        pwbench::Backend::cpu,
    };
    registry.add(counter, [](const pwbench::RunLimits& limits) {
        fixtures::CounterStrategy strategy(10);
        return pwbench::CpuExecutor::run(fixtures::TestAlgorithm{}, fixtures::absent_target(),
                                         strategy, limits);
    });
    CHECK(registry.selections().size() == 2);
    CHECK(registry.run(selection).reason == pwbench::StopReason::found);
    CHECK(registry.run(counter).reason == pwbench::StopReason::exhausted);
    CHECK(registry.run(counter).checks == 10);
}

void new_algorithm_can_define_a_different_target() {
    struct LengthAlgorithm {
        struct digest_type {
            std::size_t length;
            bool operator==(const digest_type&) const = default;
        };
        struct target_type {
            digest_type wanted;
        };
        digest_type hash(pwbench::CandidateView candidate, const target_type&) const {
            return {candidate.size};
        }
        bool matches(const digest_type& digest, const target_type& target) const {
            return digest == target.wanted;
        }
    };
    static_assert(pwbench::CpuAlgorithm<LengthAlgorithm>);
    pwbench::Registry registry;
    register_fixture(registry);
    const pwbench::Selection length{"test-length", selection.strategy, pwbench::Backend::cpu};
    registry.add(length, [](const pwbench::RunLimits& limits) {
        fixtures::BlockStrategy strategy({fixtures::bytes("alpha"), fixtures::bytes("beta")});
        return pwbench::CpuExecutor::run(LengthAlgorithm{}, {{4}}, strategy, limits);
    });
    CHECK(registry.selections().size() == 2);
    CHECK(registry.run(selection).reason == pwbench::StopReason::found);
    const auto result = registry.run(length);
    CHECK(result.reason == pwbench::StopReason::found);
    CHECK(result.candidate == fixtures::bytes("beta"));
}

}

int main() {
    return run_tests({
        {"repeatable registered pipeline", registered_pipeline_is_repeatable},
        {"unsupported combinations and no fallback",
         rejects_unsupported_combination_without_fallback},
        {"invalid registrations", rejects_invalid_registrations},
        {"preparation errors", reports_preparation_errors},
        {"independent strategy extension", algorithms_and_strategies_are_independent},
        {"independent algorithm extension", new_algorithm_can_define_a_different_target},
    });
}
