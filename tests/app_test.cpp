#include "pwbench/app/cli.hpp"

#include "test_support.hpp"

#include <sstream>
#include <vector>

namespace {

struct Invocation {
    int code;
    std::string output;
    std::string errors;
};

pwbench::app::PreparedExperiment fixture(const pwbench::app::Options& options) {
    return {{"test-algorithm", "test-strategy", pwbench::Backend::cpu},
            "test-digest",
            [password = options.password](const pwbench::RunLimits&) {
                pwbench::RunResult result;
                result.reason = pwbench::StopReason::found;
                result.candidate = pwbench::Candidate(password.begin(), password.end());
                result.checks = 7;
                return result;
            }};
}

pwbench::app::PreparedExperiment forbidden_factory(const pwbench::app::Options&) {
    throw std::runtime_error("factory must not run");
}

Invocation invoke(std::vector<std::string> arguments,
                  pwbench::app::PrepareExperiment prepare = fixture) {
    arguments.insert(arguments.begin(), "pwbench");
    std::vector<char*> argv;
    for (auto& argument : arguments) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);
    std::ostringstream output;
    std::ostringstream errors;
    const auto code =
        pwbench::app::run(static_cast<int>(arguments.size()), argv.data(), prepare, output, errors);
    return {code, output.str(), errors.str()};
}

void renders_a_prepared_experiment() {
    const auto result = invoke({"--password", "example"});
    CHECK(result.code == 0);
    CHECK(result.errors.empty());
    CHECK(result.output.find("algorithm: test-algorithm\n") != std::string::npos);
    CHECK(result.output.find("target_hash: test-digest\n") != std::string::npos);
    CHECK(result.output.find("candidate: \"example\"\n") != std::string::npos);
    CHECK(result.output.find("checks: 7\n") != std::string::npos);
}

void passes_options_without_algorithm_dependencies() {
    const auto result = invoke({"--password", "word", "--wordlist", "custom.txt", "--threads", "4"},
                               [](const pwbench::app::Options& options) {
                                   CHECK(options.password == "word");
                                   CHECK(options.wordlist == "custom.txt");
                                   CHECK(options.threads == 4);
                                   return fixture(options);
                               });
    CHECK(result.code == 0);
    CHECK(result.errors.empty());
}

void supplies_the_default_wordlist() {
    const auto result = invoke({"--password", "word"}, [](const pwbench::app::Options& options) {
        CHECK(options.wordlist.is_absolute());
        CHECK(options.wordlist.filename() == "Most-Popular-Letter-Passes.txt");
        CHECK(options.threads == 1);
        return fixture(options);
    });
    CHECK(result.code == 0);
}

void accepts_an_empty_password() {
    const auto result = invoke({"--password", ""});
    CHECK(result.code == 0);
    CHECK(result.output.find("candidate: \"\"\n") != std::string::npos);
}

void help_does_not_prepare_an_experiment() {
    const auto result = invoke({"--help"}, forbidden_factory);
    CHECK(result.code == 0);
    CHECK(result.output.find("Usage: pwbench") != std::string::npos);
    CHECK(result.errors.empty());
}

void rejects_invalid_arguments() {
    const std::vector<std::string> cases[]{
        {},
        {"--unknown"},
        {"--password"},
        {"--password", "x", "--wordlist"},
        {"--password", "x", "--threads"},
        {"--password", "x", "--threads", "0"},
        {"--password", "x", "--threads", "-1"},
        {"--password", "x", "--threads", "abc"},
        {"--password", "x", "--threads", "1x"},
        {"--password", "x", "--threads", ""},
        {"--password", "x", "--threads", "999999999999999999999999999999999"},
    };
    for (const auto& arguments : cases) {
        const auto result = invoke(arguments, forbidden_factory);
        CHECK(result.code == 2);
        CHECK(result.output.empty());
        CHECK(!result.errors.empty());
        CHECK(result.errors.find("factory must not run") == std::string::npos);
    }
}

void distinguishes_exhaustion_from_execution_errors() {
    const auto absent = invoke({"--password", "x"}, [](const pwbench::app::Options& options) {
        auto experiment = fixture(options);
        experiment.runner = [](const pwbench::RunLimits&) { return pwbench::RunResult{}; };
        return experiment;
    });
    CHECK(absent.code == 1);
    CHECK(absent.output.find("status: exhausted\n") != std::string::npos);
    CHECK(absent.errors.empty());

    const auto failed = invoke({"--password", "x"}, [](const pwbench::app::Options& options) {
        auto experiment = fixture(options);
        experiment.runner = [](const pwbench::RunLimits&) -> pwbench::RunResult {
            throw std::runtime_error("execution failure");
        };
        return experiment;
    });
    CHECK(failed.code == 2);
    CHECK(failed.output.find("status: error\n") != std::string::npos);
    CHECK(failed.errors.find("execution failure") != std::string::npos);
}

void reports_preparation_errors() {
    const auto missing = invoke({"--password", "x"}, nullptr);
    CHECK(missing.code == 2);
    CHECK(missing.errors.find("experiment factory is not set") != std::string::npos);

    const auto failed = invoke({"--password", "x"}, forbidden_factory);
    CHECK(failed.code == 2);
    CHECK(failed.errors.find("factory must not run") != std::string::npos);
}

}

int main() {
    return run_tests({
        {"application output", renders_a_prepared_experiment},
        {"application module options", passes_options_without_algorithm_dependencies},
        {"default wordlist path", supplies_the_default_wordlist},
        {"empty CLI password", accepts_an_empty_password},
        {"help without preparation", help_does_not_prepare_an_experiment},
        {"invalid CLI arguments", rejects_invalid_arguments},
        {"application exit codes", distinguishes_exhaustion_from_execution_errors},
        {"application preparation errors", reports_preparation_errors},
    });
}
