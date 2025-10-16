#include <gtest/gtest.h>
#include <iostream>
#include <fstream>
#include "framework/test_context.h"

// Global test configuration
namespace af::test {
    std::string g_grpc_host = "192.168.70.141";
    int g_grpc_port = 50051;
    std::string g_fixtures_path = "./fixtures";
}


// Custom test event listener for verbose output
class VerboseTestListener : public ::testing::EmptyTestEventListener {
public:
    // Called before a test starts
    void OnTestStart(const ::testing::TestInfo& test_info) override {
        std::cout << "\n[TEST START] " << test_info.test_suite_name() 
                  << "." << test_info.name() << std::endl;
        std::cout << std::string(70, '=') << std::endl;
    }

    // Called after a test ends
    void OnTestEnd(const ::testing::TestInfo& test_info) override {
        std::cout << std::string(70, '=') << std::endl;
        if (test_info.result()->Passed()) {
            std::cout << "[TEST PASS] " << test_info.test_suite_name() 
                      << "." << test_info.name() 
                      << " (" << test_info.result()->elapsed_time() << " ms)" 
                      << std::endl;
        } else {
            std::cout << "[TEST FAIL] " << test_info.test_suite_name() 
                      << "." << test_info.name() 
                      << " (" << test_info.result()->elapsed_time() << " ms)" 
                      << std::endl;
        }
    }

    // Called before each test suite starts
    void OnTestSuiteStart(const ::testing::TestSuite& test_suite) override {
        std::cout << "\n" << std::string(70, '#') << std::endl;
        std::cout << "# Test Suite: " << test_suite.name() << std::endl;
        std::cout << "# Total Tests: " << test_suite.total_test_count() << std::endl;
        std::cout << std::string(70, '#') << std::endl;
    }

    // Called after each test suite ends
    void OnTestSuiteEnd(const ::testing::TestSuite& test_suite) override {
        std::cout << "\n" << std::string(70, '#') << std::endl;
        std::cout << "# Test Suite Complete: " << test_suite.name() << std::endl;
        std::cout << "# Passed: " << test_suite.successful_test_count() << std::endl;
        std::cout << "# Failed: " << test_suite.failed_test_count() << std::endl;
        std::cout << std::string(70, '#') << std::endl;
    }
};

/**
 * Custom test event listener for better output
 */
class IntegrationTestListener : public ::testing::EmptyTestEventListener {
    void OnTestStart(const ::testing::TestInfo& test_info) override {
        std::cout << "\n[TEST START] " 
                  << test_info.test_suite_name() << "." 
                  << test_info.name() << std::endl;
    }

    void OnTestEnd(const ::testing::TestInfo& test_info) override {
        if (test_info.result()->Passed()) {
            std::cout << "[TEST PASS] " 
                      << test_info.test_suite_name() << "." 
                      << test_info.name() 
                      << " (" << test_info.result()->elapsed_time() << "ms)"
                      << std::endl;
        } else {
            std::cout << "[TEST FAIL] " 
                      << test_info.test_suite_name() << "." 
                      << test_info.name() << std::endl;
        }
    }
};

int main(int argc, char** argv) {

    // Disable stdout buffering for immediate output
    std::cout.setf(std::ios::unitbuf);
    std::setvbuf(stdout, NULL, _IONBF, 0);
    
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║         AF Core Integration Test Suite                    ║\n";
    std::cout << "║         Version: 1.0.0                                     ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    std::cout << std::endl;
    
    // Print environment configuration
    std::cout << "Configuration:" << std::endl;
    std::cout << "  gRPC Host: " << (std::getenv("GRPC_HOST") ? std::getenv("GRPC_HOST") : "localhost") << std::endl;
    std::cout << "  gRPC Port: " << (std::getenv("GRPC_PORT") ? std::getenv("GRPC_PORT") : "50051") << std::endl;
    std::cout << "  Fixtures:  " << (std::getenv("FIXTURES_PATH") ? std::getenv("FIXTURES_PATH") : "./fixtures") << std::endl;
    std::cout << "  Results:   " << (std::getenv("RESULTS_PATH") ? std::getenv("RESULTS_PATH") : "./results") << std::endl;
    std::cout << std::endl;
    
    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);
    
    // Get the test event listeners
    ::testing::TestEventListeners& listeners = 
        ::testing::UnitTest::GetInstance()->listeners();
    
    // Add custom verbose listener
    listeners.Append(new VerboseTestListener());
    
    // Run all tests
    std::cout << "Starting test execution..." << std::endl;
    int result = RUN_ALL_TESTS();
    
    // Print summary
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║         Test Execution Complete                            ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    
    return result;
}