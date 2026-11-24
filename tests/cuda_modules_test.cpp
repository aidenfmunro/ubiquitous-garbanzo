#include "pwbench/modules/md5.cuh"
#include "pwbench/modules/wordlist.hpp"

#include "cuda_fixtures.hpp"
#include "cuda_test_support.hpp"
#include "module_fixtures.hpp"
#include "temp_directory.hpp"

#include <future>

namespace {

using CudaModulesTest = fixtures::CudaTest;

pwbench::Registry registry() {
    pwbench::Registry registry;
    pwbench::register_cuda<fixtures::CudaSumModule, fixtures::BlocksModule>(registry);
    pwbench::register_cuda<fixtures::CudaSumModule, fixtures::CounterModule>(registry);
    pwbench::register_cuda<pwbench::Md5CudaModule, fixtures::BlocksModule>(registry);
    pwbench::register_cuda<pwbench::Md5CudaModule, pwbench::WordlistModule>(registry);
    return registry;
}

TEST(CudaOptionsTest, RejectsInvalidParametersBeforeTargetPreparation) {
    auto registry = ::registry();
    const pwbench::Selection selection{"test-cuda-sum", "test-blocks", pwbench::Backend::cuda};
    for (const auto& [name, value] :
         std::vector<std::pair<std::string, std::string>>{{"device", "-1"},
                                                          {"device", "2147483648"},
                                                          {"device", "bad"},
                                                          {"device", ""},
                                                          {"device", "1x"},
                                                          {"batch-size", "0"},
                                                          {"batch-size", "-1"},
                                                          {"batch-bytes", "0"},
                                                          {"batch-bytes", "18446744073709551616"},
                                                          {"threads", "1"}}) {
        const auto before = fixtures::CudaSumModule::preparations;
        EXPECT_THROW(registry.prepare(selection, {{nullptr, 0}, {{name, value}}}),
                     std::invalid_argument);
        EXPECT_EQ(fixtures::CudaSumModule::preparations, before);
    }
}

TEST(CudaOptionsTest, PreparesWithoutInitializingCudaAndHasIndependentSchemas) {
    auto registry = ::registry();
    const pwbench::Selection selection{"test-cuda-sum", "test-blocks", pwbench::Backend::cuda};
    auto experiment = registry.prepare(selection, {{nullptr, 0}, {{"device", "0"}}});
    EXPECT_EQ(experiment.target_hash, "0");
    const auto result = experiment.run({.max_checks = 0, .time_limit = std::nullopt});
    EXPECT_EQ(result.reason, pwbench::StopReason::check_limit);
    EXPECT_EQ(result.checks, 0U);
    EXPECT_TRUE(registry.supports({"md5", "wordlist", pwbench::Backend::cuda}));
    EXPECT_FALSE(registry.supports({"argon2id", "wordlist", pwbench::Backend::cuda}));
    EXPECT_THROW(
        (pwbench::register_cuda<fixtures::CudaSumModule, fixtures::BlocksModule>(registry)),
        std::invalid_argument);
}

TEST_F(CudaModulesTest, OwnsMoveOnlyTargetAndConfigurationAcrossRepeatedRuns) {
    const auto before = fixtures::CudaSumModule::preparations;
    pwbench::PreparedExperiment prepared;
    {
        auto registry = ::registry();
        auto password = fixtures::bytes("beta");
        prepared = registry.prepare({"test-cuda-sum", "test-blocks", pwbench::Backend::cuda},
                                    {fixtures::view(password), {}});
    }
    const auto copy = prepared;
    const pwbench::PreparedExperiment* experiments[]{&prepared, &copy};
    for (const auto* experiment : experiments) {
        const auto result = experiment->run();
        EXPECT_EQ(result.reason, pwbench::StopReason::found) << result.error;
        EXPECT_EQ(result.candidate, fixtures::bytes("beta"));
        EXPECT_EQ(result.checks, 2U);
    }
    EXPECT_EQ(fixtures::CudaSumModule::preparations, before + 1);
}

TEST_F(CudaModulesTest, IndependentlyCombinesAlgorithmsAndStrategies) {
    auto registry = ::registry();
    const pwbench::Candidate password{4};
    const auto counter = registry.prepare({"test-cuda-sum", "test-counter", pwbench::Backend::cuda},
                                          {fixtures::view(password), {}});
    EXPECT_EQ(counter.run().candidate, password);
    const auto text = fixtures::bytes("beta");
    const auto md5 = registry.prepare({"md5", "test-blocks", pwbench::Backend::cuda},
                                      {fixtures::view(text), {}});
    EXPECT_EQ(md5.target_hash, "987bcab01b929eb2c07877b224215c92");
    EXPECT_EQ(md5.run().candidate, text);
}

TEST_F(CudaModulesTest, UsesIndependentSessionsForConcurrentRuns) {
    auto registry = ::registry();
    const auto password = fixtures::bytes("beta");
    auto prepared = registry.prepare({"md5", "test-blocks", pwbench::Backend::cuda},
                                     {fixtures::view(password), {}});
    auto first = std::async(std::launch::async, [&prepared] { return prepared.run(); });
    auto second = std::async(std::launch::async, [&prepared] { return prepared.run(); });
    for (auto* future : {&first, &second}) {
        const auto result = future->get();
        EXPECT_EQ(result.reason, pwbench::StopReason::found) << result.error;
        EXPECT_EQ(result.candidate, password);
    }
}

TEST_F(CudaModulesTest, UsesWordlistAndReportsCreationErrors) {
    TempDirectory directory;
    auto registry = ::registry();
    const auto password = fixtures::bytes("beta");
    auto prepared =
        registry.prepare({"md5", "wordlist", pwbench::Backend::cuda},
                         {fixtures::view(password),
                          {{"wordlist", directory.write("words.txt", "alpha\nbeta\n").string()}}});
    EXPECT_EQ(prepared.run().candidate, password);
    auto missing = registry.prepare(
        {"md5", "wordlist", pwbench::Backend::cuda},
        {fixtures::view(password), {{"wordlist", (directory.path() / "missing.txt").string()}}});
    const auto error = missing.run();
    EXPECT_EQ(error.reason, pwbench::StopReason::error);
    EXPECT_NE(error.error.find("cannot open wordlist"), std::string::npos);
}

}
