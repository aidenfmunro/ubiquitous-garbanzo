#include "pwbench/cpu_executor.hpp"
#include "pwbench/strategies/wordlist.hpp"

#include "fixtures.hpp"
#include "temp_directory.hpp"

#include <gtest/gtest.h>

#include <limits>

namespace {

static_assert(pwbench::HostStrategy<pwbench::WordlistStrategy>);

std::vector<pwbench::Candidate> read_all(pwbench::WordlistStrategy& strategy) {
    std::vector<pwbench::Candidate> candidates;
    while (auto work = strategy.next_work()) {
        while (auto candidate = work->next_candidate()) {
            candidates.emplace_back(candidate->data, candidate->data + candidate->size);
        }
    }
    return candidates;
}

TEST(WordlistTest, PreservesLinesAcrossBlockBoundaries) {
    TempDirectory directory;
    const auto path = directory.write("words.txt", "alpha\nbeta\ngamma\nlast");
    const std::vector<pwbench::Candidate> expected{
        fixtures::bytes("alpha"), fixtures::bytes("beta"), fixtures::bytes("gamma"),
        fixtures::bytes("last")};
    for (std::size_t block_size = 1; block_size <= 32; ++block_size) {
        SCOPED_TRACE(block_size);
        pwbench::WordlistStrategy strategy(path, block_size);
        EXPECT_EQ(read_all(strategy), expected);
        EXPECT_FALSE(strategy.next_work());
    }
}

TEST(WordlistTest, PreservesEmptyPasswordsSpacesAndLineEndings) {
    TempDirectory directory;
    const auto path = directory.write("words.txt", "\n\r\n  word \r\n\tvalue\nlast\r");
    const std::vector<pwbench::Candidate> expected{
        {}, {}, fixtures::bytes("  word "), fixtures::bytes("\tvalue"), fixtures::bytes("last\r")};
    for (std::size_t block_size = 1; block_size <= 16; ++block_size) {
        SCOPED_TRACE(block_size);
        pwbench::WordlistStrategy strategy(path, block_size);
        EXPECT_EQ(read_all(strategy), expected);
    }
}

TEST(WordlistTest, SupportsEmptyFilesAndTrailingNewlines) {
    TempDirectory directory;
    pwbench::WordlistStrategy empty(directory.write("empty.txt", ""));
    EXPECT_FALSE(empty.next_work());
    EXPECT_FALSE(empty.next_work());

    pwbench::WordlistStrategy terminated(directory.write("terminated.txt", "word\n"), 5);
    EXPECT_EQ(read_all(terminated), std::vector<pwbench::Candidate>{fixtures::bytes("word")});

    pwbench::WordlistStrategy newline(directory.write("newline.txt", "\n"), 1);
    EXPECT_EQ(read_all(newline), std::vector<pwbench::Candidate>{pwbench::Candidate{}});
}

TEST(WordlistTest, SupportsBinaryCandidates) {
    TempDirectory directory;
    const std::string data{'a', '\0', static_cast<char>(255), '\n', 'b'};
    pwbench::WordlistStrategy strategy(directory.write("binary.txt", data), 2);
    const std::vector<pwbench::Candidate> expected{{'a', 0, 255}, {'b'}};
    EXPECT_EQ(read_all(strategy), expected);
}

TEST(WordlistTest, LongLineStaysInOneWorkItem) {
    TempDirectory directory;
    const std::string long_word(4096, 'a');
    pwbench::WordlistStrategy strategy(directory.write("long.txt", long_word + "\nnext\n"), 8);
    auto first = strategy.next_work();
    ASSERT_TRUE(first.has_value());
    const auto candidate = first->next_candidate();
    ASSERT_TRUE(candidate.has_value());
    EXPECT_EQ(candidate->size, long_word.size());
    EXPECT_FALSE(first->next_candidate());
    EXPECT_EQ(read_all(strategy), std::vector<pwbench::Candidate>{fixtures::bytes("next")});
}

TEST(WordlistTest, WorkOwnsBytesAfterSourceIsDestroyed) {
    TempDirectory directory;
    std::optional<pwbench::WordlistWork> first;
    {
        pwbench::WordlistStrategy strategy(directory.write("owned.txt", "first\nsecond\n"), 2);
        first = strategy.next_work();
        ASSERT_TRUE(first.has_value());
        auto second = strategy.next_work();
        ASSERT_TRUE(second.has_value());
        const auto candidate = second->next_candidate();
        ASSERT_TRUE(candidate.has_value());
        EXPECT_EQ(candidate->size, 6);
    }
    const auto candidate = first->next_candidate();
    ASSERT_TRUE(candidate.has_value());
    EXPECT_EQ(pwbench::Candidate(candidate->data, candidate->data + candidate->size),
              fixtures::bytes("first"));
}

TEST(WordlistTest, RejectsInvalidSourcesAndBlockSizes) {
    TempDirectory directory;
    const auto path = directory.write("words.txt", "word\n");
    EXPECT_THROW(
        { pwbench::WordlistStrategy strategy(directory.path() / "missing.txt"); },
        std::runtime_error);
    EXPECT_THROW({ pwbench::WordlistStrategy strategy(path, 0); }, std::invalid_argument);
    EXPECT_THROW(
        { pwbench::WordlistStrategy strategy(path, std::numeric_limits<std::size_t>::max()); },
        std::invalid_argument);
    EXPECT_THROW(
        {
            pwbench::WordlistStrategy strategy(directory.path());
            strategy.next_work();
        },
        std::runtime_error);
}

TEST(WordlistTest, RunsWithTheExistingCpuExecutor) {
    TempDirectory directory;
    const auto path = directory.write("pipeline.txt", "alpha\nbeta\ngamma\n");
    const fixtures::TestAlgorithm algorithm;
    const auto password = fixtures::bytes("beta");
    const auto target = algorithm.make_target(fixtures::view(password));
    pwbench::WordlistStrategy strategy(path, 2);
    const auto result = pwbench::CpuExecutor::run(algorithm, target, strategy, {}, {.threads = 4});
    EXPECT_EQ(result.reason, pwbench::StopReason::found);
    EXPECT_EQ(result.candidate, password);
    EXPECT_GE(result.checks, 1U);
    EXPECT_LE(result.checks, 3U);

    pwbench::WordlistStrategy absent(path, 2);
    const auto exhausted =
        pwbench::CpuExecutor::run(algorithm, fixtures::absent_target(), absent, {}, {.threads = 4});
    EXPECT_EQ(exhausted.reason, pwbench::StopReason::exhausted);
    EXPECT_EQ(exhausted.checks, 3);
}

}
