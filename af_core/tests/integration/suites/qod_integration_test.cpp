#include "framework/test_context.h"
#include "framework/test_assertions.h"
#include <gtest/gtest.h>

namespace af::test {

/**
 * Test: Create a QoD session successfully
 * Note: This test runs first (alphabetically) and creates a session for subsequent tests
 */
TEST_F(QodIntegrationTest, A_CreateQodSession_Success) {
    // Arrange
    auto correlation_id = GenerateCorrelationId();
    auto request_json = fixture_loader_->LoadFixtureWithVars(
        "qod/qod_create_session.json",
        {{"phoneNumber", "+1234567890"},
        {"networkAccessIdentifier", "123456789@domain.com"},
        {"publicAddress", "10.60.0.1"},
        {"publicPort", "59765"}}
    );

    // Act
    auto response = client_->SendMessage(
        "qod_create_session",
        request_json.dump(),
        correlation_id,
        {{"source", "integration_test"}}
    );

    // Assert
    ASSERT_RESPONSE_SUCCESS(response);
    
    // Use vector instead of initializer list
    std::vector<std::string> required_fields = {"applicationServer", "qosProfile", "sessionId", "duration"};
    ASSERT_RESPONSE_CONTAINS(response, required_fields);
    
    ASSERT_RESPONSE_TIME(response, 5000); // Max 5 seconds

    // Store session ID for use in other tests
    auto response_json = nlohmann::json::parse(response.response_payload);
    QodIntegrationTest::shared_session_id_ = response_json["sessionId"].get<std::string>();

    std::cout << "Created session ID: " << QodIntegrationTest::shared_session_id_ << std::endl;
}

/**
 * Test: Create QoD session with invalid parameters
 * Independent test - can run in any order
 */
TEST_F(QodIntegrationTest, B_CreateQodSession_InvalidQosProfile) {
    // Arrange
    auto correlation_id = GenerateCorrelationId();
    auto request_json = fixture_loader_->LoadFixture("qod/qod_create_session_invalid_qos_profile.json");

    // Act
    auto response = client_->SendMessage(
        "qod_create_session",
        request_json.dump(),
        correlation_id
    );

    // Assert
    ASSERT_GRPC_ERROR(response, grpc::StatusCode::INVALID_ARGUMENT);
}

/**
 * Test: Extend an existing QoD session
 * Depends on: A_CreateQodSession_Success (runs after due to alphabetical ordering)
 */
TEST_F(QodIntegrationTest, B_ExtendQodSession_Success) {
    // Arrange: Use session from previous test OR create new one
    std::string session_id = QodIntegrationTest::shared_session_id_.empty() 
        ? CreateQodSession() 
        : QodIntegrationTest::shared_session_id_;

    ASSERT_FALSE(session_id.empty()) << "No session ID available for extend test";

    // Prepare extend request
    auto extend_request = fixture_loader_->LoadFixtureWithVars(
        "qod/qod_session_extend_duration.json",
        {{"duration", "600"}} // Extend by 600 seconds
    );

    // Act
    auto response = client_->SendMessage(
        "qod_extend_session",
        extend_request.dump(),
        GenerateCorrelationId(),
        {{"session_id", session_id}} // Add session id in metadata for routing
    );

    // Assert
    ASSERT_GRPC_OK(response);
    ASSERT_FIELD_EQUALS(response, "sessionId", session_id);
}

/**
 * Test: Get an existing QoD session
 * Depends on: A_CreateQodSession_Success (runs after due to alphabetical ordering)
 */
TEST_F(QodIntegrationTest, C_GetQodSession_Success) {
    // Arrange: Use session from previous test OR create new one
    std::string session_id = QodIntegrationTest::shared_session_id_.empty() 
        ? CreateQodSession() 
        : QodIntegrationTest::shared_session_id_;

    ASSERT_FALSE(session_id.empty()) << "No session ID available for get test";

    // Act
    auto response = client_->SendMessage(
        "qod_get_session",
        "",
        GenerateCorrelationId(),
        {{"session_id", session_id}} // Add session id in metadata for routing
    );

    // Assert
    ASSERT_GRPC_OK(response);
    ASSERT_FIELD_EQUALS(response, "sessionId", session_id);
}

/**
 * Test: Retrieve list of QoD sessions
 * Depends on: A_CreateQodSession_Success (runs after due to alphabetical ordering)
 */
TEST_F(QodIntegrationTest, D_GetQodSessions_Success) {
    // Arrange
    auto request_json = fixture_loader_->LoadFixtureWithVars(
        "qod/qod_retrieve_sessions.json",
        {{"phoneNumber", "+1234567890"},
        {"networkAccessIdentifier", "123456789@domain.com"},
        {"publicAddress", "10.60.0.1"},
        {"publicPort", "59765"}}
    );

    auto correlation_id = GenerateCorrelationId();

    // Act
    auto response = client_->SendMessage(
        "qod_retrieve_sessions",
        request_json.dump(),
        correlation_id,
        {{"auth_type", "user"}} // TODO: remove this
    );

    // Print response for debugging
    std::cout << "Retrieve sessions response: " << response.response_payload << std::endl;

    // Assert
    ASSERT_GRPC_OK(response);
    ASSERT_ARRAY_SIZE(response, "", 1); // Root is array with 1 element
    
    // Check first element using helper
    ASSERT_FIRST_ELEMENT_EQUALS(response, "", "sessionId", QodIntegrationTest::shared_session_id_);
    
    // Or check specific index
    ASSERT_ARRAY_ELEMENT_EQUALS(response, "", 0, "sessionId", QodIntegrationTest::shared_session_id_);
    
    // Check first element contains required fields
    auto required_fields = std::vector<std::string>{"sessionId", "qosProfile", "applicationServer"};
    ASSERT_FIRST_ELEMENT_CONTAINS(response, "", required_fields);

    // Check if array contains any element where sessionId matches
    ASSERT_ARRAY_CONTAINS_WHERE(response, "", "sessionId", QodIntegrationTest::shared_session_id_);
}

/**
 * Test: Delete a QoD session
 * Depends on: A_CreateQodSession_Success (runs last due to alphabetical ordering)
 */
TEST_F(QodIntegrationTest, E_DeleteQodSession_Success) {
    // Arrange
    std::string session_id = QodIntegrationTest::shared_session_id_.empty() 
        ? CreateQodSession() 
        : QodIntegrationTest::shared_session_id_;

    ASSERT_FALSE(session_id.empty()) << "No session ID available for delete test";

    std::cout << "Attempting to delete session ID: " << session_id << std::endl;
    // Act
    auto response = client_->SendMessage(
        "qod_delete_session",
        "",
        GenerateCorrelationId(),
        {{"session_id", session_id}} // Add session id in metadata for routing
    );

    // Assert
    ASSERT_GRPC_OK(response);

    // Verify session is deleted by attempting to get it
    auto get_response = client_->SendMessage(
        "qod_get_session",
        "",
        GenerateCorrelationId(),
        {{"session_id", session_id}} // Add session id in metadata for routing
    );
    ASSERT_GRPC_ERROR(get_response, grpc::StatusCode::NOT_FOUND);
}

} // namespace af::test