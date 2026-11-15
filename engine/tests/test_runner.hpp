#pragma once

/**
 * @file test_runner.hpp
 * @brief Tiny dependency-free test harness (no framework needed).
 *
 *   TEST(name) { CHECK(cond); CHECK_EQ(a, b); CHECK_NEAR(a, b, eps); }
 */

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

namespace fh_test {

struct TestCase {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

inline int& failures() {
    static int count = 0;
    return count;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) { registry().push_back({ name, std::move(fn) }); }
};

inline void report(bool ok, const char* expr, const char* file, int line) {
    if (!ok) {
        ++failures();
        std::printf("    FAIL %s:%d: %s\n", file, line, expr);
    }
}

} // namespace fh_test

#define FH_CONCAT_INNER(a, b) a##b
#define FH_CONCAT(a, b) FH_CONCAT_INNER(a, b)

#define TEST(name)                                                              \
    static void name();                                                         \
    static ::fh_test::Registrar FH_CONCAT(registrar_, name)(#name, name);       \
    static void name()

#define CHECK(cond) ::fh_test::report(static_cast<bool>(cond), #cond, __FILE__, __LINE__)
#define CHECK_EQ(a, b) ::fh_test::report((a) == (b), #a " == " #b, __FILE__, __LINE__)
#define CHECK_NEAR(a, b, eps) ::fh_test::report(std::fabs((a) - (b)) <= (eps), #a " ~= " #b, __FILE__, __LINE__)
