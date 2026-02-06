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


// Custom test event listener for clean, intuitive output
class CleanTestListener : public ::testing::EmptyTestEventListener {
public:
    // Called before a test starts
    void OnTestStart(const ::testing::TestInfo& test_info) override {
        std::cout << "\n\n";
        std::cout << "##############################################################" << std::endl;
        std::cout << "➜ TEST START: " << test_info.name() << std::endl;
        std::cout << "##############################################################" << std::endl;
    }

    // Called after a test ends
    void OnTestEnd(const ::testing::TestInfo& test_info) override {
        std::cout << "--------------------------------------------------------------" << std::endl;
        if (test_info.result()->Passed()) {
            std::cout << "✔ PASSED: " << test_info.name()
                      << " (" << test_info.result()->elapsed_time() << " ms)"
                      << std::endl;
        } else {
            std::cout << "✘ FAILED: " << test_info.name()
                      << " (" << test_info.result()->elapsed_time() << " ms)"
                      << std::endl;
        }
    }

    // Called before each test suite starts
    void OnTestSuiteStart(const ::testing::TestSuite& test_suite) override {
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << " SUITE: " << test_suite.name() << std::endl;
        std::cout << " Tests: " << test_suite.total_test_count() << std::endl;
        std::cout << std::string(60, '=') << std::endl;
    }

    // Called after each test suite ends
    void OnTestSuiteEnd(const ::testing::TestSuite& test_suite) override {
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << " SUITE COMPLETE: " << test_suite.name() << std::endl;
        std::cout << "   Passed: " << test_suite.successful_test_count() << std::endl;
        std::cout << "   Failed: " << test_suite.failed_test_count() << std::endl;
        std::cout << std::string(60, '=') << std::endl;
    }
};

int main(int argc, char** argv) {

    // Disable stdout buffering for immediate output
    std::cout.setf(std::ios::unitbuf);
    std::setvbuf(stdout, NULL, _IONBF, 0);

    std::cout << "\n";
    std::cout << "==============================================================\n";
    std::cout << "             AF CORE INTEGRATION TEST SUITE                   \n";
    std::cout << "==============================================================\n";
    std::cout << std::endl;

    // Print environment configuration
    std::cout << "Configuration:" << std::endl;
    std::cout << "  • gRPC Endpoint: "
              << (std::getenv("GRPC_HOST") ? std::getenv("GRPC_HOST") : "localhost") << ":"
              << (std::getenv("GRPC_PORT") ? std::getenv("GRPC_PORT") : "50051") << std::endl;
    std::cout << "  • Fixtures Dir:  " << (std::getenv("FIXTURES_PATH") ? std::getenv("FIXTURES_PATH") : "./fixtures") << std::endl;
    std::cout << "  • Results Dir:   " << (std::getenv("RESULTS_PATH") ? std::getenv("RESULTS_PATH") : "./results") << std::endl;
    std::cout << std::endl;

    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);

    // Get the test event listeners
    ::testing::TestEventListeners& listeners =
        ::testing::UnitTest::GetInstance()->listeners();

    // Remove the default listener to avoid duplicate output if desired,
    // but usually better to keep it for standard xml generation etc.
    // However, if we want full control of console output:
    // delete listeners.Release(listeners.default_result_printer());

    // Append our custom listener
    listeners.Append(new CleanTestListener());

    // Run all tests
    int result = RUN_ALL_TESTS();

    // Print summary
    std::cout << "\n";
    std::cout << "==============================================================\n";
    std::cout << "                 TEST EXECUTION SUMMARY                       \n";
    std::cout << "==============================================================\n";

    return result;
}