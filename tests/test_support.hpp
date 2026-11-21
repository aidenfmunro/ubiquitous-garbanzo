#ifndef PWBENCH_TESTS_TEST_SUPPORT_HPP
#define PWBENCH_TESTS_TEST_SUPPORT_HPP

#include <exception>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) +      \
                                     ": " + #expression);                                          \
        }                                                                                          \
    } while (false)

template <class Exception, class Function>
void check_throws(Function&& function) {
    try {
        std::forward<Function>(function)();
    } catch (const Exception&) {
        return;
    }
    throw std::runtime_error("expected exception was not thrown");
}

using TestCase = std::pair<const char*, void (*)()>;

inline int run_tests(std::initializer_list<TestCase> tests) {
    int failed = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& error) {
            ++failed;
            std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
        } catch (...) {
            ++failed;
            std::cerr << "[FAIL] " << name << ": unknown exception\n";
        }
    }
    return failed == 0 ? 0 : 1;
}

#endif
