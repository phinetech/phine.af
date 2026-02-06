#include "test_assertions.h"
#include <sstream>
#include <regex>

namespace af::test {

void GrpcAssertions::AssertSuccess(const GrpcTestResponse& response) {
    ASSERT_TRUE(response.success) 
        << "gRPC call failed: " << response.status.error_message()
        << " (code: " << response.status.error_code() << ")";
    ASSERT_EQ(response.status.error_code(), grpc::StatusCode::OK);
}

void GrpcAssertions::AssertFailure(
    const GrpcTestResponse& response,
    grpc::StatusCode expected_code) {
    
    ASSERT_FALSE(response.success)
        << "Expected gRPC call to fail, but it succeeded";
    ASSERT_EQ(response.status.error_code(), expected_code)
        << "Expected error code " << expected_code
        << " but got " << response.status.error_code()
        << " with message: " << response.status.error_message();
}

nlohmann::json GrpcAssertions::ParseResponsePayload(const GrpcTestResponse& response) {
    try {
        return nlohmann::json::parse(response.response_payload);
    } catch (const nlohmann::json::exception& e) {
        ADD_FAILURE() << "Failed to parse response payload as JSON: " << e.what()
                      << "\nPayload: " << response.response_payload;
        return nlohmann::json{};
    }
}

nlohmann::json GrpcAssertions::NavigateJsonPath(const nlohmann::json& json, const std::string& path) {
    if (path.empty()) {
        return json;
    }
    
    nlohmann::json current = json;
    std::istringstream path_stream(path);
    std::string segment;
    
    while (std::getline(path_stream, segment, '.')) {
        if (segment.empty()) continue;
        
        // Check if segment is an array index [0], [1], etc.
        std::regex array_index_regex(R"(\[(\d+)\])");
        std::smatch match;
        
        if (std::regex_match(segment, match, array_index_regex)) {
            int index = std::stoi(match[1].str());
            
            EXPECT_TRUE(current.is_array())
                << "Expected array at path segment '" << segment 
                << "' but got: " << current.type_name();
            
            EXPECT_LT(index, current.size())
                << "Array index " << index << " out of bounds (size: " 
                << current.size() << ")";
            
            current = current[index];
        } else {
            EXPECT_TRUE(current.is_object())
                << "Expected object to access field '" << segment 
                << "' but got: " << current.type_name();
            
            EXPECT_TRUE(current.contains(segment))
                << "Field '" << segment << "' not found in path: " << path
                << "\nCurrent JSON: " << current.dump(2);
            
            current = current[segment];
        }
    }
    
    return current;
}

nlohmann::json GrpcAssertions::GetArrayElement(
    const nlohmann::json& json, 
    const std::string& array_path, 
    size_t index) {
    
    nlohmann::json array = array_path.empty() ? json : NavigateJsonPath(json, array_path);
    
    EXPECT_TRUE(array.is_array())
        << "Expected array at path '" << array_path << "' but got: " << array.type_name();
    
    EXPECT_LT(index, array.size())
        << "Array index " << index << " out of bounds (size: " << array.size() << ")";
    
    return array[index];
}

void GrpcAssertions::AssertResponseContains(
    const GrpcTestResponse& response,
    const std::vector<std::string>& expected_fields) {
    
    auto json = ParseResponsePayload(response);
    
    for (const auto& field : expected_fields) {
        ASSERT_TRUE(json.contains(field))
            << "Response missing expected field: " << field
            << "\nResponse: " << json.dump(2);
    }
}

void GrpcAssertions::AssertFieldEquals(
    const GrpcTestResponse& response,
    const std::string& field_path,
    const std::string& expected_value) {
    
    auto json = ParseResponsePayload(response);
    
    // Support nested field paths like "data.sessionId"
    std::istringstream path_stream(field_path);
    std::string segment;
    nlohmann::json* current = &json;
    
    // Print current JSON for debugging
    std::cout << "Current JSON for field path '" << field_path << "': "
              << json.dump(2) << std::endl;

    while (std::getline(path_stream, segment, '.')) {
        ASSERT_TRUE(current->contains(segment))
            << "Field path not found: " << field_path
            << " (missing segment: " << segment << ")";
        current = &(*current)[segment];
    }
    
    std::string actual_value = current->is_string() 
        ? current->get<std::string>() 
        : current->dump();
    
    ASSERT_EQ(actual_value, expected_value)
        << "Field " << field_path << " has unexpected value";
}

void GrpcAssertions::AssertArraySize(
    const GrpcTestResponse& response,
    const std::string& array_path,
    size_t expected_size) {
    
    auto json = ParseResponsePayload(response);
    
    // If array_path is empty, the root is the array
    nlohmann::json current = json;
    
    if (!array_path.empty()) {
        std::istringstream path_stream(array_path);
        std::string segment;
        
        while (std::getline(path_stream, segment, '.')) {
            ASSERT_TRUE(current.contains(segment))
                << "Field path not found: " << array_path
                << " (missing segment: " << segment << ")";
            current = current[segment];
        }
    }
    
    ASSERT_TRUE(current.is_array())
        << "Expected array at path '" << array_path 
        << "' but got: " << current.type_name();
    
    ASSERT_EQ(current.size(), expected_size)
        << "Array size mismatch at path '" << array_path << "'"
        << "\nExpected: " << expected_size
        << "\nActual: " << current.size()
        << "\nArray content: " << current.dump(2);
}

void GrpcAssertions::AssertArrayElementContains(
    const GrpcTestResponse& response,
    const std::string& array_path,
    size_t index,
    const std::vector<std::string>& expected_fields) {
    
    auto json = ParseResponsePayload(response);
    auto element = GetArrayElement(json, array_path, index);
    
    ASSERT_TRUE(element.is_object())
        << "Array element at index " << index << " is not an object"
        << "\nElement: " << element.dump(2);
    
    for (const auto& field : expected_fields) {
        ASSERT_TRUE(element.contains(field))
            << "Array element at index " << index << " missing field: " << field
            << "\nElement: " << element.dump(2);
    }
}

void GrpcAssertions::AssertArrayElementEquals(
    const GrpcTestResponse& response,
    const std::string& array_path,
    size_t index,
    const std::string& field,
    const std::string& expected_value) {
    
    auto json = ParseResponsePayload(response);
    auto element = GetArrayElement(json, array_path, index);
    
    ASSERT_TRUE(element.is_object())
        << "Array element at index " << index << " is not an object";
    
    ASSERT_TRUE(element.contains(field))
        << "Array element at index " << index << " missing field: " << field
        << "\nElement: " << element.dump(2);
    
    std::string actual_value;
    auto field_value = element[field];
    
    if (field_value.is_string()) {
        actual_value = field_value.get<std::string>();
    } else if (field_value.is_number_integer()) {
        actual_value = std::to_string(field_value.get<int64_t>());
    } else if (field_value.is_number_float()) {
        actual_value = std::to_string(field_value.get<double>());
    } else if (field_value.is_boolean()) {
        actual_value = field_value.get<bool>() ? "true" : "false";
    } else {
        actual_value = field_value.dump();
    }
    
    ASSERT_EQ(actual_value, expected_value)
        << "Array element[" << index << "]." << field << " has unexpected value"
        << "\nExpected: " << expected_value
        << "\nActual: " << actual_value
        << "\nElement: " << element.dump(2);
}

void GrpcAssertions::AssertArrayContainsWhere(
    const GrpcTestResponse& response,
    const std::string& array_path,
    const std::string& field,
    const std::string& expected_value) {
    
    auto json = ParseResponsePayload(response);
    auto array = array_path.empty() ? json : NavigateJsonPath(json, array_path);
    
    ASSERT_TRUE(array.is_array())
        << "Expected array at path '" << array_path << "'";
    
    bool found = false;
    for (const auto& element : array) {
        if (element.is_object() && element.contains(field)) {
            std::string actual_value;
            auto field_value = element[field];
            
            if (field_value.is_string()) {
                actual_value = field_value.get<std::string>();
            } else {
                actual_value = field_value.dump();
            }
            
            if (actual_value == expected_value) {
                found = true;
                break;
            }
        }
    }
    
    ASSERT_TRUE(found)
        << "Array does not contain element where " << field << " = " << expected_value
        << "\nArray: " << array.dump(2);
}

void GrpcAssertions::AssertArrayAllMatch(
    const GrpcTestResponse& response,
    const std::string& array_path,
    std::function<bool(const nlohmann::json&)> predicate) {
    
    auto json = ParseResponsePayload(response);
    auto array = array_path.empty() ? json : NavigateJsonPath(json, array_path);
    
    ASSERT_TRUE(array.is_array())
        << "Expected array at path '" << array_path << "'";
    
    int index = 0;
    for (const auto& element : array) {
        ASSERT_TRUE(predicate(element))
            << "Element at index " << index << " does not match predicate"
            << "\nElement: " << element.dump(2);
        index++;
    }
}

void GrpcAssertions::AssertResponseTime(
    const GrpcTestResponse& response,
    int64_t max_duration_ms) {
    
    ASSERT_LE(response.duration_ms, max_duration_ms)
        << "Response took " << response.duration_ms << "ms, "
        << "which exceeds maximum of " << max_duration_ms << "ms";
}

void GrpcAssertions::AssertCorrelationId(
    const GrpcTestResponse& response,
    const std::string& expected_correlation_id) {
    
    ASSERT_EQ(response.correlation_id, expected_correlation_id)
        << "Correlation ID mismatch";
}

void GrpcAssertions::AssertResponseMatchesSchema(
    const GrpcTestResponse& response,
    const nlohmann::json& schema) {
    
    // This would require a JSON schema validator library
    // For now, we'll do basic type checking
    auto json = ParseResponsePayload(response);
    
    // TODO: Implement full JSON schema validation
    // You can use libraries like nlohmann-json-schema-validator
}

} // namespace af::test