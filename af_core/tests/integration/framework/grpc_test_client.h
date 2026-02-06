#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>
#include <map>
#include "message.grpc.pb.h"

namespace af::test {

/**
 * Configuration for gRPC test client
 */
struct GrpcClientConfig {
    std::string host = "localhost";
    int port = 50051;
    bool use_tls = false;
    std::string ca_cert_path = "";
    int timeout_seconds = 30;
};

/**
 * Response wrapper for gRPC calls
 */
struct GrpcTestResponse {
    bool success;
    grpc::Status status;
    std::string response_payload;
    std::string correlation_id;
    std::map<std::string, std::string> metadata;
    int64_t duration_ms;
};

/**
 * Generic gRPC test client for integration testing
 */
class GrpcTestClient {
public:
    explicit GrpcTestClient(const GrpcClientConfig& config);
    ~GrpcTestClient() = default;

    /**
     * Send a message to the gRPC endpoint
     */
    GrpcTestResponse SendMessage(
        const std::string& message_type,
        const std::string& payload,
        const std::string& correlation_id = "",
        const std::map<std::string, std::string>& metadata = {}
    );

    /**
     * Send a message with a JSON file
     */
    GrpcTestResponse SendMessageFromFile(
        const std::string& message_type,
        const std::string& json_file_path,
        const std::string& correlation_id = "",
        const std::map<std::string, std::string>& metadata = {}
    );

    /**
     * Check if the server is healthy
     */
    bool IsHealthy();

    /**
     * Wait for the server to be ready
     */
    bool WaitForReady(int timeout_seconds = 30);

private:
    GrpcClientConfig config_;
    std::unique_ptr<af::proto::InternalCommunication::Stub> stub_;
    
    std::string EncodePayload(const std::string& json_payload);
    std::string DecodePayload(const std::string& encoded_payload);
    std::shared_ptr<grpc::Channel> CreateChannel();
};

} // namespace af::test