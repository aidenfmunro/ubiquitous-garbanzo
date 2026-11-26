#include "temp_directory.hpp"

#ifdef PWBENCH_ENABLE_CUDA
#include "cuda_test_support.hpp"
#endif

#include <gtest/gtest.h>

#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>

#include <cerrno>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

extern char** environ;

namespace {

struct Invocation {
    int code;
    std::string output;
    std::string errors;
};

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot read test output: " + path.string());
    }
    return {std::istreambuf_iterator<char>(input), {}};
}

Invocation invoke(const TempDirectory& directory, std::vector<std::string> arguments) {
    arguments.insert(arguments.begin(), PWBENCH_CLI_PATH);
    std::vector<char*> argv;
    for (auto& argument : arguments) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

    const auto output_path = directory.path() / "stdout.txt";
    const auto errors_path = directory.path() / "stderr.txt";
    posix_spawn_file_actions_t actions;
    int error = posix_spawn_file_actions_init(&actions);
    if (error != 0) {
        throw std::system_error(error, std::generic_category(), "posix_spawn_file_actions_init");
    }
    constexpr int flags = O_WRONLY | O_CREAT | O_TRUNC;
    error =
        posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, output_path.c_str(), flags, 0600);
    if (error == 0) {
        error = posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, errors_path.c_str(),
                                                 flags, 0600);
    }
    pid_t child{};
    if (error == 0) {
        error = posix_spawn(&child, argv.front(), &actions, nullptr, argv.data(), environ);
    }
    posix_spawn_file_actions_destroy(&actions);
    if (error != 0) {
        throw std::system_error(error, std::generic_category(), "launching pwbench");
    }

    int status{};
    pid_t waited;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited == -1 && errno == EINTR);
    if (waited == -1) {
        throw std::system_error(errno, std::generic_category(), "waiting for pwbench");
    }
    auto output = read_file(output_path);
    auto errors = read_file(errors_path);
    if (!WIFEXITED(status)) {
        throw std::runtime_error("pwbench terminated by signal " +
                                 std::to_string(WTERMSIG(status)) + "\n" + output + errors);
    }
    return {WEXITSTATUS(status), std::move(output), std::move(errors)};
}

void expects(const Invocation& result, int exit_code, std::string_view text) {
    SCOPED_TRACE(result.output + result.errors);
    EXPECT_EQ(result.code, exit_code);
    EXPECT_NE((result.output + result.errors).find(text), std::string::npos) << text;
}

class Md5CliTest : public ::testing::Test {
protected:
    TempDirectory directory;
    const std::string words = directory.write("words.txt", "alpha\nbeta\ngamma\n").string();

    Invocation password(std::string value, const std::string& wordlist, std::string threads) {
        return invoke(directory, {"--algorithm", "md5", "--strategy", "wordlist", "--backend",
                                  "cpu", "--password", std::move(value), "--wordlist", wordlist,
                                  "--threads", std::move(threads)});
    }
};

TEST_F(Md5CliTest, FindsPassword) {
    const auto result = password("beta", words, "1");
    expects(result, 0, "status: found");
    EXPECT_NE(result.output.find("target_hash: 987bcab01b929eb2c07877b224215c92"),
              std::string::npos);
}

TEST_F(Md5CliTest, FindsPasswordWithMultipleThreads) {
    const auto result = password("beta", words, "4");
    expects(result, 0, "status: found");
    EXPECT_NE(result.output.find("target_hash: 987bcab01b929eb2c07877b224215c92"),
              std::string::npos);
}

TEST_F(Md5CliTest, ExhaustsWordlistForAbsentPassword) {
    const auto result = password("absent", words, "1");
    expects(result, 1, "status: exhausted");
    EXPECT_NE(result.output.find("target_hash: e5671794bf87ebab2a0d5e0ded530e68"),
              std::string::npos);
}

TEST_F(Md5CliTest, FindsEmptyPassword) {
    const auto result = password("", directory.write("empty-password.txt", "\n").string(), "1");
    expects(result, 0, "status: found");
    EXPECT_NE(result.output.find("target_hash: d41d8cd98f00b204e9800998ecf8427e"),
              std::string::npos);
}

TEST_F(Md5CliTest, MakesNoChecksForEmptyFile) {
    const auto result = password("beta", directory.write("empty-file.txt", "").string(), "1");
    expects(result, 1, "checks: 0");
    EXPECT_NE(result.output.find("target_hash: 987bcab01b929eb2c07877b224215c92"),
              std::string::npos);
}

TEST_F(Md5CliTest, RejectsMissingWordlist) {
    expects(password("beta", (directory.path() / "missing.txt").string(), "1"), 2,
            "cannot open wordlist");
}

TEST_F(Md5CliTest, RejectsZeroThreads) {
    expects(password("beta", words, "0"), 2, "--threads must be a positive integer");
}

TEST_F(Md5CliTest, ShowsHelpOnRequest) {
    const auto result = invoke(directory, {"--help"});
    EXPECT_EQ(result.code, 0) << result.errors;
    EXPECT_NE(result.output.find("Usage: pwbench"), std::string::npos) << result.output;
}

TEST_F(Md5CliTest, ShowsHelpWithoutArguments) {
    const auto result = invoke(directory, {});
    EXPECT_EQ(result.code, 0) << result.errors;
    EXPECT_NE(result.output.find("Usage: pwbench"), std::string::npos) << result.output;
    EXPECT_EQ(result.output.find("target_hash:"), std::string::npos) << result.output;
}

TEST_F(Md5CliTest, ShowsHelpWhenOnlyPasswordIsSpecified) {
    const auto result = invoke(directory, {"--password", "password"});
    EXPECT_EQ(result.code, 0) << result.errors;
    EXPECT_NE(result.output.find("Usage: pwbench"), std::string::npos) << result.output;
    EXPECT_EQ(result.output.find("target_hash:"), std::string::npos) << result.output;
}

TEST_F(Md5CliTest, RejectsPartialSelectionWithHelp) {
    const auto result = invoke(directory, {"--algorithm", "md5", "--password", "password"});
    EXPECT_EQ(result.code, 2) << result.output << result.errors;
    EXPECT_NE(result.errors.find("are all required"), std::string::npos) << result.errors;
    EXPECT_NE(result.errors.find("Usage:"), std::string::npos) << result.errors;
}

TEST_F(Md5CliTest, RejectsMissingPassword) {
    const auto result =
        invoke(directory, {"--algorithm", "md5", "--strategy", "wordlist", "--backend", "cpu"});
    EXPECT_EQ(result.code, 2) << result.output << result.errors;
    EXPECT_NE(result.errors.find("--password is required"), std::string::npos) << result.errors;
}

TEST_F(Md5CliTest, ShowsOptionsRegisteredForMd5) {
    const auto result = invoke(directory, {"--algorithm", "md5", "--help"});
    EXPECT_EQ(result.code, 0) << result.errors;
    EXPECT_NE(result.output.find("--wordlist PATH"), std::string::npos) << result.output;
    EXPECT_EQ(result.output.find("--memory-kib"), std::string::npos) << result.output;
}

class Sha256CliTest : public ::testing::Test {
protected:
    TempDirectory directory;
    const std::string words = directory.write("words.txt", "alpha\nbeta\ngamma\n").string();
};

TEST_F(Sha256CliTest, FindsPassword) {
    const auto result =
        invoke(directory, {"--algorithm", "sha256", "--strategy", "wordlist", "--backend", "cpu",
                           "--password", "beta", "--wordlist", words, "--threads", "4"});
    expects(result, 0, "status: found\ncandidate: \"beta\"\n");
    EXPECT_NE(result.output.find(
                  "target_hash: f44e64e75f3948e9f73f8dfa94721c4ce8cbb4f265c4790c702b2d41cfbf2753"),
              std::string::npos);
}

#ifndef PWBENCH_ENABLE_CUDA
TEST_F(Md5CliTest, RejectsUnsupportedBackend) {
    const auto result = invoke(directory, {"--algorithm", "md5", "--strategy", "wordlist",
                                           "--backend", "cuda", "--password", "password"});
    EXPECT_EQ(result.code, 2) << result.output << result.errors;
    EXPECT_NE(result.errors.find("unsupported combination"), std::string::npos) << result.errors;
    EXPECT_EQ(result.output.find("target_hash:"), std::string::npos) << result.output;
}

#endif

#ifdef PWBENCH_ENABLE_CUDA
TEST_F(Md5CliTest, ShowsCudaOptionsWithoutPreparingTarget) {
    const auto result = invoke(directory, {"--algorithm", "md5", "--backend", "cuda", "--help"});
    EXPECT_EQ(result.code, 0) << result.errors;
    EXPECT_NE(result.output.find("--device N"), std::string::npos);
    EXPECT_NE(result.output.find("--batch-size N"), std::string::npos);
    EXPECT_NE(result.output.find("--batch-bytes N"), std::string::npos);
    EXPECT_EQ(result.output.find("--threads N"), std::string::npos);
    EXPECT_EQ(result.output.find("target_hash:"), std::string::npos);
}

class CudaCliTest : public fixtures::CudaTest {
protected:
    TempDirectory directory;
    const std::string words = directory.write("words.txt", "alpha\nbeta\ngamma\n").string();

    Invocation run(std::initializer_list<std::string> arguments) {
        std::vector<std::string> argv{"--algorithm", "md5",  "--strategy", "wordlist",
                                      "--backend",   "cuda", "--wordlist", words};
        argv.insert(argv.end(), arguments);
        return invoke(directory, std::move(argv));
    }
};

TEST_F(CudaCliTest, FindsPasswordAndCountsTheCompletedBatch) {
    const auto result = run({"--password", "beta"});
    expects(result, 0, "status: found\ncandidate: \"beta\"\nchecks: 3\n");
    EXPECT_NE(result.output.find("backend: cuda"), std::string::npos);
    EXPECT_NE(result.output.find("target_hash: 987bcab01b929eb2c07877b224215c92"),
              std::string::npos);
}

TEST_F(CudaCliTest, HonorsBatchSizeAndByteBudget) {
    expects(run({"--password", "beta", "--batch-size", "2"}), 0, "checks: 2\n");
    expects(run({"--password", "beta", "--batch-bytes", "1"}), 0, "checks: 2\n");
}

TEST_F(CudaCliTest, ExhaustsWordlistForAbsentPassword) {
    expects(run({"--password", "absent"}), 1, "status: exhausted\nchecks: 3\n");
}

TEST_F(CudaCliTest, HandlesEmptyPasswordAndEmptyFile) {
    directory.write("words.txt", "\nalpha\n");
    expects(run({"--password", ""}), 0, "candidate: \"\"\nchecks: 2\n");
    directory.write("words.txt", "");
    expects(run({"--password", "beta"}), 1, "checks: 0\n");
}

TEST_F(CudaCliTest, RejectsCpuOptionsAndInvalidBatchSizes) {
    expects(run({"--password", "beta", "--threads", "1"}), 2,
            "unknown argument for selected combination: --threads");
    expects(run({"--password", "beta", "--batch-size", "0"}), 2,
            "--batch-size must be a positive integer");
    expects(run({"--password", "beta", "--device", "-1"}), 2,
            "--device must be a nonnegative integer");
}

TEST_F(CudaCliTest, UnavailableDeviceDoesNotFallBackToCpu) {
    int count{};
    ASSERT_EQ(cudaGetDeviceCount(&count), cudaSuccess);
    const auto result = run({"--password", "beta", "--device", std::to_string(count)});
    expects(result, 2, "cudaSetDevice");
    EXPECT_EQ(result.output.find("backend: cpu"), std::string::npos);
}

class Sha256CudaCliTest : public fixtures::CudaTest {
protected:
    TempDirectory directory;
};

TEST_F(Sha256CudaCliTest, FindsPassword) {
    const auto words = directory.write("words.txt", "alpha\nbeta\ngamma\n");
    const auto result = invoke(directory, {"--algorithm", "sha256", "--strategy", "wordlist",
                                           "--backend", "cuda", "--password", "beta", "--wordlist",
                                           words.string(), "--batch-size", "2"});
    expects(result, 0, "status: found\ncandidate: \"beta\"\nchecks: 2\n");
    EXPECT_NE(result.output.find("backend: cuda"), std::string::npos);
    EXPECT_NE(result.output.find(
                  "target_hash: f44e64e75f3948e9f73f8dfa94721c4ce8cbb4f265c4790c702b2d41cfbf2753"),
              std::string::npos);
}
#endif

class Argon2idCliTest : public ::testing::Test {
protected:
    TempDirectory directory;
    const std::string words =
        directory.write("words.txt", "another-fixture\nbench-fixture\n").string();

    Invocation run(std::initializer_list<std::string> arguments) {
        std::vector<std::string> argv{"--strategy", "wordlist",   "--backend",
                                      "cpu",        "--wordlist", words};
        argv.insert(argv.end(), arguments);
        return invoke(directory, std::move(argv));
    }

    Invocation password(std::string value) {
        return run({"--algorithm", "argon2id", "--memory-kib", "16", "--iterations", "1", "--lanes",
                    "1", "--password", std::move(value)});
    }
};

TEST_F(Argon2idCliTest, FindsPasswordAndPrintsKnownPhcString) {
    expects(password("bench-fixture"), 0,
            "target_hash: $argon2id$v=19$m=16,t=1,p=1$cHdiZW5jaC1hcmdvbjJpZA"
            "$MV5AkayZpGl+DVUrqW9A7X3cN0W4LtlznnWUE8nS4Ks\n"
            "status: found\ncandidate: \"bench-fixture\"\nchecks: 2\n");
}

TEST_F(Argon2idCliTest, ExhaustsWordlistForAbsentPassword) {
    expects(password("missing-fixture"), 1, "status: exhausted\nchecks: 2\n");
}

TEST_F(Argon2idCliTest, RejectsMemoryCostThatIsTooSmall) {
    expects(run({"--algorithm", "argon2id", "--password", "bench-fixture", "--memory-kib", "8",
                 "--lanes", "2"}),
            2, "Argon2id: Memory cost is too small");
}

TEST_F(Argon2idCliTest, RejectsLaneCountOverflow) {
    expects(
        run({"--algorithm", "argon2id", "--password", "bench-fixture", "--lanes", "4294967296"}), 2,
        "--lanes must be a positive integer");
}

TEST_F(Argon2idCliTest, RejectsArgon2idOptionsForMd5) {
    expects(run({"--algorithm", "md5", "--password", "bench-fixture", "--iterations", "1"}), 2,
            "unknown argument for selected combination: --iterations");
}

TEST_F(Argon2idCliTest, FindsPasswordWithMultipleThreads) {
    expects(run({"--algorithm", "argon2id", "--memory-kib", "16", "--iterations", "1", "--lanes",
                 "1", "--password", "bench-fixture", "--threads", "4"}),
            0, "candidate: \"bench-fixture\"");
}

TEST_F(Argon2idCliTest, RejectsUnsupportedCudaCombination) {
    const auto result = invoke(directory, {"--algorithm", "argon2id", "--strategy", "wordlist",
                                           "--backend", "cuda", "--password", "password"});
    expects(result, 2, "unsupported combination");
    EXPECT_EQ(result.output.find("target_hash:"), std::string::npos);
}

TEST_F(Argon2idCliTest, ShowsOptionsRegisteredForArgon2id) {
    const auto result = invoke(directory, {"--algorithm", "argon2id", "--help"});
    EXPECT_EQ(result.code, 0) << result.errors;
    EXPECT_NE(result.output.find("--memory-kib N"), std::string::npos) << result.output;
    EXPECT_NE(result.output.find("--lanes N"), std::string::npos) << result.output;
}

}
