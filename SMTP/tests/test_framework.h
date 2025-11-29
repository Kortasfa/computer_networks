#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <iostream>
#include <string>
#include <vector>
#include <functional>

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define BOLD    "\033[1m"

#define TEST(name) void name()
#define ASSERT_TRUE(condition) \
    if (!(condition)) { \
        std::cerr << RED << "  ✗ FAILED: " << RESET << #condition << " at line " << __LINE__ << std::endl; \
        TestFramework::getInstance().recordFailure(); \
        return; \
    }

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) { \
        std::cerr << RED << "  ✗ FAILED: " << RESET << #a << " == " << #b \
                  << " (got " << (a) << " vs " << (b) << ") at line " << __LINE__ << std::endl; \
        TestFramework::getInstance().recordFailure(); \
        return; \
    }

#define ASSERT_NE(a, b) \
    if ((a) == (b)) { \
        std::cerr << RED << "  ✗ FAILED: " << RESET << #a << " != " << #b \
                  << " at line " << __LINE__ << std::endl; \
        TestFramework::getInstance().recordFailure(); \
        return; \
    }

#define ASSERT_CONTAINS(str, substr) \
    if ((str).find(substr) == std::string::npos) { \
        std::cerr << RED << "  ✗ FAILED: " << RESET << "String doesn't contain '" << substr \
                  << "' at line " << __LINE__ << std::endl; \
        TestFramework::getInstance().recordFailure(); \
        return; \
    }

#define RUN_TEST(test_func) \
    std::cout << CYAN << "Running: " << RESET << #test_func << std::endl; \
    TestFramework::getInstance().resetFailureFlag(); \
    test_func(); \
    if (!TestFramework::getInstance().hasFailed()) { \
        std::cout << GREEN << "  ✓ PASSED" << RESET << std::endl; \
        TestFramework::getInstance().recordSuccess(); \
    } \
    std::cout << std::endl;

class TestFramework {
private:
    int total_tests;
    int passed_tests;
    int failed_tests;
    bool current_test_failed;
    
    TestFramework() : total_tests(0), passed_tests(0), failed_tests(0), current_test_failed(false) {}

public:
    static TestFramework& getInstance() {
        static TestFramework instance;
        return instance;
    }
    
    void resetFailureFlag() {
        current_test_failed = false;
    }
    
    void recordFailure() {
        if (!current_test_failed) {
            current_test_failed = true;
            failed_tests++;
            total_tests++;
        }
    }
    
    void recordSuccess() {
        if (!current_test_failed) {
            passed_tests++;
            total_tests++;
        }
    }
    
    bool hasFailed() const {
        return current_test_failed;
    }
    
    void printSummary() const {
        std::cout << "======================================" << std::endl;
        std::cout << BOLD << "Test Summary:" << RESET << std::endl;
        std::cout << "  Total:  " << total_tests << std::endl;
        std::cout << GREEN << "  Passed: " << passed_tests << RESET << std::endl;
        if (failed_tests > 0) {
            std::cout << RED << "  Failed: " << failed_tests << RESET << std::endl;
        } else {
            std::cout << "  Failed: " << failed_tests << std::endl;
        }
        std::cout << "======================================" << std::endl;
        
        if (failed_tests == 0) {
            std::cout << GREEN << BOLD << "All tests passed! ✓" << RESET << std::endl;
        } else {
            std::cout << RED << BOLD << "Some tests failed! ✗" << RESET << std::endl;
        }
    }
    
    int getExitCode() const {
        return (failed_tests > 0) ? 1 : 0;
    }
    
    void reset() {
        total_tests = 0;
        passed_tests = 0;
        failed_tests = 0;
        current_test_failed = false;
    }
};

#endif // TEST_FRAMEWORK_H
