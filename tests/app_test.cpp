#include "pwbench/app/cli.hpp"

#include "module_fixtures.hpp"

#include <gtest/gtest.h>

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

TEST(AppTest, RunsRegisteredModulesWithTheirOwnOptions) {
    const auto registry = fixtures::module_registry();
    const auto sum =
        invoke(selected({"--password", "beta", "--salt", "11", "--threads", "2"}), registry);
    EXPECT_EQ(sum.code, 0);
    EXPECT_TRUE(sum.errors.empty());
    EXPECT_NE(sum.output.find("algorithm: test-sum\n"), std::string::npos);
    EXPECT_NE(sum.output.find("target_hash: 423\n"), std::string::npos);
    EXPECT_NE(sum.output.find("candidate: \"beta\"\n"), std::string::npos);

    const auto length =
        invoke({"--factor", "3", "--second", "beta", "--algorithm", "test-length", "--strategy",
                "test-blocks", "--backend", "cpu", "--password", "beta"},
               registry);
    EXPECT_EQ(length.code, 0);
    EXPECT_TRUE(length.errors.empty());
    EXPECT_NE(length.output.find("target_hash: 12\n"), std::string::npos);

    const auto counter = invoke({"--algorithm", "test-length", "--strategy", "test-counter",
                                 "--backend", "cpu", "--password", "beta", "--count", "17"},
                                registry);
    EXPECT_EQ(counter.code, 1);
    EXPECT_NE(counter.output.find("checks: 17\n"), std::string::npos);
}

TEST(AppTest, NoSelectionShowsHelpWithoutPreparation) {
    const auto registry = fixtures::module_registry();
    fixtures::SumModule::preparations = 0;
    for (const auto& arguments : std::vector<std::vector<std::string>>{
             {}, {"--password", "beta"}, {"--threads", "4"}, {"--help"}}) {
        SCOPED_TRACE(::testing::PrintToString(arguments));
        const auto result = invoke(arguments, registry);
        EXPECT_EQ(result.code, 0);
        EXPECT_TRUE(result.errors.empty());
        EXPECT_NE(result.output.find("Usage: pwbench"), std::string::npos);
        EXPECT_NE(result.output.find("--algorithm test-sum --strategy test-blocks --backend cpu"),
                  std::string::npos);
        EXPECT_NE(result.output.find("--factor N"), std::string::npos);
        EXPECT_EQ(result.output.find("status:"), std::string::npos);
    }
    EXPECT_EQ(fixtures::SumModule::preparations, 0);
}

TEST(AppTest, HelpIsGeneratedFromSelectedModuleDescriptions) {
    const auto registry = fixtures::module_registry();
    const auto result =
        invoke({"--algorithm", "test-length", "--strategy", "test-counter", "--help"}, registry);
    EXPECT_EQ(result.code, 0);
    EXPECT_NE(result.output.find("--factor N"), std::string::npos);
    EXPECT_NE(result.output.find("--count N"), std::string::npos);
    EXPECT_EQ(result.output.find("--salt"), std::string::npos);
    EXPECT_EQ(result.output.find("--first"), std::string::npos);
    EXPECT_EQ(result.output.find("test-sum"), std::string::npos);
    const auto unavailable = invoke({"--algorithm", "missing", "--help"}, registry);
    EXPECT_NE(unavailable.output.find("No registered combinations match"), std::string::npos);
}

TEST(AppTest, PartialAndUnsupportedSelectionNeverFallsBack) {
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
        SCOPED_TRACE(::testing::PrintToString(arguments));
        const auto result = invoke(arguments, registry);
        EXPECT_EQ(result.code, 2);
        EXPECT_TRUE(result.output.empty());
        EXPECT_NE(result.errors.find("Usage: pwbench"), std::string::npos);
    }
    EXPECT_EQ(fixtures::SumModule::preparations, 0);
}

TEST(AppTest, RejectsInvalidArgumentsBeforeHashing) {
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
        SCOPED_TRACE(::testing::PrintToString(arguments));
        const auto result = invoke(selected(arguments), registry);
        EXPECT_EQ(result.code, 2);
        EXPECT_TRUE(result.output.empty());
        EXPECT_FALSE(result.errors.empty());
    }
    EXPECT_EQ(fixtures::SumModule::preparations, 0);
}

TEST(AppTest, AcceptsAnEmptyPassword) {
    const auto result =
        invoke(selected({"--password", "", "--first", ""}), fixtures::module_registry());
    EXPECT_EQ(result.code, 0);
    EXPECT_NE(result.output.find("candidate: \"\"\n"), std::string::npos);
}

TEST(AppTest, DistinguishesExecutionAndPreparationErrors) {
    pwbench::Registry registry;
    const pwbench::Selection selection{"test-sum", "test-blocks", pwbench::Backend::cpu};
    registry.add(selection, {},
                 [](const pwbench::ExperimentRequest&) -> pwbench::PreparedExperiment {
                     throw std::runtime_error("preparation failure");
                 });
    const auto failed = invoke(selected({"--password", "x"}), registry);
    EXPECT_EQ(failed.code, 2);
    EXPECT_TRUE(failed.output.empty());
    EXPECT_NE(failed.errors.find("preparation failure"), std::string::npos);
    pwbench::Registry execution;
    execution.add(selection, {}, [](const pwbench::ExperimentRequest&) {
        return pwbench::PreparedExperiment{"test",
                                           [](const pwbench::RunLimits&) -> pwbench::RunResult {
                                               throw std::runtime_error("execution failure");
                                           }};
    });
    const auto error = invoke(selected({"--password", "x"}), execution);
    EXPECT_EQ(error.code, 2);
    EXPECT_NE(error.output.find("status: error\n"), std::string::npos);
    EXPECT_NE(error.errors.find("execution failure"), std::string::npos);
    pwbench::Registry unknown;
    unknown.add(selection, {},
                [](const pwbench::ExperimentRequest&) -> pwbench::PreparedExperiment { throw 42; });
    EXPECT_NE(
        invoke(selected({"--password", "x"}), unknown).errors.find("unknown preparation error"),
        std::string::npos);
}

}
