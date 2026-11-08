#pragma once

#include <cmath>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace cavdyn_test {

inline void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

inline double relative_difference(double actual, double expected)
{
    return std::abs(actual - expected) / std::abs(expected);
}

/// Requires |actual − expected| ≤ tolerance·|expected| and reports both values.
inline void require_close(double actual, double expected, double tolerance,
                          const std::string& what)
{
    const double difference = relative_difference(actual, expected);
    if (!(difference <= tolerance)) {
        throw std::runtime_error(what + ": got " + std::to_string(actual) +
                                 ", expected " + std::to_string(expected) +
                                 " (relative difference " + std::to_string(difference) +
                                 ", tolerance " + std::to_string(tolerance) + ")");
    }
}

template <typename Exception, typename Function>
void require_throws(Function&& function, const std::string& message_part,
                    const std::string& what)
{
    try {
        function();
    } catch (const Exception& error) {
        require(std::string(error.what()).find(message_part) != std::string::npos,
                what + ": message '" + error.what() + "' does not mention '" +
                message_part + "'");
        return;
    }
    throw std::runtime_error(what + ": no exception was thrown");
}

using TestCase = std::pair<const char*, void (*)()>;

/// Runs every test, or only the one named by argv[1]. Returns the exit code.
inline int run_tests(int argc, char** argv, const std::vector<TestCase>& tests)
{
    const char* only = argc > 1 ? argv[1] : nullptr;
    int failures = 0;
    int ran = 0;
    for (const auto& [name, function] : tests) {
        if (only && std::strcmp(only, name) != 0) {
            continue;
        }
        ++ran;
        try {
            function();
            std::cout << "PASS " << name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cout << "FAIL " << name << ": " << error.what() << '\n';
        }
    }
    if (ran == 0) {
        std::cerr << "No test named '" << (only ? only : "") << "'\n";
        return 2;
    }
    return failures == 0 ? 0 : 1;
}

} // namespace cavdyn_test
