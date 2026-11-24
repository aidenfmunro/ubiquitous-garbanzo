#ifndef PWBENCH_TESTS_FIXTURES_HPP
#define PWBENCH_TESTS_FIXTURES_HPP

#include "pwbench/contracts.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace fixtures {

inline pwbench::Candidate bytes(std::string_view text) {
    return {text.begin(), text.end()};
}

inline pwbench::CandidateView view(const pwbench::Candidate& candidate) {
    return {candidate.data(), candidate.size()};
}

// Test stub, not a cryptographic hash.
struct TestAlgorithm {
    static constexpr std::string_view id = "test-sum";
    using digest_type = std::uint64_t;
    struct target_type {
        digest_type digest;
        std::uint64_t salt;
    };

    digest_type hash(pwbench::CandidateView candidate, const target_type& target) const {
        auto digest = target.salt;
        for (std::size_t index = 0; index < candidate.size; ++index) {
            digest += candidate.data[index];
        }
        return digest;
    }

    bool matches(digest_type digest, const target_type& target) const {
        return digest == target.digest;
    }

    target_type make_target(pwbench::CandidateView candidate, std::uint64_t salt = 7) const {
        target_type target{0, salt};
        target.digest = hash(candidate, target);
        return target;
    }
};

inline TestAlgorithm::target_type absent_target() {
    return {std::numeric_limits<std::uint64_t>::max(), 0};
}

class BlockStrategy {
public:
    static constexpr std::string_view id = "test-blocks";
    struct work_type {
        std::vector<pwbench::Candidate> candidates;
        std::size_t position{};

        std::optional<pwbench::CandidateView> next_candidate() {
            if (position == candidates.size()) {
                return std::nullopt;
            }
            return view(candidates[position++]);
        }
    };

    explicit BlockStrategy(std::vector<pwbench::Candidate> candidates, std::size_t block_size = 2)
        : candidates_(std::move(candidates)), block_size_(block_size) {
        if (block_size_ == 0) {
            throw std::invalid_argument("block size must be positive");
        }
    }

    std::optional<work_type> next_work() {
        if (position_ == candidates_.size()) {
            return std::nullopt;
        }
        work_type work;
        const auto count = std::min(block_size_, candidates_.size() - position_);
        work.candidates.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            work.candidates.push_back(std::move(candidates_[position_++]));
        }
        return work;
    }

private:
    std::vector<pwbench::Candidate> candidates_;
    std::size_t block_size_;
    std::size_t position_{};
};

class CounterStrategy {
public:
    static constexpr std::string_view id = "test-counter";
    struct work_type {
        std::uint64_t position;
        std::uint64_t end;
        pwbench::Byte scratch{};

        std::optional<pwbench::CandidateView> next_candidate() {
            if (position == end) {
                return std::nullopt;
            }
            scratch = static_cast<pwbench::Byte>(position++ % 256);
            return pwbench::CandidateView{&scratch, 1};
        }
    };

    explicit CounterStrategy(std::uint64_t count, std::uint64_t block_size = 64)
        : count_(count), block_size_(block_size) {
        if (block_size == 0) {
            throw std::invalid_argument("block size must be positive");
        }
    }

    std::optional<work_type> next_work() {
        if (position_ == count_) {
            return std::nullopt;
        }
        const auto begin = position_;
        position_ += std::min(block_size_, count_ - position_);
        return work_type{begin, position_};
    }

private:
    std::uint64_t count_;
    std::uint64_t block_size_;
    std::uint64_t position_{};
};

static_assert(pwbench::CpuAlgorithm<TestAlgorithm>);
static_assert(pwbench::HostStrategy<BlockStrategy>);
static_assert(pwbench::HostStrategy<CounterStrategy>);

}

#endif
