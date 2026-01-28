#include "test_context.h"
#include <random>
#include <thread>
#include <fstream>

namespace af::test {

std::string QodIntegrationTest::shared_session_id_;

// External configuration (set in main.cpp)
extern std::string g_grpc_host;
extern int g_grpc_port;
extern std::string g_fixtures_path;

void IntegrationTestContext::SetUp() {
    // Create gRPC client
    GrpcClientConfig config;
    config.host = g_grpc_host;
    config.port = g_grpc_port;
    config.use_tls = false;
    config.timeout_seconds = 30;

    // Print the configuration for debugging
    std::cout << "gRPC Client Config - Host: " << config.host
              << ", Port: " << config.port
              << ", Use TLS: " << (config.use_tls ? "true" : "false")
              << ", Timeout: " << config.timeout_seconds << "s" << std::endl;

    client_ = std::make_unique<GrpcTestClient>(config);

    // Create fixture loader
    fixture_loader_ = std::make_unique<TestFixtureLoader>(g_fixtures_path);

    // Wait for service to be ready
    ASSERT_TRUE(client_->WaitForReady(30))
        << "gRPC service not ready within timeout";
}

void IntegrationTestContext::TearDown() {
    CleanupTestData();
}

std::string IntegrationTestContext::GenerateCorrelationId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);

    return "test-" + std::to_string(dis(gen)) + "-" +
           std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
}

void IntegrationTestContext::WaitForAsync(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void IntegrationTestContext::CleanupTestData() {
    // Override in subclasses to cleanup specific test data
}

// QoD Integration Test Context

void QodIntegrationTest::SetUp() {
    IntegrationTestContext::SetUp();
    created_sessions_.clear();
}

void QodIntegrationTest::TearDown() {
    // Clean up any sessions created during the test
    for (const auto& session_id : created_sessions_) {
        try {
            DeleteQodSession(session_id);
        } catch (...) {
            // Ignore cleanup errors
        }
    }

    IntegrationTestContext::TearDown();
}

std::string QodIntegrationTest::CreateQodSession(const std::string& fixture_name) {
    auto request_json = fixture_loader_->LoadFixtureWithVars("qod/" + fixture_name,
        {{"phoneNumber", "+1234567890"},
        {"networkAccessIdentifier", "123456789@domain.com"},
        {"publicAddress", "10.60.0.1"},
        {"publicPort", "59765"},
        {"ueId", "10.60.0.1"},
        {"qos_profile", "QOS_M"}}
    );

    // Request is logged by client_->SendMessage

    auto response = client_->SendMessage(
        "qod_create_session",
        request_json.dump(),
        GenerateCorrelationId()
    );

    EXPECT_TRUE(response.success)
        << "Failed to create QoD session: " << response.status.error_message();

    if (response.success) {
        auto json = nlohmann::json::parse(response.response_payload);
        std::string session_id = json["sessionId"].get<std::string>();
        created_sessions_.push_back(session_id);
        return session_id;
    }

    return "";
}

void QodIntegrationTest::DeleteQodSession(const std::string& session_id) {
    nlohmann::json delete_request = {
        {"sessionId", session_id}
    };

    auto response = client_->SendMessage(
        "qod_delete_session",
        delete_request.dump(),
        GenerateCorrelationId()
    );

    // Remove from tracking list
    created_sessions_.erase(
        std::remove(created_sessions_.begin(), created_sessions_.end(), session_id),
        created_sessions_.end()
    );
}

GrpcTestResponse QodIntegrationTest::GetQodSession(const std::string& session_id) {
    nlohmann::json get_request = {
        {"sessionId", session_id}
    };

    return client_->SendMessage(
        "qod_get_session",
        get_request.dump(),
        GenerateCorrelationId()
    );
}

} // namespace af::test