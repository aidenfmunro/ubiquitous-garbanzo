#ifndef PWBENCH_CONTRACTS_HPP
#define PWBENCH_CONTRACTS_HPP

#include "pwbench/types.hpp"

#include <concepts>
#include <optional>

namespace pwbench {

template <class T>
concept CpuAlgorithm =
    requires(const T& algorithm, CandidateView candidate, const typename T::target_type& target,
             const typename T::digest_type& digest) {
        { algorithm.hash(candidate, target) } -> std::same_as<typename T::digest_type>;
        { algorithm.matches(digest, target) } -> std::same_as<bool>;
    };

template <class T>
concept HostWork = std::movable<T> && requires(T& work) {
    { work.next_candidate() } -> std::same_as<std::optional<CandidateView>>;
};

template <class T>
concept HostStrategy = requires(T& strategy) {
    requires HostWork<typename T::work_type>;
    { strategy.next_work() } -> std::same_as<std::optional<typename T::work_type>>;
};

}

#endif
