#ifndef PWBENCH_PARAMETERS_HPP
#define PWBENCH_PARAMETERS_HPP

#include <charconv>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace pwbench {

struct OptionSpec {
    std::string name;
    std::string value_name;
    std::string description;
    std::optional<std::string> default_value;
};

using Parameters = std::map<std::string, std::string, std::less<>>;

inline const std::string& parameter(const Parameters& parameters, std::string_view name) {
    const auto entry = parameters.find(name);
    if (entry == parameters.end()) {
        throw std::invalid_argument("missing value for --" + std::string(name));
    }
    return entry->second;
}

template <class Integer>
Integer positive_parameter(const Parameters& parameters, std::string_view name) {
    const auto& value = parameter(parameters, name);
    Integer number{};
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), number);
    if (error != std::errc{} || end != value.data() + value.size() || number <= 0) {
        throw std::invalid_argument("--" + std::string(name) +
                                    " must be a positive integer in range");
    }
    return number;
}

}

#endif
