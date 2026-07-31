/// @file http_qod_client.cpp
/// @brief HttpQodClient implementation — HTTP client for CAMARA QoD REST API.
///
/// Uses standard HTTP/1.1 to call CAMARA Quality-on-Demand REST API endpoints.

#include "http_qod_client.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <boost/asio.hpp>

#include <chrono>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>

using json = nlohmann::json;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

namespace phine::adapter {

// ─── Construction ───────────────────────────────────────────────────────────

HttpQodClient::HttpQodClient(const QodClientConfig& config)
    : config_(config) {

    base_url_ = config_.af_core_address;

    // Parse URL to extract host and port
    std::string url = base_url_;
    if (url.substr(0, 8) == "https://") {
        use_tls_ = true;
        url = url.substr(8);
    } else if (url.substr(0, 7) == "http://") {
        use_tls_ = false;
        url = url.substr(7);
    }

    // Split host:port
    auto colon_pos = url.find(":");
    if (colon_pos != std::string::npos) {
        host_ = url.substr(0, colon_pos);
        // Strip any trailing path
        auto slash_pos = url.find("/", colon_pos);
        if (slash_pos != std::string::npos) {
            port_ = url.substr(colon_pos + 1, slash_pos - colon_pos - 1);
        } else {
            port_ = url.substr(colon_pos + 1);
        }
    } else {
        auto slash_pos = url.find("/");
        host_ = (slash_pos != std::string::npos) ? url.substr(0, slash_pos) : url;
        port_ = use_tls_ ? "443" : "80";
    }

    spdlog::info("[HttpQodClient] Created — target: {} ({}:{})",
                 base_url_, host_, port_);
}

// ─── Connection readiness ───────────────────────────────────────────────────

bool HttpQodClient::wait_for_ready(int timeout_seconds) {
    auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);

    spdlog::info("[HttpQodClient] Waiting for af_core at {} (timeout {}s)…",
                 base_url_, timeout_seconds);

    while (std::chrono::steady_clock::now() < deadline) {
        try {
            asio::io_context io;
            tcp::resolver resolver(io);
            auto endpoints = resolver.resolve(host_, port_);
            tcp::socket socket(io);
            asio::connect(socket, endpoints);
            socket.close();
            spdlog::info("[HttpQodClient] af_core reachable at {}:{}", host_, port_);
            return true;
        } catch (const std::exception&) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    spdlog::error("[HttpQodClient] Timeout waiting for af_core at {}:{}", host_, port_);
    return false;
}

// ─── Public API ─────────────────────────────────────────────────────────────

Result<SessionInfo> HttpQodClient::create_session(
    const CreateSessionRequest& request) {
    auto payload = serialise_create_request(request);

    spdlog::debug("[HttpQodClient] create_session payload: {}", payload);

    auto result = do_http_request("POST", "/quality-on-demand/v1/sessions", payload);
    auto response = parse_camara_response(result);
    if (!response.success) {
        return Result<SessionInfo>::error(response.error_code, response.error_message);
    }
    return Result<SessionInfo>::ok(parse_session_info(response.value));
}

Result<SessionInfo> HttpQodClient::get_session(const std::string& session_id) {
    auto result = do_http_request("GET", "/quality-on-demand/v1/sessions/" + session_id);
    auto response = parse_camara_response(result);
    if (!response.success) {
        return Result<SessionInfo>::error(response.error_code, response.error_message);
    }
    return Result<SessionInfo>::ok(parse_session_info(response.value));
}

Result<void> HttpQodClient::delete_session(const std::string& session_id) {
    auto result = do_http_request("DELETE", "/quality-on-demand/v1/sessions/" + session_id);
    auto response = parse_camara_response(result);
    if (!response.success) {
        return Result<void>::error(response.error_code, response.error_message);
    }
    return Result<void>::ok();
}

Result<SessionInfo> HttpQodClient::extend_session(
    const std::string& session_id, const ExtendSessionRequest& request) {
    auto payload = serialise_extend_request(request);

    auto result = do_http_request("POST", "/quality-on-demand/v1/sessions/" + session_id + "/extend", payload);
    auto response = parse_camara_response(result);
    if (!response.success) {
        return Result<SessionInfo>::error(response.error_code, response.error_message);
    }
    return Result<SessionInfo>::ok(parse_session_info(response.value));
}

Result<std::vector<SessionInfo>> HttpQodClient::retrieve_sessions(
    const Device& device) {
    auto payload = serialise_device(device);

    auto result = do_http_request("POST", "/quality-on-demand/v1/retrieve-sessions", payload);
    auto response = parse_camara_response(result);
    if (!response.success) {
        return Result<std::vector<SessionInfo>>::error(response.error_code, response.error_message);
    }
    return Result<std::vector<SessionInfo>>::ok(parse_session_list(response.value));
}

// ─── HTTP Request (with retry logic) ────────────────────────────────────────

HttpQodClient::HttpResult HttpQodClient::do_http_request(
    const std::string& method, const std::string& path, const std::string& body) {

    HttpResult result;
    int delay_ms = config_.initial_retry_delay_ms;

    for (int attempt = 1; attempt <= config_.max_retries; ++attempt) {
        spdlog::debug("[HttpQodClient] {} {} (attempt {}/{})",
                      method, path, attempt, config_.max_retries);

        result = {}; // Reset result for this attempt

        try {
            asio::io_context io;
            tcp::resolver resolver(io);
            auto endpoints = resolver.resolve(host_, port_);

            tcp::socket socket(io);
            asio::connect(socket, endpoints);

            // Build HTTP/1.1 request
            std::ostringstream request_stream;
            request_stream << method << " " << path << " HTTP/1.1\r\n";
            request_stream << "Host: " << host_ << ":" << port_ << "\r\n";
            request_stream << "Accept: application/json\r\n";

            if (!body.empty()) {
                request_stream << "Content-Type: application/json\r\n";
                request_stream << "Content-Length: " << body.size() << "\r\n";
            }

            request_stream << "Connection: close\r\n";
            request_stream << "\r\n";

            if (!body.empty()) {
                request_stream << body;
            }

            std::string request_str = request_stream.str();
            asio::write(socket, asio::buffer(request_str));

            // Read response
            asio::streambuf response_buf;
            boost::system::error_code ec;

            // Read status line
            asio::read_until(socket, response_buf, "\r\n", ec);
            if (ec) {
                result.body = "Failed to read response status: " + ec.message();
                continue; // Retry
            }

            std::istream response_stream(&response_buf);
            std::string http_version;
            int status_code;
            std::string status_message;
            response_stream >> http_version >> status_code;
            std::getline(response_stream, status_message);

            result.status_code = status_code;

            // Read headers
            asio::read_until(socket, response_buf, "\r\n\r\n", ec);
            std::string header_line;
            std::size_t content_length = 0;
            bool chunked = false;

            while (std::getline(response_stream, header_line) && header_line != "\r") {
                // Parse content-length
                if (header_line.find("Content-Length:") != std::string::npos ||
                    header_line.find("content-length:") != std::string::npos) {
                    auto colon = header_line.find(":");
                    if (colon != std::string::npos) {
                        content_length = std::stoul(header_line.substr(colon + 2));
                    }
                }
                if (header_line.find("chunked") != std::string::npos) {
                    chunked = true;
                }
            }

            // Read body
            std::ostringstream body_stream;

            // First, consume any data already in the buffer
            if (response_buf.size() > 0) {
                body_stream << &response_buf;
            }

            if (content_length > 0) {
                std::size_t already_read = body_stream.str().size();
                if (already_read < content_length) {
                    std::size_t remaining = content_length - already_read;
                    std::vector<char> buf(remaining);
                    std::size_t total_read = 0;
                    while (total_read < remaining) {
                        std::size_t n = socket.read_some(
                            asio::buffer(buf.data() + total_read, remaining - total_read), ec);
                        if (ec) break;
                        total_read += n;
                    }
                    body_stream.write(buf.data(), static_cast<std::streamsize>(total_read));
                }
            } else if (chunked || content_length == 0) {
                // Read until EOF for chunked or unknown length
                while (asio::read(socket, response_buf,
                                  asio::transfer_at_least(1), ec)) {
                    body_stream << &response_buf;
                }
            }

            result.body = body_stream.str();
            result.success = true;

            // Check if we should retry
            if (result.status_code >= 200 && result.status_code < 300) {
                // Success
                return result;
            }

            // Determine if retryable
            bool retryable = result.status_code == 503 ||
                             result.status_code == 502 ||
                             result.status_code == 504 ||
                             result.status_code == 429;

            if (!retryable) {
                // Non-retryable error, return immediately
                return result;
            }

            spdlog::warn("[HttpQodClient] {} {} failed (attempt {}/{}): status={} body={}",
                         method, path, attempt, config_.max_retries,
                         result.status_code, result.body.substr(0, 200));

        } catch (const std::exception& e) {
            result.body = std::string("HTTP request failed: ") + e.what();
            result.success = false;
            spdlog::warn("[HttpQodClient] {} {} exception (attempt {}/{}): {}",
                         method, path, attempt, config_.max_retries, e.what());
        }

        // Retry with exponential backoff
        if (attempt < config_.max_retries) {
            spdlog::info("[HttpQodClient] Retrying in {}ms…", delay_ms);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            delay_ms *= 2;  // exponential backoff
        }
    }

    // All retries exhausted
    if (!result.success) {
        result.body = "All retry attempts failed";
    }
    return result;
}

// ─── Parse CAMARA Response ──────────────────────────────────────────────────

Result<std::string> HttpQodClient::parse_camara_response(const HttpResult& http_result) {
    if (!http_result.success) {
        return Result<std::string>::error("HTTP_ERROR", http_result.body);
    }

    // Handle successful responses (2xx)
    if (http_result.status_code >= 200 && http_result.status_code < 300) {
        // For DELETE (204 No Content), return empty success
        if (http_result.status_code == 204 || http_result.body.empty()) {
            return Result<std::string>::ok("");
        }
        return Result<std::string>::ok(http_result.body);
    }

    // Handle CAMARA error responses (4xx, 5xx)
    try {
        auto error_json = json::parse(http_result.body);

        // CAMARA standard error format
        if (error_json.contains("code") && error_json.contains("message")) {
            return Result<std::string>::error(
                error_json["code"].get<std::string>(),
                error_json["message"].get<std::string>());
        }

        // Alternative error format
        if (error_json.contains("status") && error_json.contains("message")) {
            std::string code = std::to_string(error_json["status"].get<int>());
            return Result<std::string>::error(code, error_json["message"].get<std::string>());
        }
    } catch (...) {
        // Failed to parse error as JSON
    }

    // Generic HTTP error
    return Result<std::string>::error(
        "HTTP_" + std::to_string(http_result.status_code),
        http_result.body.empty() ? "Request failed" : http_result.body);
}

// ─── Correlation ID ─────────────────────────────────────────────────────────

std::string HttpQodClient::generate_correlation_id() {
    static thread_local std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<uint32_t> dist;

    auto hex = [](uint32_t v, int width) {
        std::ostringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(width) << v;
        return ss.str();
    };

    return hex(dist(gen), 8) + "-" + hex(dist(gen) & 0xFFFF, 4) + "-4" +
           hex(dist(gen) & 0x0FFF, 3) + "-" +
           hex((dist(gen) & 0x3FFF) | 0x8000, 4) + "-" +
           hex(dist(gen), 8) + hex(dist(gen) & 0xFFFF, 4);
}

// ─── JSON Serialisation ─────────────────────────────────────────────────────

std::string HttpQodClient::serialise_create_request(
    const CreateSessionRequest& req) {
    json j;

    // Device
    json dev;
    if (req.device.phone_number) {
        dev["phoneNumber"] = *req.device.phone_number;
    }
    if (req.device.network_access_identifier) {
        dev["networkAccessIdentifier"] = *req.device.network_access_identifier;
    }
    if (req.device.ipv4_address) {
        json ip;
        ip["publicAddress"] = req.device.ipv4_address->public_address;
        if (req.device.ipv4_address->public_port > 0) {
            ip["publicPort"] = req.device.ipv4_address->public_port;
        }
        dev["ipv4Address"] = ip;
    }
    j["device"] = dev;

    // Application server
    j["applicationServer"] = {
        {"ipv4Address", req.application_server.ipv4_address}};

    // Device ports
    if (req.device_ports) {
        json dp;
        if (!req.device_ports->ports.empty()) {
            dp["ports"] = req.device_ports->ports;
        }
        if (!req.device_ports->ranges.empty()) {
            json ranges = json::array();
            for (const auto& r : req.device_ports->ranges) {
                ranges.push_back({{"from", r.from}, {"to", r.to}});
            }
            dp["ranges"] = ranges;
        }
        j["devicePorts"] = dp;
    }

    // Application server ports
    if (req.application_server_ports) {
        json asp;
        if (!req.application_server_ports->ports.empty()) {
            asp["ports"] = req.application_server_ports->ports;
        }
        if (!req.application_server_ports->ranges.empty()) {
            json ranges = json::array();
            for (const auto& r : req.application_server_ports->ranges) {
                ranges.push_back({{"from", r.from}, {"to", r.to}});
            }
            asp["ranges"] = ranges;
        }
        j["applicationServerPorts"] = asp;
    }

    j["qosProfile"] = req.qos_profile;
    j["duration"] = req.duration_seconds;

    if (req.sink) {
        j["sink"] = *req.sink;
    }

    return j.dump();
}

std::string HttpQodClient::serialise_extend_request(
    const ExtendSessionRequest& req) {
    json j;
    j["requestedAdditionalDuration"] = req.requested_additional_duration;
    return j.dump();
}

std::string HttpQodClient::serialise_device(const Device& dev) {
    json j;
    json d;
    if (dev.phone_number) {
        d["phoneNumber"] = *dev.phone_number;
    }
    if (dev.network_access_identifier) {
        d["networkAccessIdentifier"] = *dev.network_access_identifier;
    }
    if (dev.ipv4_address) {
        json ip;
        ip["publicAddress"] = dev.ipv4_address->public_address;
        if (dev.ipv4_address->public_port > 0) {
            ip["publicPort"] = dev.ipv4_address->public_port;
        }
        d["ipv4Address"] = ip;
    }
    j["device"] = d;
    return j.dump();
}

phine::adapter::SessionInfo HttpQodClient::parse_session_info(const std::string& raw) {
    phine::adapter::SessionInfo info;
    if (raw.empty()) return info;

    try {
        auto j = json::parse(raw);
        if (j.contains("sessionId")) {
            info.session_id = j["sessionId"].get<std::string>();
        }
        if (j.contains("qosStatus")) {
            info.qos_status =
                qos_status_from_string(j["qosStatus"].get<std::string>());
        }
        if (j.contains("statusInfo")) {
            info.status_info =
                status_info_from_string(j["statusInfo"].get<std::string>());
        }
        if (j.contains("duration")) {
            info.duration = j["duration"].get<int>();
        }
        if (j.contains("qosProfile")) {
            info.qos_profile = j["qosProfile"].get<std::string>();
        }
        if (j.contains("startedAt")) {
            info.started_at = j["startedAt"].get<std::string>();
        }
        if (j.contains("expiresAt")) {
            info.expires_at = j["expiresAt"].get<std::string>();
        }
    } catch (const json::exception& e) {
        spdlog::warn("[HttpQodClient] Failed to parse session JSON: {}", e.what());
    }
    return info;
}

std::vector<phine::adapter::SessionInfo> HttpQodClient::parse_session_list(
    const std::string& raw) {
    std::vector<phine::adapter::SessionInfo> sessions;
    if (raw.empty()) return sessions;

    try {
        auto arr = json::parse(raw);
        if (!arr.is_array()) {
            sessions.push_back(parse_session_info(raw));
            return sessions;
        }
        for (const auto& item : arr) {
            sessions.push_back(parse_session_info(item.dump()));
        }
    } catch (const json::exception& e) {
        spdlog::warn("[HttpQodClient] Failed to parse session list: {}", e.what());
    }
    return sessions;
}

}  // namespace phine::adapter
