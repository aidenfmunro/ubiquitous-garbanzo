#include "pwbench/registry.hpp"

#include "module_fixtures.hpp"

#include <gtest/gtest.h>

namespace {

const pwbench::Selection selection{"test-sum", "test-blocks", pwbench::Backend::cpu};

pwbench::PreparedExperiment empty_experiment(const pwbench::ExperimentRequest&) {
    return {"test", [](const pwbench::RunLimits&) { return pwbench::RunResult{}; }};
}

TEST(RegistryTest, PreparesOnceAndOwnsRepeatableExecution) {
    pwbench::PreparedExperiment prepared;
    fixtures::SumModule::preparations = 0;
    {
        auto registry = fixtures::module_registry();
        auto password = fixtures::bytes("beta");
        const pwbench::ExperimentRequest request{fixtures::view(password), {{"salt", "11"}}};
        prepared = registry.prepare(selection, request);
        EXPECT_TRUE(registry.supports(selection));
        EXPECT_EQ(registry.selections().size(), 4);
        EXPECT_EQ(fixtures::SumModule::preparations, 1);
        password.clear();
    }
    EXPECT_EQ(prepared.target_hash, "423");
    for (int repeat = 0; repeat < 3; ++repeat) {
        const auto result = prepared.run();
        EXPECT_EQ(result.reason, pwbench::StopReason::found);
        EXPECT_EQ(result.candidate, fixtures::bytes("beta"));
        EXPECT_EQ(result.checks, 2);
    }
    EXPECT_EQ(fixtures::SumModule::preparations, 1);
    const auto limited = prepared.run({.max_checks = 1, .time_limit = {}});
    EXPECT_EQ(limited.reason, pwbench::StopReason::check_limit);
    EXPECT_EQ(limited.checks, 1);
}

TEST(RegistryTest, RejectsUnsupportedCombinationsBeforePreparation) {
    const auto registry = fixtures::module_registry();
    fixtures::SumModule::preparations = 0;
    for (const auto& missing :
         std::vector<pwbench::Selection>{{"test-sum", "test-blocks", pwbench::Backend::cuda},
                                         {"missing", "test-blocks", pwbench::Backend::cpu},
                                         {"test-sum", "missing", pwbench::Backend::cpu}}) {
        EXPECT_FALSE(registry.supports(missing));
        EXPECT_THROW(static_cast<void>(registry.prepare(missing, {})), std::invalid_argument);
        EXPECT_THROW(static_cast<void>(registry.options(missing)), std::invalid_argument);
    }
    EXPECT_EQ(fixtures::SumModule::preparations, 0);
}

TEST(RegistryTest, RejectsInvalidRegistrations) {
    pwbench::Registry registry;
    registry.add(selection, {}, empty_experiment);
    EXPECT_THROW(registry.add(selection, {}, empty_experiment), std::invalid_argument);
    EXPECT_THROW(registry.add({"", "s", pwbench::Backend::cpu}, {}, empty_experiment),
                 std::invalid_argument);
    EXPECT_THROW(registry.add({"a", "", pwbench::Backend::cpu}, {}, empty_experiment),
                 std::invalid_argument);
    EXPECT_THROW(registry.add({"a", "s", static_cast<pwbench::Backend>(9)}, {}, empty_experiment),
                 std::invalid_argument);
    EXPECT_THROW(registry.add({"a", "s", pwbench::Backend::cpu}, {}, {}), std::invalid_argument);
}

TEST(RegistryTest, RejectsParameterCollisionsAndInvalidNames) {
    const pwbench::Selection other{"other", "other", pwbench::Backend::cpu};
    pwbench::Registry registry;
    for (const auto name : {"password", "algorithm", "strategy", "backend", "help", "", "bad name",
                            "--wrong", "name=value"}) {
        EXPECT_THROW(registry.add(other, {{name, "N", "test", "1"}}, empty_experiment),
                     std::invalid_argument);
    }
    EXPECT_THROW(
        {
            registry.add(other, {{"cost", "N", "test", "1"}, {"cost", "N", "duplicate", "2"}},
                         empty_experiment);
        },
        std::invalid_argument);
    EXPECT_THROW(registry.add(other, {{"cost", "", "test", "1"}}, empty_experiment),
                 std::invalid_argument);
    struct CollisionModule : fixtures::BlocksModule {
        static std::vector<pwbench::OptionSpec> options() {
            return {{"salt", "N", "Conflicts with algorithm", "1"}};
        }
    };
    EXPECT_THROW((pwbench::register_cpu<fixtures::SumModule, CollisionModule>(registry)),
                 std::invalid_argument);
    EXPECT_TRUE(registry.selections().empty());
}

TEST(RegistryTest, ResolvesDefaultsRequiredValuesAndEmptyValues) {
    pwbench::Registry registry;
    std::size_t calls = 0;
    registry.add(selection, {{"cost", "N", "test", "3"}, {"text", "TEXT", "required", {}}},
                 [&](const pwbench::ExperimentRequest& request) {
                     ++calls;
                     EXPECT_EQ(pwbench::parameter(request.parameters, "cost"), "3");
                     EXPECT_TRUE(pwbench::parameter(request.parameters, "text").empty());
                     return empty_experiment(request);
                 });
    EXPECT_EQ(registry.options(selection).size(), 2);
    EXPECT_THROW(static_cast<void>(registry.prepare(selection, {})), std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(registry.prepare(selection, {{}, {{"text", ""}, {"unknown", "1"}}})),
        std::invalid_argument);
    EXPECT_THROW(static_cast<void>(registry.prepare(selection, {{nullptr, 1}, {{"text", ""}}})),
                 std::invalid_argument);
    EXPECT_EQ(calls, 0);
    static_cast<void>(registry.prepare(selection, {{}, {{"text", ""}}}));
    EXPECT_EQ(calls, 1);
}

TEST(RegistryTest, PositiveParametersRejectInvalidSignedAndUnsignedValues) {
    EXPECT_EQ(pwbench::positive_parameter<int>({{"cost", "12"}}, "cost"), 12);
    EXPECT_EQ(pwbench::positive_parameter<std::uint32_t>({{"cost", "4294967295"}}, "cost"),
              4294967295U);
    for (const auto value : {"", "0", "-1", "12x", "+1", " 1", "999999999999999999999"}) {
        EXPECT_THROW(pwbench::positive_parameter<int>({{"cost", value}}, "cost"),
                     std::invalid_argument);
        EXPECT_THROW(pwbench::positive_parameter<std::uint32_t>({{"cost", value}}, "cost"),
                     std::invalid_argument);
    }
}

TEST(RegistryTest, ReportsPreparationAndExecutionErrors) {
    pwbench::Registry registry;
    registry.add(selection, {},
                 [](const pwbench::ExperimentRequest&) -> pwbench::PreparedExperiment {
                     throw std::runtime_error("target preparation failed");
                 });
    EXPECT_THROW(static_cast<void>(registry.prepare(selection, {})), std::runtime_error);
    registry.add({"empty", "other", pwbench::Backend::cpu}, {},
                 [](const pwbench::ExperimentRequest&) { return pwbench::PreparedExperiment{}; });
    EXPECT_THROW(static_cast<void>(registry.prepare({"empty", "other", pwbench::Backend::cpu}, {})),
                 std::invalid_argument);
    pwbench::PreparedExperiment prepared{"test",
                                         [](const pwbench::RunLimits&) -> pwbench::RunResult {
                                             throw std::runtime_error("source creation failed");
                                         }};
    EXPECT_EQ(prepared.run().error, "source creation failed");
    EXPECT_EQ(prepared.run().checks, 0);
    prepared.runner = [](const pwbench::RunLimits&) -> pwbench::RunResult { throw 42; };
    EXPECT_EQ(prepared.run().error, "unknown execution error");
    prepared.runner = {};
    EXPECT_EQ(prepared.run().reason, pwbench::StopReason::error);
}

TEST(RegistryTest, SupportsIndependentModulesAndMoveOnlyTargets) {
    const auto registry = fixtures::module_registry();
    const auto password = fixtures::bytes("beta");
    const pwbench::ExperimentRequest request{fixtures::view(password), {}};
    const auto length = registry.prepare({"test-length", "test-blocks", pwbench::Backend::cpu},
                                         {request.password, {{"factor", "3"}}});
    EXPECT_EQ(length.target_hash, "12");
    EXPECT_EQ(length.run().candidate, password);
    const auto counter = registry.prepare({"test-length", "test-counter", pwbench::Backend::cpu},
                                          {request.password, {{"count", "19"}}});
    EXPECT_EQ(counter.run().reason, pwbench::StopReason::exhausted);
    EXPECT_EQ(counter.run().checks, 19);
    EXPECT_EQ(registry.prepare(selection, request).run().candidate, password);
}

}
