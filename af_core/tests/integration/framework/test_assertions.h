#pragma once

#include <gtest/gtest.h>
#include "grpc_test_client.h"
#include <nlohmann/json.hpp>

namespace af::test {

/**
 * Custom assertions for gRPC integration tests
 */
class GrpcAssertions {
public:
    /**
     * Assert that the gRPC call was successful
     */
    static void AssertSuccess(const GrpcTestResponse& response);

    /**
     * Assert that the gRPC call failed with a specific status
     */
    static void AssertFailure(
        const GrpcTestResponse& response,
        grpc::StatusCode expected_code
    );

    /**
     * Assert response payload contains expected fields
     */
    static void AssertResponseContains(
        const GrpcTestResponse& response,
        const std::vector<std::string>& expected_fields
    );

    /**
     * Assert response matches JSON schema
     */
    static void AssertResponseMatchesSchema(
        const GrpcTestResponse& response,
        const nlohmann::json& schema
    );

    /**
     * Assert response field has expected value
     */
    static void AssertFieldEquals(
        const GrpcTestResponse& response,
        const std::string& field_path,
        const std::string& expected_value
    );

    /**
     * Assert response array has expected size
     */
    static void AssertArraySize(
        const GrpcTestResponse& response,
        const std::string& array_path,
        size_t expected_size
    );

    /**
     * Assert specific element in array contains expected fields
     */
    static void AssertArrayElementContains(const GrpcTestResponse& response,
        const std::string& array_path,
        size_t index,
        const std::vector<std::string>& expected_fields);

    /**
     * Assert specific element in array has field with expected value
     */
    static void AssertArrayElementEquals(const GrpcTestResponse& response,
                                        const std::string& array_path,
                                        size_t index,
                                        const std::string& field,
                                        const std::string& expected_value);

    /**
     * Assert array matches predicate for all elements
     */
    static void AssertArrayAllMatch(const GrpcTestResponse& response,
        const std::string& array_path,
        std::function<bool(const nlohmann::json&)> predicate);

    /**
     * Assert specific element in array has field with expected value
     */
    static void AssertArrayContainsWhere(const GrpcTestResponse& response,
                                        const std::string& array_path,
                                        const std::string& field,
                                        const std::string& expected_value);

    /**
     * Assert response time is within acceptable range
     */
    static void AssertResponseTime(
        const GrpcTestResponse& response,
        int64_t max_duration_ms
    );

    /**
     * Assert correlation ID matches
     */
    static void AssertCorrelationId(
        const GrpcTestResponse& response,
        const std::string& expected_correlation_id
    );

private:
    static nlohmann::json ParseResponsePayload(const GrpcTestResponse& response);
    static nlohmann::json NavigateJsonPath(const nlohmann::json& json, const std::string& path);
    static nlohmann::json GetArrayElement(const nlohmann::json& json, 
                                         const std::string& array_path, 
                                         size_t index);
};

// Convenience macros
#define ASSERT_GRPC_OK(response) \
    af::test::GrpcAssertions::AssertSuccess(response)

#define ASSERT_GRPC_ERROR(response, code) \
    af::test::GrpcAssertions::AssertFailure(response, code)

#define ASSERT_RESPONSE_SUCCESS(response) \
    af::test::GrpcAssertions::AssertSuccess(response)

#define ASSERT_RESPONSE_TIME(response, max_ms) \
    af::test::GrpcAssertions::AssertResponseTime(response, max_ms)

#define ASSERT_RESPONSE_CONTAINS(response, fields) \
    af::test::GrpcAssertions::AssertResponseContains(response, fields)

#define ASSERT_FIELD_EQUALS(response, field, value) \
    af::test::GrpcAssertions::AssertFieldEquals(response, field, value)

#define ASSERT_ARRAY_SIZE(response, array_path, size) \
    af::test::GrpcAssertions::AssertArraySize(response, array_path, size)

#define ASSERT_ARRAY_ELEMENT_EQUALS(response, path, index, field, value) \
    af::test::GrpcAssertions::AssertArrayElementEquals(response, path, index, field, value)

#define ASSERT_ARRAY_ELEMENT_CONTAINS(response, path, index, fields) \
    af::test::GrpcAssertions::AssertArrayElementContains(response, path, index, fields)

#define ASSERT_ARRAY_ELEMENT_EQUALS(response, path, index, field, value) \
    af::test::GrpcAssertions::AssertArrayElementEquals(response, path, index, field, value)

#define ASSERT_ARRAY_CONTAINS_WHERE(response, path, field, value) \
    af::test::GrpcAssertions::AssertArrayContainsWhere(response, path, field, value)

#define ASSERT_FIRST_ELEMENT_EQUALS(response, path, field, value) \
    af::test::GrpcAssertions::AssertArrayElementEquals(response, path, 0, field, value)

#define ASSERT_FIRST_ELEMENT_CONTAINS(response, path, fields) \
    af::test::GrpcAssertions::AssertArrayElementContains(response, path, 0, fields)

} // namespace af::test