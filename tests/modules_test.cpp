#include "pwbench/modules/argon2id.hpp"
#include "pwbench/modules/md5.hpp"
#include "pwbench/modules/sha256.hpp"
#include "pwbench/modules/wordlist.hpp"

#include "module_fixtures.hpp"
#include "temp_directory.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <barrier>

namespace {

static_assert(pwbench::Argon2idModule::poll_interval == 1);
static_assert(pwbench::Md5Module::poll_interval == 256);
static_assert(pwbench::Sha256Module::poll_interval == 256);

TEST(ModulesTest, RealModulesWorkWithNewAlgorithmAndStrategy) {
    TempDirectory directory;
    const auto path = directory.write("words.txt", "alpha\nbeta\n");
    const auto password = fixtures::bytes("beta");
    pwbench::Registry registry;
    pwbench::register_cpu<pwbench::Md5Module, fixtures::BlocksModule>(registry);
    pwbench::register_cpu<pwbench::Sha256Module, fixtures::BlocksModule>(registry);
    pwbench::register_cpu<fixtures::LengthModule, pwbench::WordlistModule>(registry);
    const auto md5 = registry.prepare({"md5", "test-blocks", pwbench::Backend::cpu},
                                      {fixtures::view(password), {}});
    EXPECT_EQ(md5.target_hash, "987bcab01b929eb2c07877b224215c92");
    EXPECT_EQ(md5.run().candidate, password);
    const auto sha256 = registry.prepare({"sha256", "test-blocks", pwbench::Backend::cpu},
                                         {fixtures::view(password), {}});
    EXPECT_EQ(sha256.target_hash,
              "f44e64e75f3948e9f73f8dfa94721c4ce8cbb4f265c4790c702b2d41cfbf2753");
    EXPECT_EQ(sha256.run().candidate, password);
    const pwbench::Selection length{"test-length", "wordlist", pwbench::Backend::cpu};
    const auto& options = registry.options(length);
    const auto wordlist =
        std::ranges::find(options, std::string("wordlist"), &pwbench::OptionSpec::name);
    ASSERT_NE(wordlist, options.end());
    ASSERT_TRUE(wordlist->default_value.has_value());
    EXPECT_TRUE(std::filesystem::path(*wordlist->default_value).is_absolute());
    EXPECT_EQ(std::filesystem::path(*wordlist->default_value).filename(),
              "Most-Popular-Letter-Passes.txt");
    const auto prepared = registry.prepare(
        length, {fixtures::view(password), {{"wordlist", path.string()}, {"factor", "4"}}});
    EXPECT_EQ(prepared.target_hash, "16");
    EXPECT_EQ(prepared.run().candidate, password);
    EXPECT_EQ(prepared.run().checks, 2);
}

TEST(ModulesTest, SourceCreationErrorsAreReportedAfterPreparation) {
    TempDirectory directory;
    pwbench::Registry registry;
    pwbench::register_cpu<pwbench::Md5Module, pwbench::WordlistModule>(registry);
    const auto prepared =
        registry.prepare({"md5", "wordlist", pwbench::Backend::cpu},
                         {{}, {{"wordlist", (directory.path() / "missing.txt").string()}}});
    EXPECT_EQ(prepared.target_hash, "d41d8cd98f00b204e9800998ecf8427e");
    const auto result = prepared.run();
    EXPECT_EQ(result.reason, pwbench::StopReason::error);
    EXPECT_NE(result.error.find("cannot open wordlist"), std::string::npos);
    EXPECT_EQ(result.checks, 0);
}

struct StopState {
    std::barrier<> rendezvous{2};
    std::atomic_bool loser_entered{false};
    std::atomic_bool winner_done{false};
};

std::shared_ptr<StopState> stop_state;

struct StopAlgorithm {
    using digest_type = pwbench::Byte;
    struct target_type {
        std::shared_ptr<StopState> state;
    };

    digest_type hash(pwbench::CandidateView candidate, const target_type& target) const {
        const auto value = candidate.data[0];
        if (value == 1) {
            target.state->rendezvous.arrive_and_wait();
        } else if (!target.state->loser_entered.exchange(true)) {
            target.state->rendezvous.arrive_and_wait();
            target.state->winner_done.wait(false);
        }
        return value;
    }

    bool matches(digest_type digest, const target_type&) const {
        return digest == 1;
    }
};

struct StopModule {
    using algorithm_type = StopAlgorithm;
    static constexpr std::string_view id = "test-stop";
    static constexpr std::size_t poll_interval = 1;

    static std::vector<pwbench::OptionSpec> options() {
        return {};
    }

    static pwbench::PreparedAlgorithm<StopAlgorithm> prepare(pwbench::CandidateView,
                                                             const pwbench::Parameters&) {
        return {{}, {stop_state}, "one"};
    }
};

class StopStrategy {
public:
    struct work_type {
        std::shared_ptr<StopState> state;
        pwbench::Byte byte;
        std::size_t position{};

        work_type(std::shared_ptr<StopState> state, pwbench::Byte byte)
            : state(std::move(state)), byte(byte) {}
        work_type(work_type&&) = default;
        work_type& operator=(work_type&&) = default;

        ~work_type() {
            if (state && byte == 1) {
                state->winner_done.store(true);
                state->winner_done.notify_all();
            }
        }

        std::optional<pwbench::CandidateView> next_candidate() {
            if (position++ == 300) {
                return std::nullopt;
            }
            return pwbench::CandidateView{&byte, 1};
        }
    };

    explicit StopStrategy(std::shared_ptr<StopState> state) : state_(std::move(state)) {}

    std::optional<work_type> next_work() {
        if (position_ == 2) {
            return std::nullopt;
        }
        return work_type{state_, static_cast<pwbench::Byte>(position_++ == 0)};
    }

private:
    std::shared_ptr<StopState> state_;
    std::size_t position_{};
};

struct StopStrategyModule {
    using strategy_type = StopStrategy;
    using config_type = std::shared_ptr<StopState>;
    static constexpr std::string_view id = "test-stop-work";

    static std::vector<pwbench::OptionSpec> options() {
        return {};
    }

    static config_type parse(const pwbench::Parameters&) {
        return stop_state;
    }

    static StopStrategy create(const config_type& config) {
        return StopStrategy(config);
    }
};

TEST(ModulesTest, ModulePollIntervalStopsOtherWorkersBeforeNextHash) {
    stop_state = std::make_shared<StopState>();
    pwbench::Registry registry;
    pwbench::register_cpu<StopModule, StopStrategyModule>(registry);
    const auto prepared = registry.prepare({"test-stop", "test-stop-work", pwbench::Backend::cpu},
                                           {{}, {{"threads", "2"}}});
    stop_state.reset();
    const auto result = prepared.run();
    EXPECT_EQ(result.reason, pwbench::StopReason::found);
    EXPECT_EQ(result.candidate, pwbench::Candidate{1});
    EXPECT_EQ(result.checks, 2);
}

}
