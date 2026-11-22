#include "pwbench/app/cli.hpp"

#include "module_fixtures.hpp"
#include "test_support.hpp"

#include <sstream>
#include <vector>

namespace {

struct Invocation {
    int code;
    std::string output;
    std::string errors;
};

Invocation invoke(std::vector<std::string> arguments, const pwbench::Registry& registry) {
    arguments.insert(arguments.begin(), "pwbench");
    std::vector<char*> argv;
    for (auto& argument : arguments) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);
    std::ostringstream output;
    std::ostringstream errors;
    const auto code = pwbench::app::run(static_cast<int>(arguments.size()), argv.data(), registry,
                                        output, errors);
    return {code, output.str(), errors.str()};
}

std::vector<std::string> selected(std::vector<std::string> arguments) {
    arguments.insert(arguments.begin(),
                     {"--algorithm", "test-sum", "--strategy", "test-blocks", "--backend", "cpu"});
    return arguments;
}

void runs_registered_modules_with_their_own_options() {
    const auto registry = fixtures::module_registry();
    const auto sum =
        invoke(selected({"--password", "beta", "--salt", "11", "--threads", "2"}), registry);
    CHECK(sum.code == 0);
    CHECK(sum.errors.empty());
    CHECK(sum.output.find("algorithm: test-sum\n") != std::string::npos);
    CHECK(sum.output.find("target_hash: 423\n") != std::string::npos);
    CHECK(sum.output.find("candidate: \"beta\"\n") != std::string::npos);

    const auto length =
        invoke({"--factor", "3", "--second", "beta", "--algorithm", "test-length", "--strategy",
                "test-blocks", "--backend", "cpu", "--password", "beta"},
               registry);
    CHECK(length.code == 0);
    CHECK(length.errors.empty());
    CHECK(length.output.find("target_hash: 12\n") != std::string::npos);

    const auto counter = invoke({"--algorithm", "test-length", "--strategy", "test-counter",
                                 "--backend", "cpu", "--password", "beta", "--count", "17"},
                                registry);
    CHECK(counter.code == 1);
    CHECK(counter.output.find("checks: 17\n") != std::string::npos);
}

void no_selection_shows_help_without_preparation() {
    const auto registry = fixtures::module_registry();
    fixtures::SumModule::preparations = 0;
    for (const auto& arguments : std::vector<std::vector<std::string>>{
             {}, {"--password", "beta"}, {"--threads", "4"}, {"--help"}}) {
        const auto result = invoke(arguments, registry);
        CHECK(result.code == 0);
        CHECK(result.errors.empty());
        CHECK(result.output.find("Usage: pwbench") != std::string::npos);
        CHECK(result.output.find("--algorithm test-sum --strategy test-blocks --backend cpu") !=
              std::string::npos);
        CHECK(result.output.find("--factor N") != std::string::npos);
        CHECK(result.output.find("status:") == std::string::npos);
    }
    CHECK(fixtures::SumModule::preparations == 0);
}

void help_is_generated_from_selected_module_descriptions() {
    const auto registry = fixtures::module_registry();
    const auto result =
        invoke({"--algorithm", "test-length", "--strategy", "test-counter", "--help"}, registry);
    CHECK(result.code == 0);
    CHECK(result.output.find("--factor N") != std::string::npos);
    CHECK(result.output.find("--count N") != std::string::npos);
    CHECK(result.output.find("--salt") == std::string::npos);
    CHECK(result.output.find("--first") == std::string::npos);
    CHECK(result.output.find("test-sum") == std::string::npos);
    const auto unavailable = invoke({"--algorithm", "missing", "--help"}, registry);
    CHECK(unavailable.output.find("No registered combinations match") != std::string::npos);
}

void partial_and_unsupported_selection_never_falls_back() {
    const auto registry = fixtures::module_registry();
    fixtures::SumModule::preparations = 0;
    const std::vector<std::string> cases[]{
        {"--algorithm", "test-sum", "--password", "beta"},
        {"--strategy", "test-blocks", "--password", "beta"},
        {"--backend", "cpu", "--password", "beta"},
        {"--algorithm", "test-sum", "--strategy", "test-blocks", "--password", "beta"},
        {"--algorithm", "missing", "--strategy", "test-blocks", "--backend", "cpu", "--password",
         "beta"},
        {"--algorithm", "test-sum", "--strategy", "missing", "--backend", "cpu", "--password",
         "beta"},
        {"--algorithm", "test-sum", "--strategy", "test-blocks", "--backend", "cuda", "--password",
         "beta"},
        {"--algorithm", "test-sum", "--strategy", "test-blocks", "--backend", "missing",
         "--password", "beta"},
    };
    for (const auto& arguments : cases) {
        const auto result = invoke(arguments, registry);
        CHECK(result.code == 2);
        CHECK(result.output.empty());
        CHECK(result.errors.find("Usage: pwbench") != std::string::npos);
    }
    CHECK(fixtures::SumModule::preparations == 0);
}

void rejects_invalid_arguments_before_hashing() {
    const auto registry = fixtures::module_registry();
    fixtures::SumModule::preparations = 0;
    const std::vector<std::string> cases[]{
        {},
        {"--unknown", "value", "--password", "beta"},
        {"--password"},
        {"--password", "beta", "--threads"},
        {"--password", "beta", "--threads", "0"},
        {"--password", "beta", "--threads", "-1"},
        {"--password", "beta", "--threads", "abc"},
        {"--password", "beta", "--threads", "1x"},
        {"--password", "beta", "--threads", ""},
        {"--password", "beta", "--threads", "999999999999999999999999999999999"},
        {"--password", "beta", "--threads", "1", "--threads", "2"},
        {"--password", "beta", "--password", "alpha"},
        {"--password", "beta", "--algorithm", "test-length"},
        {"--password", "beta", "--factor", "3"},
        {"--password", "beta", "unexpected"},
    };
    for (const auto& arguments : cases) {
        const auto result = invoke(selected(arguments), registry);
        CHECK(result.code == 2);
        CHECK(result.output.empty());
        CHECK(!result.errors.empty());
    }
    CHECK(fixtures::SumModule::preparations == 0);
}

void accepts_an_empty_password() {
    const auto result =
        invoke(selected({"--password", "", "--first", ""}), fixtures::module_registry());
    CHECK(result.code == 0);
    CHECK(result.output.find("candidate: \"\"\n") != std::string::npos);
}

void distinguishes_execution_and_preparation_errors() {
    pwbench::Registry registry;
    const pwbench::Selection selection{"test-sum", "test-blocks", pwbench::Backend::cpu};
    registry.add(selection, {},
                 [](const pwbench::ExperimentRequest&) -> pwbench::PreparedExperiment {
                     throw std::runtime_error("preparation failure");
                 });
    const auto failed = invoke(selected({"--password", "x"}), registry);
    CHECK(failed.code == 2);
    CHECK(failed.output.empty());
    CHECK(failed.errors.find("preparation failure") != std::string::npos);
    pwbench::Registry execution;
    execution.add(selection, {}, [](const pwbench::ExperimentRequest&) {
        return pwbench::PreparedExperiment{"test",
                                           [](const pwbench::RunLimits&) -> pwbench::RunResult {
                                               throw std::runtime_error("execution failure");
                                           }};
    });
    const auto error = invoke(selected({"--password", "x"}), execution);
    CHECK(error.code == 2);
    CHECK(error.output.find("status: error\n") != std::string::npos);
    CHECK(error.errors.find("execution failure") != std::string::npos);
    pwbench::Registry unknown;
    unknown.add(selection, {},
                [](const pwbench::ExperimentRequest&) -> pwbench::PreparedExperiment { throw 42; });
    CHECK(invoke(selected({"--password", "x"}), unknown).errors.find("unknown preparation error") !=
          std::string::npos);
}

}

int main() {
    return run_tests({
        {"module-specific options without CLI changes",
         runs_registered_modules_with_their_own_options},
        {"no selection means help", no_selection_shows_help_without_preparation},
        {"generated module help", help_is_generated_from_selected_module_descriptions},
        {"no implicit selection or fallback", partial_and_unsupported_selection_never_falls_back},
        {"argument validation", rejects_invalid_arguments_before_hashing},
        {"empty password", accepts_an_empty_password},
        {"preparation and execution errors", distinguishes_execution_and_preparation_errors},
    });
}
