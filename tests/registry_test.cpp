#include "pwbench/registry.hpp"

#include "module_fixtures.hpp"
#include "test_support.hpp"

namespace {

const pwbench::Selection selection{"test-sum", "test-blocks", pwbench::Backend::cpu};

pwbench::PreparedExperiment empty_experiment(const pwbench::ExperimentRequest&) {
    return {"test", [](const pwbench::RunLimits&) { return pwbench::RunResult{}; }};
}

void prepares_once_and_owns_repeatable_execution() {
    pwbench::PreparedExperiment prepared;
    fixtures::SumModule::preparations = 0;
    {
        auto registry = fixtures::module_registry();
        auto password = fixtures::bytes("beta");
        const pwbench::ExperimentRequest request{fixtures::view(password), {{"salt", "11"}}};
        prepared = registry.prepare(selection, request);
        CHECK(registry.supports(selection));
        CHECK(registry.selections().size() == 4);
        CHECK(fixtures::SumModule::preparations == 1);
        password.clear();
    }
    CHECK(prepared.target_hash == "423");
    for (int repeat = 0; repeat < 3; ++repeat) {
        const auto result = prepared.run();
        CHECK(result.reason == pwbench::StopReason::found);
        CHECK(result.candidate == fixtures::bytes("beta"));
        CHECK(result.checks == 2);
    }
    CHECK(fixtures::SumModule::preparations == 1);
    const auto limited = prepared.run({.max_checks = 1, .time_limit = {}});
    CHECK(limited.reason == pwbench::StopReason::check_limit);
    CHECK(limited.checks == 1);
}

void rejects_unsupported_combinations_before_preparation() {
    const auto registry = fixtures::module_registry();
    fixtures::SumModule::preparations = 0;
    for (const auto& missing :
         std::vector<pwbench::Selection>{{"test-sum", "test-blocks", pwbench::Backend::cuda},
                                         {"missing", "test-blocks", pwbench::Backend::cpu},
                                         {"test-sum", "missing", pwbench::Backend::cpu}}) {
        CHECK(!registry.supports(missing));
        check_throws<std::invalid_argument>(
            [&] { static_cast<void>(registry.prepare(missing, {})); });
        check_throws<std::invalid_argument>([&] { static_cast<void>(registry.options(missing)); });
    }
    CHECK(fixtures::SumModule::preparations == 0);
}

void rejects_invalid_registrations() {
    pwbench::Registry registry;
    registry.add(selection, {}, empty_experiment);
    check_throws<std::invalid_argument>([&] { registry.add(selection, {}, empty_experiment); });
    check_throws<std::invalid_argument>(
        [&] { registry.add({"", "s", pwbench::Backend::cpu}, {}, empty_experiment); });
    check_throws<std::invalid_argument>(
        [&] { registry.add({"a", "", pwbench::Backend::cpu}, {}, empty_experiment); });
    check_throws<std::invalid_argument>(
        [&] { registry.add({"a", "s", static_cast<pwbench::Backend>(9)}, {}, empty_experiment); });
    check_throws<std::invalid_argument>(
        [&] { registry.add({"a", "s", pwbench::Backend::cpu}, {}, {}); });
}

void rejects_parameter_collisions_and_invalid_names() {
    const pwbench::Selection other{"other", "other", pwbench::Backend::cpu};
    pwbench::Registry registry;
    for (const auto name : {"password", "algorithm", "strategy", "backend", "help", "", "bad name",
                            "--wrong", "name=value"}) {
        check_throws<std::invalid_argument>(
            [&] { registry.add(other, {{name, "N", "test", "1"}}, empty_experiment); });
    }
    check_throws<std::invalid_argument>([&] {
        registry.add(other, {{"cost", "N", "test", "1"}, {"cost", "N", "duplicate", "2"}},
                     empty_experiment);
    });
    check_throws<std::invalid_argument>(
        [&] { registry.add(other, {{"cost", "", "test", "1"}}, empty_experiment); });
    struct CollisionModule : fixtures::BlocksModule {
        static std::vector<pwbench::OptionSpec> options() {
            return {{"salt", "N", "Conflicts with algorithm", "1"}};
        }
    };
    check_throws<std::invalid_argument>(
        [&] { pwbench::register_cpu<fixtures::SumModule, CollisionModule>(registry); });
    CHECK(registry.selections().empty());
}

void resolves_defaults_required_values_and_empty_values() {
    pwbench::Registry registry;
    std::size_t calls = 0;
    registry.add(selection, {{"cost", "N", "test", "3"}, {"text", "TEXT", "required", {}}},
                 [&](const pwbench::ExperimentRequest& request) {
                     ++calls;
                     CHECK(pwbench::parameter(request.parameters, "cost") == "3");
                     CHECK(pwbench::parameter(request.parameters, "text").empty());
                     return empty_experiment(request);
                 });
    CHECK(registry.options(selection).size() == 2);
    check_throws<std::invalid_argument>(
        [&] { static_cast<void>(registry.prepare(selection, {})); });
    check_throws<std::invalid_argument>([&] {
        static_cast<void>(registry.prepare(selection, {{}, {{"text", ""}, {"unknown", "1"}}}));
    });
    check_throws<std::invalid_argument>(
        [&] { static_cast<void>(registry.prepare(selection, {{nullptr, 1}, {{"text", ""}}})); });
    CHECK(calls == 0);
    static_cast<void>(registry.prepare(selection, {{}, {{"text", ""}}}));
    CHECK(calls == 1);
}

void positive_parameters_reject_invalid_signed_and_unsigned_values() {
    CHECK(pwbench::positive_parameter<int>({{"cost", "12"}}, "cost") == 12);
    CHECK(pwbench::positive_parameter<std::uint32_t>({{"cost", "4294967295"}}, "cost") ==
          4294967295U);
    for (const auto value : {"", "0", "-1", "12x", "+1", " 1", "999999999999999999999"}) {
        check_throws<std::invalid_argument>(
            [&] { pwbench::positive_parameter<int>({{"cost", value}}, "cost"); });
        check_throws<std::invalid_argument>(
            [&] { pwbench::positive_parameter<std::uint32_t>({{"cost", value}}, "cost"); });
    }
}

void reports_preparation_and_execution_errors() {
    pwbench::Registry registry;
    registry.add(selection, {},
                 [](const pwbench::ExperimentRequest&) -> pwbench::PreparedExperiment {
                     throw std::runtime_error("target preparation failed");
                 });
    check_throws<std::runtime_error>([&] { static_cast<void>(registry.prepare(selection, {})); });
    registry.add({"empty", "other", pwbench::Backend::cpu}, {},
                 [](const pwbench::ExperimentRequest&) { return pwbench::PreparedExperiment{}; });
    check_throws<std::invalid_argument>([&] {
        static_cast<void>(registry.prepare({"empty", "other", pwbench::Backend::cpu}, {}));
    });
    pwbench::PreparedExperiment prepared{"test",
                                         [](const pwbench::RunLimits&) -> pwbench::RunResult {
                                             throw std::runtime_error("source creation failed");
                                         }};
    CHECK(prepared.run().error == "source creation failed");
    CHECK(prepared.run().checks == 0);
    prepared.runner = [](const pwbench::RunLimits&) -> pwbench::RunResult { throw 42; };
    CHECK(prepared.run().error == "unknown execution error");
    prepared.runner = {};
    CHECK(prepared.run().reason == pwbench::StopReason::error);
}

void supports_independent_modules_and_move_only_targets() {
    const auto registry = fixtures::module_registry();
    const auto password = fixtures::bytes("beta");
    const pwbench::ExperimentRequest request{fixtures::view(password), {}};
    const auto length = registry.prepare({"test-length", "test-blocks", pwbench::Backend::cpu},
                                         {request.password, {{"factor", "3"}}});
    CHECK(length.target_hash == "12");
    CHECK(length.run().candidate == password);
    const auto counter = registry.prepare({"test-length", "test-counter", pwbench::Backend::cpu},
                                          {request.password, {{"count", "19"}}});
    CHECK(counter.run().reason == pwbench::StopReason::exhausted);
    CHECK(counter.run().checks == 19);
    CHECK(registry.prepare(selection, request).run().candidate == password);
}

}

int main() {
    return run_tests({
        {"owned repeatable execution after preparation",
         prepares_once_and_owns_repeatable_execution},
        {"unsupported combinations", rejects_unsupported_combinations_before_preparation},
        {"invalid registrations", rejects_invalid_registrations},
        {"parameter collisions", rejects_parameter_collisions_and_invalid_names},
        {"defaults and required parameters", resolves_defaults_required_values_and_empty_values},
        {"positive signed and unsigned parameters",
         positive_parameters_reject_invalid_signed_and_unsigned_values},
        {"preparation and execution errors", reports_preparation_and_execution_errors},
        {"independent modules and move-only targets",
         supports_independent_modules_and_move_only_targets},
    });
}
