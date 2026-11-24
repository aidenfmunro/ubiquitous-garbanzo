#include "pwbench/cuda_executor.hpp"

#include "cuda_fixtures.hpp"
#include "cuda_test_support.hpp"
#include "fixtures.hpp"

#include <chrono>
#include <limits>
#include <thread>

namespace {

using CudaExecutorTest = fixtures::CudaTest;
using pwbench::CudaExecutor;
using pwbench::StopReason;

fixtures::CudaSum::target_type target(unsigned sum) {
    return {std::make_unique<unsigned>(sum)};
}

TEST(CudaBatchTest, OwnsBytesAndPreservesEmptyAndBinaryCandidates) {
    pwbench::CudaBatch batch;
    auto value = fixtures::bytes("abc");
    batch.append(fixtures::view(value));
    batch.append({nullptr, 0});
    const pwbench::Candidate binary{0, 255, 0};
    batch.append(fixtures::view(binary));
    value.clear();
    EXPECT_EQ(batch.count(), 3U);
    EXPECT_EQ(std::vector(batch.offsets().begin(), batch.offsets().end()),
              (std::vector<std::uint64_t>{0, 3, 3, 6}));
    EXPECT_EQ(batch.candidate(1).size, 0U);
    EXPECT_EQ(batch.candidate(2).data[1], 255);
    EXPECT_THROW(batch.append({nullptr, 1}), std::invalid_argument);
    EXPECT_THROW(batch.candidate(3), std::out_of_range);
    batch.clear();
    EXPECT_EQ(batch.count(), 0U);
    EXPECT_TRUE(batch.bytes().empty());
    batch.append(fixtures::view(binary));
    EXPECT_EQ(batch.offsets().back(), 3U);
}

TEST(CudaBatchTest, ReusesStorageWithoutExposingPreviousBatchBytes) {
    pwbench::CudaBatch batch;
    const auto original = fixtures::bytes("long previous candidate");
    batch.append(fixtures::view(original));
    const auto* storage = batch.bytes().data();
    batch.clear();
    EXPECT_TRUE(batch.bytes().empty());
    EXPECT_EQ(batch.offsets().size(), 1U);
    EXPECT_EQ(batch.offsets()[0], 0U);

    const pwbench::Candidate candidate{0, 255};
    batch.append(fixtures::view(candidate));
    batch.append({nullptr, 0});
    EXPECT_EQ(batch.bytes().data(), storage);
    EXPECT_EQ(std::vector(batch.bytes().begin(), batch.bytes().end()), candidate);
    EXPECT_EQ(std::vector(batch.offsets().begin(), batch.offsets().end()),
              (std::vector<std::uint64_t>{0, 2, 2}));
    EXPECT_EQ(batch.candidate(1).size, 0U);
}

TEST(CudaBatchTest, PreservesPayloadAndOffsetsWhenAppendIsRejected) {
    pwbench::CudaBatch batch;
    const pwbench::Candidate candidate{42};
    batch.append(fixtures::view(candidate));
    EXPECT_THROW(batch.append({candidate.data(), std::numeric_limits<std::size_t>::max()}),
                 std::length_error);
    EXPECT_THROW(batch.append({nullptr, 1}), std::invalid_argument);
    EXPECT_EQ(batch.count(), 1U);
    EXPECT_EQ(std::vector(batch.bytes().begin(), batch.bytes().end()), candidate);
    EXPECT_EQ(std::vector(batch.offsets().begin(), batch.offsets().end()),
              (std::vector<std::uint64_t>{0, 1}));
    batch.append(fixtures::view(candidate));
    EXPECT_EQ(batch.count(), 2U);
    EXPECT_EQ(batch.bytes().size(), 2U);
}

TEST(CudaBatchTest, RejectsAllocationSizeOverflowBeforeCallingCuda) {
    pwbench::CudaBuffer<std::uint64_t> buffer;
    EXPECT_THROW(buffer.reserve(std::numeric_limits<std::size_t>::max()), std::length_error);
}

TEST_F(CudaExecutorTest, SupportsCooperativeMultiKernelAlgorithmAndMoveOnlyTarget) {
    fixtures::CounterStrategy source(10);
    const auto result = CudaExecutor::run(fixtures::CudaSum{}, target(4), source);
    EXPECT_EQ(result.reason, StopReason::found) << result.error;
    EXPECT_EQ(result.candidate, (pwbench::Candidate{4}));
    EXPECT_EQ(result.checks, 6U);
}

TEST_F(CudaExecutorTest, CopiesBorrowedViewsBeforeAdvancingTheSource) {
    fixtures::CounterStrategy source(10, 10);
    const auto result = CudaExecutor::run(fixtures::CudaSum{}, target(1), source);
    EXPECT_EQ(result.reason, StopReason::found) << result.error;
    EXPECT_EQ(result.candidate, (pwbench::Candidate{1}));
    EXPECT_EQ(result.checks, 3U);
}

TEST_F(CudaExecutorTest, PreservesEmptyAndBinaryCandidatesAcrossWorkBoundaries) {
    fixtures::BlockStrategy source({{}, {0, 255}, {1}}, 1);
    const auto result = CudaExecutor::run(fixtures::CudaSum{}, target(255), source);
    EXPECT_EQ(result.reason, StopReason::found) << result.error;
    EXPECT_EQ(result.candidate, (pwbench::Candidate{0, 255}));
    EXPECT_EQ(result.checks, 3U);
}

TEST_F(CudaExecutorTest, HandlesBatchesOfOnlyEmptyCandidates) {
    fixtures::BlockStrategy source({{}, {}, {}}, 1);
    const auto result = CudaExecutor::run(fixtures::CudaSum{}, target(0), source);
    EXPECT_EQ(result.reason, StopReason::found) << result.error;
    ASSERT_TRUE(result.candidate);
    EXPECT_TRUE(result.candidate->empty());
    EXPECT_EQ(result.checks, 3U);
}

TEST_F(CudaExecutorTest, ReturnsTheFirstMatchingIndexWithinTheBatch) {
    fixtures::BlockStrategy source({{0, 1}, {1}, {2}}, 1);
    const auto result = CudaExecutor::run(fixtures::CudaSum{}, target(1), source);
    EXPECT_EQ(result.reason, StopReason::found) << result.error;
    EXPECT_EQ(result.candidate, (pwbench::Candidate{0, 1}));
    EXPECT_EQ(result.checks, 3U);
}

TEST_F(CudaExecutorTest, EnforcesCandidateByteBudgetAndRunsOversizedCandidateAlone) {
    const auto long_candidate = fixtures::bytes("oversized");
    fixtures::BlockStrategy source({{1}, {2}, long_candidate, {3}}, 1);
    unsigned sum{};
    for (auto byte : long_candidate) {
        sum += byte;
    }
    const auto result = CudaExecutor::run(fixtures::CudaSum{}, target(sum), source, {},
                                          {.batch_size = 100, .batch_bytes = 2});
    EXPECT_EQ(result.reason, StopReason::found) << result.error;
    EXPECT_EQ(result.candidate, long_candidate);
    EXPECT_EQ(result.checks, 3U);
}

TEST_F(CudaExecutorTest, HonorsExecutorAndAlgorithmBatchCapacities) {
    for (const auto capacity : {1U, 2U, 3U, 7U}) {
        fixtures::CounterStrategy source(10);
        const auto result = CudaExecutor::run(fixtures::CudaSum{.capacity = 20}, target(4), source,
                                              {}, {.batch_size = capacity});
        EXPECT_EQ(result.reason, StopReason::found) << result.error;
        EXPECT_EQ(result.checks, ((5U + capacity - 1) / capacity) * capacity);
    }
}

TEST_F(CudaExecutorTest, EnforcesCheckLimitAndDistinguishesExhaustion) {
    for (const auto limit : {0U, 1U, 5U, 9U, 10U, 11U}) {
        fixtures::CounterStrategy source(10);
        const auto result = CudaExecutor::run(fixtures::CudaSum{}, target(999), source,
                                              {.max_checks = limit, .time_limit = std::nullopt});
        EXPECT_EQ(result.reason, limit < 10 ? StopReason::check_limit : StopReason::exhausted)
            << result.error;
        EXPECT_EQ(result.checks, std::min(limit, 10U));
        EXPECT_FALSE(result.candidate);
    }
}

TEST_F(CudaExecutorTest, MatchAtCheckLimitIsSuccess) {
    fixtures::CounterStrategy source(10);
    const auto result = CudaExecutor::run(fixtures::CudaSum{}, target(4), source,
                                          {.max_checks = 5, .time_limit = std::nullopt});
    EXPECT_EQ(result.reason, StopReason::found) << result.error;
    EXPECT_EQ(result.checks, 5U);
}

TEST_F(CudaExecutorTest, EmptySourceMakesNoChecks) {
    fixtures::CounterStrategy source(0);
    const auto result = CudaExecutor::run(fixtures::CudaSum{}, target(9), source);
    EXPECT_EQ(result.reason, StopReason::exhausted) << result.error;
    EXPECT_EQ(result.checks, 0U);
}

TEST_F(CudaExecutorTest, ZeroTimeLimitDoesNoWork) {
    fixtures::CounterStrategy source(100);
    const auto result =
        CudaExecutor::run(fixtures::CudaSum{}, target(99), source,
                          {.max_checks = std::nullopt, .time_limit = std::chrono::nanoseconds{0}});
    EXPECT_EQ(result.reason, StopReason::time_limit) << result.error;
    EXPECT_EQ(result.checks, 0U);
}

TEST_F(CudaExecutorTest, ChecksDeadlineAfterReadingCandidates) {
    struct SlowSource {
        using work_type = fixtures::CounterStrategy::work_type;
        std::optional<work_type> next_work() {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            return work_type{0, 1};
        }
    } source;
    const auto result = CudaExecutor::run(
        fixtures::CudaSum{}, target(0), source,
        {.max_checks = std::nullopt, .time_limit = std::chrono::milliseconds(10)});
    EXPECT_EQ(result.reason, StopReason::time_limit) << result.error;
    EXPECT_EQ(result.checks, 0U);
}

TEST_F(CudaExecutorTest, MatchFromLaunchedBatchWinsOverDeadline) {
    fixtures::CounterStrategy warmup(1);
    ASSERT_EQ(CudaExecutor::run(fixtures::CudaSum{}, target(0), warmup).reason, StopReason::found);
    fixtures::CounterStrategy source(10);
    const fixtures::CudaSum algorithm{.completion_delay = std::chrono::milliseconds(1100)};
    const auto result =
        CudaExecutor::run(algorithm, target(1), source,
                          {.max_checks = std::nullopt, .time_limit = std::chrono::seconds(1)});
    EXPECT_EQ(result.reason, StopReason::found) << result.error;
    EXPECT_EQ(result.checks, 3U);
    EXPECT_GE(result.elapsed, std::chrono::seconds(1));
}

TEST_F(CudaExecutorTest, RejectsInvalidOptionsAndLimits) {
    for (const auto options :
         {pwbench::CudaOptions{.device = -1}, pwbench::CudaOptions{.batch_size = 0},
          pwbench::CudaOptions{.batch_bytes = 0}}) {
        fixtures::CounterStrategy source(10);
        const auto result = CudaExecutor::run(fixtures::CudaSum{}, target(0), source, {}, options);
        EXPECT_EQ(result.reason, StopReason::error);
        EXPECT_EQ(result.checks, 0U);
    }
    fixtures::CounterStrategy source(10);
    EXPECT_EQ(
        CudaExecutor::run(fixtures::CudaSum{}, target(0), source,
                          {.max_checks = std::nullopt, .time_limit = std::chrono::nanoseconds{-1}})
            .reason,
        StopReason::error);
    EXPECT_EQ(CudaExecutor::run(fixtures::CudaSum{.capacity = 0}, target(0), source).reason,
              StopReason::error);
}

TEST_F(CudaExecutorTest, ReportsUnavailableDeviceAndAllowsNextRun) {
    int count{};
    ASSERT_EQ(cudaGetDeviceCount(&count), cudaSuccess);
    fixtures::CounterStrategy invalid_source(10);
    const auto error =
        CudaExecutor::run(fixtures::CudaSum{}, target(0), invalid_source, {}, {.device = count});
    EXPECT_EQ(error.reason, StopReason::error);
    EXPECT_NE(error.error.find("cudaSetDevice"), std::string::npos);
    fixtures::CounterStrategy valid_source(10);
    EXPECT_EQ(CudaExecutor::run(fixtures::CudaSum{}, target(0), valid_source).reason,
              StopReason::found);
}

TEST_F(CudaExecutorTest, DrainsQueuedWorkBeforeReleasingAlgorithmStorageOnFailure) {
    fixtures::CounterStrategy source(10);
    const auto result =
        CudaExecutor::run(fixtures::CudaSum{.fail_on_launch = 2}, target(99), source);
    EXPECT_EQ(result.reason, StopReason::error);
    EXPECT_EQ(result.error, "test CUDA launch failure");
    EXPECT_EQ(result.checks, 3U);
    EXPECT_FALSE(result.candidate);
}

TEST_F(CudaExecutorTest, FailureTakesPrecedenceOverQueuedMatch) {
    fixtures::CounterStrategy source(10);
    const auto result =
        CudaExecutor::run(fixtures::CudaSum{.fail_on_launch = 2}, target(4), source);
    EXPECT_EQ(result.reason, StopReason::error);
    EXPECT_EQ(result.checks, 3U);
    EXPECT_FALSE(result.candidate);
}

TEST_F(CudaExecutorTest, HandlesEmptyAndMoveOnlyWorkItems) {
    struct Source {
        struct work_type {
            std::unique_ptr<pwbench::Byte> byte;
            bool consumed{};
            std::optional<pwbench::CandidateView> next_candidate() {
                if (!byte || std::exchange(consumed, true)) {
                    return std::nullopt;
                }
                return pwbench::CandidateView{byte.get(), 1};
            }
        };
        unsigned work{};
        std::optional<work_type> next_work() {
            if (work++ == 0) {
                return work_type{};
            }
            if (work == 2) {
                return work_type{std::make_unique<pwbench::Byte>(4)};
            }
            return std::nullopt;
        }
    } source;
    const auto result = CudaExecutor::run(fixtures::CudaSum{}, target(4), source);
    EXPECT_EQ(result.reason, StopReason::found) << result.error;
    EXPECT_EQ(result.candidate, (pwbench::Candidate{4}));
    EXPECT_EQ(result.checks, 1U);
}

TEST_F(CudaExecutorTest, RejectsInvalidMatchIndex) {
    fixtures::CounterStrategy source(10);
    const auto result =
        CudaExecutor::run(fixtures::CudaSum{.invalid_match = true}, target(99), source);
    EXPECT_EQ(result.reason, StopReason::error);
    EXPECT_NE(result.error.find("invalid candidate index"), std::string::npos);
    EXPECT_EQ(result.checks, 3U);
}

TEST_F(CudaExecutorTest, ReportsSourceFailureAndInvalidViews) {
    struct BadSource {
        struct work_type {
            std::optional<pwbench::CandidateView> next_candidate() {
                return std::nullopt;
            }
        };
        std::optional<work_type> next_work() {
            throw std::runtime_error("test source failure");
        }
    } source;
    const auto result = CudaExecutor::run(fixtures::CudaSum{}, target(0), source);
    EXPECT_EQ(result.reason, StopReason::error);
    EXPECT_EQ(result.error, "test source failure");
    struct BadViewSource {
        struct work_type {
            std::optional<pwbench::CandidateView> next_candidate() {
                return pwbench::CandidateView{nullptr, 1};
            }
        };
        std::optional<work_type> next_work() {
            return work_type{};
        }
    } invalid;
    const auto invalid_result = CudaExecutor::run(fixtures::CudaSum{}, target(0), invalid);
    EXPECT_EQ(invalid_result.reason, StopReason::error);
    EXPECT_EQ(invalid_result.error, "nonempty candidate has null data");
}

}
