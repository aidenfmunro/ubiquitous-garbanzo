#include "pwbench/modules/argon2id.hpp"
#include "pwbench/modules/md5.hpp"
#include "pwbench/modules/wordlist.hpp"

#include "module_fixtures.hpp"
#include "temp_directory.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <atomic>
#include <barrier>

namespace {

static_assert(pwbench::Argon2idModule::poll_interval == 1);
static_assert(pwbench::Md5Module::poll_interval == 256);

void real_modules_work_with_new_algorithm_and_strategy() {
    TempDirectory directory;
    const auto path = directory.write("words.txt", "alpha\nbeta\n");
    const auto password = fixtures::bytes("beta");
    pwbench::Registry registry;
    pwbench::register_cpu<pwbench::Md5Module, fixtures::BlocksModule>(registry);
    pwbench::register_cpu<fixtures::LengthModule, pwbench::WordlistModule>(registry);
    const auto md5 = registry.prepare({"md5", "test-blocks", pwbench::Backend::cpu},
                                      {fixtures::view(password), {}});
    CHECK(md5.target_hash == "987bcab01b929eb2c07877b224215c92");
    CHECK(md5.run().candidate == password);
    const pwbench::Selection length{"test-length", "wordlist", pwbench::Backend::cpu};
    const auto& options = registry.options(length);
    const auto wordlist =
        std::ranges::find(options, std::string("wordlist"), &pwbench::OptionSpec::name);
    CHECK(wordlist != options.end());
    CHECK(wordlist->default_value.has_value());
    CHECK(std::filesystem::path(*wordlist->default_value).is_absolute());
    CHECK(std::filesystem::path(*wordlist->default_value).filename() ==
          "Most-Popular-Letter-Passes.txt");
    const auto prepared = registry.prepare(
        length, {fixtures::view(password), {{"wordlist", path.string()}, {"factor", "4"}}});
    CHECK(prepared.target_hash == "16");
    CHECK(prepared.run().candidate == password);
    CHECK(prepared.run().checks == 2);
}

void source_creation_errors_are_reported_after_preparation() {
    TempDirectory directory;
    pwbench::Registry registry;
    pwbench::register_cpu<pwbench::Md5Module, pwbench::WordlistModule>(registry);
    const auto prepared =
        registry.prepare({"md5", "wordlist", pwbench::Backend::cpu},
                         {{}, {{"wordlist", (directory.path() / "missing.txt").string()}}});
    CHECK(prepared.target_hash == "d41d8cd98f00b204e9800998ecf8427e");
    const auto result = prepared.run();
    CHECK(result.reason == pwbench::StopReason::error);
    CHECK(result.error.find("cannot open wordlist") != std::string::npos);
    CHECK(result.checks == 0);
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

void module_poll_interval_stops_other_workers_before_next_hash() {
    stop_state = std::make_shared<StopState>();
    pwbench::Registry registry;
    pwbench::register_cpu<StopModule, StopStrategyModule>(registry);
    const auto prepared = registry.prepare({"test-stop", "test-stop-work", pwbench::Backend::cpu},
                                           {{}, {{"threads", "2"}}});
    stop_state.reset();
    const auto result = prepared.run();
    CHECK(result.reason == pwbench::StopReason::found);
    CHECK(result.candidate == pwbench::Candidate{1});
    CHECK(result.checks == 2);
}

}

int main() {
    return run_tests({
        {"real modules with new combinations", real_modules_work_with_new_algorithm_and_strategy},
        {"strategy creation errors", source_creation_errors_are_reported_after_preparation},
        {"module polling policy", module_poll_interval_stops_other_workers_before_next_hash},
    });
}
