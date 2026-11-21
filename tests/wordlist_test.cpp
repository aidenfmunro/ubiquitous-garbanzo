#include "pwbench/cpu_executor.hpp"
#include "pwbench/strategies/wordlist.hpp"

#include "fixtures.hpp"
#include "temp_directory.hpp"
#include "test_support.hpp"

#include <limits>

namespace {

static_assert(pwbench::CpuStrategy<pwbench::WordlistStrategy>);

std::vector<pwbench::Candidate> read_all(pwbench::WordlistStrategy& strategy) {
    std::vector<pwbench::Candidate> candidates;
    while (auto work = strategy.next_work()) {
        while (auto candidate = work->next_candidate()) {
            candidates.emplace_back(candidate->data, candidate->data + candidate->size);
        }
    }
    return candidates;
}

void preserves_lines_across_block_boundaries() {
    TempDirectory directory;
    const auto path = directory.write("words.txt", "alpha\nbeta\ngamma\nlast");
    const std::vector<pwbench::Candidate> expected{
        fixtures::bytes("alpha"), fixtures::bytes("beta"), fixtures::bytes("gamma"),
        fixtures::bytes("last")};
    for (std::size_t block_size = 1; block_size <= 32; ++block_size) {
        pwbench::WordlistStrategy strategy(path, block_size);
        CHECK(read_all(strategy) == expected);
        CHECK(!strategy.next_work());
    }
}

void preserves_empty_passwords_spaces_and_line_endings() {
    TempDirectory directory;
    const auto path = directory.write("words.txt", "\n\r\n  word \r\n\tvalue\nlast\r");
    const std::vector<pwbench::Candidate> expected{
        {}, {}, fixtures::bytes("  word "), fixtures::bytes("\tvalue"), fixtures::bytes("last\r")};
    for (std::size_t block_size = 1; block_size <= 16; ++block_size) {
        pwbench::WordlistStrategy strategy(path, block_size);
        CHECK(read_all(strategy) == expected);
    }
}

void supports_empty_files_and_trailing_newlines() {
    TempDirectory directory;
    pwbench::WordlistStrategy empty(directory.write("empty.txt", ""));
    CHECK(!empty.next_work());
    CHECK(!empty.next_work());

    pwbench::WordlistStrategy terminated(directory.write("terminated.txt", "word\n"), 5);
    CHECK(read_all(terminated) == std::vector<pwbench::Candidate>{fixtures::bytes("word")});

    pwbench::WordlistStrategy newline(directory.write("newline.txt", "\n"), 1);
    CHECK(read_all(newline) == std::vector<pwbench::Candidate>{pwbench::Candidate{}});
}

void supports_binary_candidates() {
    TempDirectory directory;
    const std::string data{'a', '\0', static_cast<char>(255), '\n', 'b'};
    pwbench::WordlistStrategy strategy(directory.write("binary.txt", data), 2);
    const std::vector<pwbench::Candidate> expected{{'a', 0, 255}, {'b'}};
    CHECK(read_all(strategy) == expected);
}

void long_line_stays_in_one_work_item() {
    TempDirectory directory;
    const std::string long_word(4096, 'a');
    pwbench::WordlistStrategy strategy(directory.write("long.txt", long_word + "\nnext\n"), 8);
    auto first = strategy.next_work();
    CHECK(first.has_value());
    const auto candidate = first->next_candidate();
    CHECK(candidate.has_value());
    CHECK(candidate->size == long_word.size());
    CHECK(!first->next_candidate());
    CHECK(read_all(strategy) == std::vector<pwbench::Candidate>{fixtures::bytes("next")});
}

void work_owns_bytes_after_source_is_destroyed() {
    TempDirectory directory;
    std::optional<pwbench::WordlistWork> first;
    {
        pwbench::WordlistStrategy strategy(directory.write("owned.txt", "first\nsecond\n"), 2);
        first = strategy.next_work();
        CHECK(first.has_value());
        auto second = strategy.next_work();
        CHECK(second.has_value());
        const auto candidate = second->next_candidate();
        CHECK(candidate.has_value());
        CHECK(candidate->size == 6);
    }
    const auto candidate = first->next_candidate();
    CHECK(candidate.has_value());
    CHECK(pwbench::Candidate(candidate->data, candidate->data + candidate->size) ==
          fixtures::bytes("first"));
}

void rejects_invalid_sources_and_block_sizes() {
    TempDirectory directory;
    const auto path = directory.write("words.txt", "word\n");
    check_throws<std::runtime_error>(
        [&] { pwbench::WordlistStrategy strategy(directory.path() / "missing.txt"); });
    check_throws<std::invalid_argument>([&] { pwbench::WordlistStrategy strategy(path, 0); });
    check_throws<std::invalid_argument>(
        [&] { pwbench::WordlistStrategy strategy(path, std::numeric_limits<std::size_t>::max()); });
    check_throws<std::runtime_error>([&] {
        pwbench::WordlistStrategy strategy(directory.path());
        strategy.next_work();
    });
}

void runs_with_the_existing_cpu_executor() {
    TempDirectory directory;
    const auto path = directory.write("pipeline.txt", "alpha\nbeta\ngamma\n");
    const fixtures::TestAlgorithm algorithm;
    const auto password = fixtures::bytes("beta");
    const auto target = algorithm.make_target(fixtures::view(password));
    pwbench::WordlistStrategy strategy(path, 2);
    const auto result = pwbench::CpuExecutor::run(algorithm, target, strategy, {}, {.threads = 4});
    CHECK(result.reason == pwbench::StopReason::found);
    CHECK(result.candidate == password);
    CHECK(result.checks >= 1 && result.checks <= 3);

    pwbench::WordlistStrategy absent(path, 2);
    const auto exhausted =
        pwbench::CpuExecutor::run(algorithm, fixtures::absent_target(), absent, {}, {.threads = 4});
    CHECK(exhausted.reason == pwbench::StopReason::exhausted);
    CHECK(exhausted.checks == 3);
}

}

int main() {
    return run_tests({
        {"wordlist block boundaries", preserves_lines_across_block_boundaries},
        {"empty passwords and line endings", preserves_empty_passwords_spaces_and_line_endings},
        {"empty files and trailing newlines", supports_empty_files_and_trailing_newlines},
        {"binary candidates", supports_binary_candidates},
        {"long lines", long_line_stays_in_one_work_item},
        {"wordlist work ownership", work_owns_bytes_after_source_is_destroyed},
        {"invalid wordlist inputs", rejects_invalid_sources_and_block_sizes},
        {"wordlist CPU pipeline", runs_with_the_existing_cpu_executor},
    });
}
