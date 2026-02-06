#include "grpc_test_client.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <grpcpp/grpcpp.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <nlohmann/json.hpp>
#include "../common/communication/include/message.h"

 // Include generated protobuf/gRPC code
 #include "message.grpc.pb.h"

namespace af::test {

GrpcTestClient::GrpcTestClient(const GrpcClientConfig& config)
    : config_(config) {
    stub_ = af::proto::InternalCommunication::NewStub(CreateChannel());
    // If GrpcClientConfig has a timeout field, set it here
    // timeout_seconds_ = config.timeout_seconds;
}

std::shared_ptr<grpc::Channel> GrpcTestClient::CreateChannel() {
    std::string target = config_.host + ":" + std::to_string(config_.port);

    if (config_.use_tls) {
        grpc::SslCredentialsOptions ssl_opts;
        if (!config_.ca_cert_path.empty()) {
            std::ifstream cert_file(config_.ca_cert_path);
            std::stringstream buffer;
            buffer << cert_file.rdbuf();
            ssl_opts.pem_root_certs = buffer.str();
        }
        return grpc::CreateChannel(target, grpc::SslCredentials(ssl_opts));
    } else {
        return grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
    }
}

std::string GrpcTestClient::EncodePayload(const std::string& json_payload) {
    // Base64 encode the payload
    BIO *bio, *b64;
    BUF_MEM *buffer_ptr;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, json_payload.c_str(), json_payload.length());
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &buffer_ptr);

    std::string encoded(buffer_ptr->data, buffer_ptr->length);
    BIO_free_all(bio);

    return encoded;
}

std::string GrpcTestClient::DecodePayload(const std::string& encoded_payload) {
    // Base64 decode the payload
    BIO *bio, *b64;

    int decode_len = encoded_payload.length();
    std::vector<char> buffer(decode_len);

    bio = BIO_new_mem_buf(encoded_payload.c_str(), -1);
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    int length = BIO_read(bio, buffer.data(), decode_len);
    BIO_free_all(bio);

    return std::string(buffer.data(), length);
}

GrpcTestResponse GrpcTestClient::SendMessage(
    const std::string& message_type,
    const std::string& payload,
    const std::string& correlation_id,
    const std::map<std::string, std::string>& metadata
) {

    std::cerr << "Sending gRPC message of type: " << message_type
              << " with correlation ID: " << correlation_id << std::endl;

    // Create request
    af::proto::InternalMessage request;
    request.set_message_type(message_type);
    request.set_correlation_id(correlation_id);
    request.set_payload(payload.data(), payload.size());

    // Set metadata
    for (const auto& [key, value] : metadata) {
        (*request.mutable_metadata())[key] = value;
    }


    // Set correlation ID

    request.set_correlation_id(correlation_id);

    // Log the request being sent
    std::cout << "\n  [" << message_type << "] Request:" << std::endl;
    std::cout << "  Correlation ID: " << correlation_id << std::endl;

    // Attempt to pretty print JSON if it looks like JSON
    try {
        if (!payload.empty() && (payload.front() == '{' || payload.front() == '[')) {
             auto json = nlohmann::json::parse(payload);
             std::cout << "  Payload: \n" << json.dump(4) << std::endl; // Indent 4 spaces
        } else {
             std::cout << "  Payload: " << payload << std::endl;
        }
    } catch (...) {
        std::cout << "  Payload: " << payload << std::endl;
    }

    if (!metadata.empty()) {
        std::cout << "  Metadata:" << std::endl;
        for (const auto& [key, value] : metadata) {
            std::cout << "    - " << key << ": " << value << std::endl;
        }
    }
    std::cout << std::endl;

    // Set timeout
    grpc::ClientContext context;
    // context.set_deadline(
    //     std::chrono::system_clock::now() + std::chrono::seconds(timeout_seconds_)
    // );

    // Send request
    auto start_time = std::chrono::steady_clock::now();

    af::proto::InternalMessage response;

    grpc::Status status = stub_->SendMessage(&context, request, &response);

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time
    ).count();

    // Log the response
    std::cout << "  [" << message_type << "] Response (" << duration << "ms):" << std::endl;
    std::cout << "  Status: " << (status.ok() ? "SUCCESS" : "FAILED")
              << " (Code: " << status.error_code() << ")" << std::endl;

    if (!status.ok()) {
        std::cout << "  Error: " << status.error_message() << std::endl;
    }
    std::cout << std::endl;

    // Build response object
    GrpcTestResponse test_response;
    test_response.success = status.ok();
    test_response.status = status;
    test_response.response_payload = status.error_message();
    test_response.correlation_id = correlation_id;
    test_response.duration_ms = duration;

    if (status.ok()) {
        // Parse response payload
        test_response.response_payload = response.payload();

        // Extract response metadata
        for (const auto& [key, value] : response.metadata()) {
            test_response.metadata[key] = value;
        }
    }

    return test_response;
}

bool GrpcTestClient::IsHealthy() {

    // Create gRPC request
    af::proto::InternalMessage request;
    request.set_message_type("health_check");

    grpc::ClientContext context;
    af::proto::InternalMessage response;
    grpc::Status status = stub_->SendMessage(&context, request, &response);

    return status.ok();
}

bool GrpcTestClient::WaitForReady(int max_wait_seconds) {
    auto start = std::chrono::steady_clock::now();

    while (true) {
        if (IsHealthy()) {
            return true;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start
        ).count();

        if (elapsed >= max_wait_seconds) {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

} // namespace af::test