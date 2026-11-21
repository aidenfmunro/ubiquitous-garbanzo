#ifndef PWBENCH_TYPES_HPP
#define PWBENCH_TYPES_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pwbench {

using Byte = std::uint8_t;
using Candidate = std::vector<Byte>;

// Borrowed until the next next_candidate() call or destruction of the work item.
struct CandidateView {
    const Byte* data{};
    std::size_t size{};
};

enum class Backend { cpu, cuda };

enum class StopReason { found, exhausted, check_limit, time_limit, error };

constexpr std::string_view to_string(Backend backend) noexcept {
    switch (backend) {
    case Backend::cpu:
        return "cpu";
    case Backend::cuda:
        return "cuda";
    }
    return "unknown";
}

constexpr std::string_view to_string(StopReason reason) noexcept {
    switch (reason) {
    case StopReason::found:
        return "found";
    case StopReason::exhausted:
        return "exhausted";
    case StopReason::check_limit:
        return "check_limit";
    case StopReason::time_limit:
        return "time_limit";
    case StopReason::error:
        return "error";
    }
    return "unknown";
}

struct RunLimits {
    std::optional<std::uint64_t> max_checks;
    std::optional<std::chrono::nanoseconds> time_limit;
};

struct RunResult {
    StopReason reason{StopReason::exhausted};
    std::optional<Candidate> candidate;
    std::uint64_t checks{};
    std::chrono::nanoseconds elapsed{};
    std::string error;
};

inline RunResult error_result(std::string message) {
    RunResult result;
    result.reason = StopReason::error;
    result.error = std::move(message);
    return result;
}

}

#endif
