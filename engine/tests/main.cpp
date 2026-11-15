#include <cstdio>

#include "test_runner.hpp"

int main() {
    int failed_tests = 0;
    for (const auto& test : fh_test::registry()) {
        const int before = fh_test::failures();
        test.fn();
        const bool ok = fh_test::failures() == before;
        if (!ok) ++failed_tests;
        std::printf("[%s] %s\n", ok ? " OK " : "FAIL", test.name);
    }

    const size_t total = fh_test::registry().size();
    std::printf("\n%zu tests, %d failed, %d failed checks\n", total, failed_tests, fh_test::failures());
    return failed_tests == 0 ? 0 : 1;
}
