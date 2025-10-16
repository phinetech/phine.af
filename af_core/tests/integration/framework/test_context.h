#pragma once

#include "grpc_test_client.h"
#include "test_fixture_loader.h"
#include <gtest/gtest.h>
#include <memory>

namespace af::test {

/**
 * Base test fixture for integration tests
 * Provides common setup and teardown functionality
 */
class IntegrationTestContext : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    // Helper methods available to all tests
    std::unique_ptr<GrpcTestClient> client_;
    std::unique_ptr<TestFixtureLoader> fixture_loader_;
    
    // Test utilities
    std::string GenerateCorrelationId();
    void WaitForAsync(int milliseconds);
    void CleanupTestData();
};

/**
 * Test suite base for QoD integration tests
 */
class QodIntegrationTest : public IntegrationTestContext {
protected:
    void SetUp() override;
    void TearDown() override;
    
    // QoD-specific helpers
    std::string CreateQodSession(const std::string& fixture_name = "qod_create_session.json");
    void DeleteQodSession(const std::string& session_id);
    GrpcTestResponse GetQodSession(const std::string& session_id);

    // Shared session ID across tests (only when tests depend on each other)
    static std::string shared_session_id_;
    
private:
    std::vector<std::string> created_sessions_; // Track for cleanup
};

} // namespace af::test