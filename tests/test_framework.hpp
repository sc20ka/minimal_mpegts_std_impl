#ifndef TEST_FRAMEWORK_HPP
#define TEST_FRAMEWORK_HPP

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <cstring>
#include <cstdint>

namespace test {

// ============================================================================
// Test Statistics
// ============================================================================

struct TestStats {
    int total = 0;
    int passed = 0;
    int failed = 0;
};

extern TestStats global_stats;

// ============================================================================
// Assertion Macros
// ============================================================================

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "  ❌ FAILED: " << message << "\n"; \
            std::cerr << "     at " << __FILE__ << ":" << __LINE__ << "\n"; \
            test::global_stats.failed++; \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_EQ(actual, expected, message) \
    do { \
        if ((actual) != (expected)) { \
            std::cerr << "  ❌ FAILED: " << message << "\n"; \
            std::cerr << "     Expected: " << (expected) << "\n"; \
            std::cerr << "     Actual: " << (actual) << "\n"; \
            std::cerr << "     at " << __FILE__ << ":" << __LINE__ << "\n"; \
            test::global_stats.failed++; \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_NE(actual, not_expected, message) \
    do { \
        if ((actual) == (not_expected)) { \
            std::cerr << "  ❌ FAILED: " << message << "\n"; \
            std::cerr << "     Should not be: " << (not_expected) << "\n"; \
            std::cerr << "     at " << __FILE__ << ":" << __LINE__ << "\n"; \
            test::global_stats.failed++; \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_TRUE(condition, message) TEST_ASSERT((condition), message)
#define TEST_ASSERT_FALSE(condition, message) TEST_ASSERT(!(condition), message)

// ============================================================================
// Test Registration
// ============================================================================

struct TestCase {
    std::string name;
    std::function<bool()> func;
};

class TestRegistry {
public:
    static TestRegistry& instance() {
        static TestRegistry reg;
        return reg;
    }

    void registerTest(const std::string& name, std::function<bool()> func) {
        tests_.push_back({name, func});
    }

    int runAll() {
        std::cout << "\n";
        std::cout << "========================================\n";
        std::cout << "  Running Tests\n";
        std::cout << "========================================\n\n";

        for (const auto& test : tests_) {
            global_stats.total++;
            std::cout << "Running: " << test.name << "\n";

            if (test.func()) {
                std::cout << "  ✅ PASSED\n";
                global_stats.passed++;
            } else {
                // Failed message already printed by assertion
            }
            std::cout << "\n";
        }

        std::cout << "========================================\n";
        std::cout << "  Test Results\n";
        std::cout << "========================================\n";
        std::cout << "Total:  " << global_stats.total << "\n";
        std::cout << "Passed: " << global_stats.passed << " ✅\n";
        std::cout << "Failed: " << global_stats.failed << " ❌\n";
        std::cout << "========================================\n\n";

        return global_stats.failed == 0 ? 0 : 1;
    }

private:
    std::vector<TestCase> tests_;
};

// ============================================================================
// Test Registration Helper
// ============================================================================

struct TestRegistrar {
    TestRegistrar(const std::string& name, std::function<bool()> func) {
        TestRegistry::instance().registerTest(name, func);
    }
};

#define TEST(test_name) \
    bool test_##test_name(); \
    test::TestRegistrar registrar_##test_name(#test_name, test_##test_name); \
    bool test_##test_name()

// ============================================================================
// Utility Functions
// ============================================================================

inline void printHex(const uint8_t* data, size_t length, size_t max_bytes = 16) {
    std::cout << "     Hex: ";
    for (size_t i = 0; i < std::min(length, max_bytes); ++i) {
        printf("%02X ", data[i]);
    }
    if (length > max_bytes) {
        std::cout << "...";
    }
    std::cout << "\n";
}

} // namespace test

#endif // TEST_FRAMEWORK_HPP
