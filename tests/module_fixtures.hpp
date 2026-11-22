#ifndef PWBENCH_TESTS_MODULE_FIXTURES_HPP
#define PWBENCH_TESTS_MODULE_FIXTURES_HPP

#include "pwbench/cpu_module.hpp"

#include "fixtures.hpp"

#include <memory>

namespace fixtures {

struct SumModule {
    using algorithm_type = TestAlgorithm;
    static constexpr std::string_view id = TestAlgorithm::id;
    static constexpr std::size_t poll_interval = 256;
    static inline std::size_t preparations{};

    static std::vector<pwbench::OptionSpec> options() {
        return {{"salt", "N", "Test salt", "7"}};
    }

    static pwbench::PreparedAlgorithm<TestAlgorithm>
    prepare(pwbench::CandidateView password, const pwbench::Parameters& parameters) {
        ++preparations;
        const TestAlgorithm algorithm;
        const auto salt = pwbench::positive_parameter<std::uint64_t>(parameters, "salt");
        const auto target = algorithm.make_target(password, salt);
        return {algorithm, target, std::to_string(target.digest)};
    }
};

struct BlocksModule {
    using strategy_type = BlockStrategy;
    using config_type = std::vector<pwbench::Candidate>;
    static constexpr std::string_view id = BlockStrategy::id;

    static std::vector<pwbench::OptionSpec> options() {
        return {{"first", "TEXT", "First candidate", "alpha"},
                {"second", "TEXT", "Second candidate", "beta"}};
    }

    static config_type parse(const pwbench::Parameters& parameters) {
        return {bytes(pwbench::parameter(parameters, "first")),
                bytes(pwbench::parameter(parameters, "second"))};
    }

    static BlockStrategy create(const config_type& config) {
        return BlockStrategy(config);
    }
};

struct CounterModule {
    using strategy_type = CounterStrategy;
    using config_type = std::uint64_t;
    static constexpr std::string_view id = CounterStrategy::id;

    static std::vector<pwbench::OptionSpec> options() {
        return {{"count", "N", "Number of byte candidates", "10"}};
    }

    static config_type parse(const pwbench::Parameters& parameters) {
        return pwbench::positive_parameter<config_type>(parameters, "count");
    }

    static CounterStrategy create(config_type config) {
        return CounterStrategy(config);
    }
};

struct LengthAlgorithm {
    using digest_type = std::size_t;
    struct target_type {
        std::unique_ptr<std::size_t> expected;
        std::size_t factor;
    };

    digest_type hash(pwbench::CandidateView candidate, const target_type& target) const {
        return candidate.size * target.factor;
    }

    bool matches(digest_type digest, const target_type& target) const {
        return digest == *target.expected;
    }
};

struct LengthModule {
    using algorithm_type = LengthAlgorithm;
    static constexpr std::string_view id = "test-length";
    static constexpr std::size_t poll_interval = 1;

    static std::vector<pwbench::OptionSpec> options() {
        return {{"factor", "N", "Test length factor", "2"}};
    }

    static pwbench::PreparedAlgorithm<LengthAlgorithm>
    prepare(pwbench::CandidateView password, const pwbench::Parameters& parameters) {
        const auto factor = pwbench::positive_parameter<std::size_t>(parameters, "factor");
        const auto expected = password.size * factor;
        return {{}, {std::make_unique<std::size_t>(expected), factor}, std::to_string(expected)};
    }
};

inline pwbench::Registry module_registry() {
    pwbench::Registry registry;
    pwbench::register_cpu<SumModule, BlocksModule>(registry);
    pwbench::register_cpu<SumModule, CounterModule>(registry);
    pwbench::register_cpu<LengthModule, BlocksModule>(registry);
    pwbench::register_cpu<LengthModule, CounterModule>(registry);
    return registry;
}

}

#endif
