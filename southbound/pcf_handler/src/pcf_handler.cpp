/**
 * @file pcf_handler.cpp
 * @brief Implementation of the PCF handler
 */

#include "pcf_handler.h"
#include <yaml-cpp/yaml.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <nlohmann/json.hpp>
#include <communication_factory.h>

using namespace oai::model::pcf;
namespace af {
namespace southbound {

// Inner class for handling messages
class PcfHandler::PcfMessageHandler : public af::communication::MessageHandler {
public:
    PcfMessageHandler(PcfHandler* handler) : handler_(handler) {}

    af::communication::MessagePtr handle_message(
        const af::communication::MessagePtr& message) override {
        return handler_->process_message(message);
    }

private:
    PcfHandler* handler_;
};

PcfHandler::PcfHandler(const std::string& config_path)
    : AfComponent("pcf_handler"), config_path_(config_path) {

    // Setup logger
    initializeLogger(spdlog::level::debug);

    logger_->info("Initializing PCF Handler");

    // Create message handler
    message_handler_ = std::make_shared<PcfMessageHandler>(this);

    // Create request router
    request_router_ = std::make_shared<RequestRouter>();

    // Load configuration
    load_config();

    // Create QoD PCF handler
    qod_pcf_handler_ = std::make_shared<QodPcfHandler>(config_path_);

    // Create PCC rule manager
    pcc_rule_manager_ = std::make_shared<PccRuleManager>();

    // Create PCF client
    pcf_client_ = std::make_shared<PcfClientWrapper>(
        pcf_base_url_, use_tls_, api_version_);
}

PcfHandler::~PcfHandler() {
    stop();
}

af::communication::MessagePtr PcfHandler::process_message(
    const af::communication::MessagePtr& message) {

    if (!message) {
        logger_->error("Received null message");
        return nullptr;
    }

    logger_->debug("Processing message of type: {}", message->message_type);

    // Route the message to the appropriate handler
    return request_router_->route_message(message);
}

void PcfHandler::initializeLogger(spdlog::level::level_enum log_level) {
    // Check if a logger with this name already exists
    logger_ = spdlog::get("af_pcf");

    if (!logger_) {
        // Create a new logger with a colored console sink
        logger_ = spdlog::stdout_color_mt("api_adapter");
    }

    // Set the log level
    logger_->set_level(log_level);

    // Set the log pattern: timestamp [level] [component] message
    logger_->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] [%n] %v");
}

void PcfHandler::initialize() {
    logger_->info("Initializing PCF Handler components");

    // Initialize PCF client
    pcf_client_->initialize();

    request_router_->initialize(this);

    // Initialize PCC rule manager
    pcc_rule_manager_->initialize();

    // Initialize communication with AF Core
    initialize_communication();

    // Register message handlers
    register_handlers();

    // Register QoD related handlers
    register_qod_handlers();

    logger_->info("PCF Handler initialization complete");
}

void PcfHandler::start() {
    logger_->info("Starting PCF Handler");

    // Start communication service
    if (core_comm_) {
        if (!core_comm_->start()) {
            logger_->error("Failed to start communication service");
        }
    }

    logger_->info("PCF Handler started");
}

void PcfHandler::stop() {
    logger_->info("Stopping PCF Handler");

    // Stop communication service
    if (core_comm_) {
        core_comm_->stop();
    }

    logger_->info("PCF Handler stopped");
}

void PcfHandler::load_config() {
    try {
        logger_->info("Loading configuration from {}", config_path_);
        YAML::Node config = YAML::LoadFile(config_path_);

        // Load PCF connection details
        pcf_base_url_ = config["pcf_handler"]["pcf_base_url"].as<std::string>(
            "http://pcf:80/npcf-policyauthorization/v1");

        use_tls_ = config["pcf_handler"]["use_tls"].as<bool>(false);
        api_version_ = config["pcf_handler"]["api_version"].as<std::string>("v1");

        auto comm_config = config["pcf_handler"]["communication"];
        if (comm_config) {
            if (comm_config["type"]) {
                comm_type_ = comm_config["type"].as<std::string>();
            }
            if (comm_config["server_address"]) {
                server_address_ = comm_config["server_address"].as<std::string>();
            }
            if (comm_config["server_port"]) {
                server_port_ = comm_config["server_port"].as<std::string>();
            }
            if (comm_config["core_address"]) {
                core_address_ = comm_config["core_address"].as<std::string>();
            }
            if (comm_config["core_port"]) {
                core_port_ = comm_config["core_port"].as<std::string>();
            }
        }

        if (comm_type_ == "direct") {
            core_destination_ = "af_core";
        } else {
            std::string advertised_core = core_address_;
            if (advertised_core.empty() || advertised_core == "0.0.0.0") {
                advertised_core = "af_core";
            }
            core_destination_ = advertised_core + ":" + core_port_;
        }

        logger_->info("PCF base URL: {}", pcf_base_url_);
        logger_->info("Using TLS: {}", use_tls_ ? "true" : "false");
        logger_->info("API version: {}", api_version_);
        logger_->info("Communication type: {}", comm_type_);
        logger_->info("PCF handler listen address: {}:{}", server_address_, server_port_);
        logger_->info("AF Core destination: {}", core_destination_);
    }
    catch (const std::exception& e) {
        logger_->error("Failed to load configuration: {}", e.what());

        // Set default values
        pcf_base_url_ = "http://pcf:80/npcf-policyauthorization/v1";
        use_tls_ = false;
        api_version_ = "v1";
        comm_type_ = "grpc";
        server_address_ = "0.0.0.0";
        server_port_ = "50055";
        core_address_ = "af_core";
        core_port_ = "50051";
        core_destination_ = "af_core:50051";
    }
}

void PcfHandler::initialize_communication() {
    try {
        logger_->info("Initializing communication with AF Core");

        // Create communication service
        std::unordered_map<std::string, std::string> comm_config;
        comm_config["server_address"] = server_address_;
        comm_config["server_port"] = server_port_;

        core_comm_ = af::communication::CommunicationFactory::create_service(
            comm_type_, "pcf_handler", comm_config);

        if (!core_comm_) {
            throw std::runtime_error("Failed to create communication service");
        }

        logger_->info("Communication with AF Core initialized");
    }
    catch (const std::exception& e) {
        logger_->error("Failed to initialize communication: {}", e.what());
        throw;
    }
}

void PcfHandler::register_handlers() {
    logger_->info("Registering message handlers");

    // Register handlers for the core communication service
    if (core_comm_) {
        core_comm_->register_handler("pcf_create_app_session", message_handler_);
        core_comm_->register_handler("pcf_update_app_session", message_handler_);
        core_comm_->register_handler("pcf_delete_app_session", message_handler_);
        core_comm_->register_handler("pcf_get_app_session", message_handler_);
        core_comm_->register_handler("pcf_notification", message_handler_);

        core_comm_->register_handler("pcf_create_qod_session", message_handler_);
        core_comm_->register_handler("pcf_update_qod_session", message_handler_);
        core_comm_->register_handler("pcf_delete_qod_session", message_handler_);

        logger_->info("Message handlers registered");
    }
}

// TODO: move them to qod handlers
void PcfHandler::register_qod_handlers() {
    request_router_->register_handler("pcf_create_qod_session",
        [this](const af::communication::MessagePtr& msg) {
            return qod_pcf_handler_->handle_qod_create_pcf_session(msg);
        });

    request_router_->register_handler("pcf_update_qod_session",
        [this](const af::communication::MessagePtr& msg) {
            return qod_pcf_handler_->handle_qod_update_pcf_session(msg);
        });
    request_router_->register_handler("pcf_delete_qod_session",
        [this](const af::communication::MessagePtr& msg) {
            return qod_pcf_handler_->handle_qod_delete_pcf_session(msg);
        });
}

af::communication::MessagePtr PcfHandler::create_app_session(
    const af::communication::MessagePtr& app_session_data) {

    logger_->info("Creating application session with PCF");

    auto response = std::make_shared<af::communication::Message>();
    response->correlation_id = app_session_data->correlation_id;

    try {
        // Extract payload as JSON
        std::string payload_str(app_session_data->payload.begin(), app_session_data->payload.end());
        auto request_data = nlohmann::json::parse(payload_str);

        logger_->debug("App session request: {}", request_data.dump());

        oai::model::pcf::AppSessionContext app_session = {};
        oai::model::pcf::AppSessionContextReqData app_session_req_data = {};
        // Required fields
        if (request_data.contains("af_app_id")) {
            app_session_req_data.setAfAppId(request_data["af_app_id"]);
        }

        // Add UE information if available
        if (request_data.contains("ue_ipv4")) {
            app_session_req_data.setUeIpv4(request_data["ue_ipv4"]);
        }

        if (request_data.contains("ue_ipv6")) {
            app_session_req_data.setUeIpv6(request_data["ue_ipv6"]);
        }

        // Add UE IMSI if available
        if (request_data.contains("ue_supi")) {
            app_session_req_data.setSupi(request_data["ue_supi"]);
        }

        // Add media component information if available
        if (request_data.contains("media_components") &&
            request_data["media_components"].is_array()) {

            nlohmann::json media_components;
            std::map<std::string, oai::model::pcf::MediaComponent> media_component_map;

            for (const auto& component : request_data["media_components"]) {
                oai::model::pcf::MediaComponent media_component_;

                std::string media_component_id = component["media_component_id"];
                media_component_.setAfAppId(request_data["af_app_id"]);

                nlohmann::json media_component;

                // Set media type
                if (component.contains("media_type")) {
                    media_component["medType"] = component["media_type"];
                    media_component_.setMedType(component["media_type"]);
                }

                // TODO: handle flow infomation


                // Set QoS information if available
                if (component.contains("qos_info")) {

                    if (component["qos_info"].contains("max_bw_ul")) {
                        media_component_.setMaxSuppBwUl(component["qos_info"]["max_bw_ul"]);
                    }

                    if (component["qos_info"].contains("max_bw_dl")) {
                        media_component_.setMaxSuppBwDl(component["qos_info"]["max_bw_dl"]);
                    }

                    if (component["qos_info"].contains("min_bw_ul")) {
                        media_component_.setMinDesBwUl(component["qos_info"]["min_bw_ul"]);
                    }

                    if (component["qos_info"].contains("min_bw_dl")) {
                        media_component_.setMinDesBwDl(component["qos_info"]["min_bw_dl"]);
                    }

                }

                media_components[media_component_id] = media_component;
                media_component_map[media_component_id] = media_component_;
            }

            app_session_req_data.setMedComponents(media_component_map);
        }

        // TODO: handle event subscription information

        // Add additional parameters if needed
        if (request_data.contains("supp_features")) {
            app_session_req_data.setSuppFeat(request_data["supp_features"]);
        }

        // Set the application session context
        app_session.setAscReqData(app_session_req_data);

        nlohmann::json json_data;
        to_json(json_data, app_session);

        // Call PCF client to create the app session
        logger_->debug("App session request data: {}", json_data.dump());
        auto pcf_response = pcf_client_->create_app_session(json_data);

        if (pcf_response.first) {
            // Success
            auto& session_data = pcf_response.second;
            logger_->info("App session created successfully");

            logger_->debug("PCF response data: {}", session_data.dump());

            // TODO: handle app session response data
            // Store session information
            AppSessionInfo session_info;
            // session_info.app_session_id = session_data["appSessionId"];

            // if (request_data.contains("ue_ipv4")) {
            //     session_info.ipv4_address = request_data["ue_ipv4"];
            // }

            // if (request_data.contains("ue_ipv6")) {
            //     session_info.ipv6_prefix = request_data["ue_ipv6"];
            // }

            // // Extract media component IDs
            // if (session_data.contains("medComponents") &&
            //     session_data["medComponents"].is_object()) {

            //     for (auto& [media_id, _] : session_data["medComponents"].items()) {
            //         session_info.media_components.push_back(media_id);
            //     }
            // }

            session_info.active = true;

            store_app_session(session_info.app_session_id, session_info);

            // Create success response
            response->message_type = "pcf_app_session_created";

            nlohmann::json result = {
                {"app_session_id", session_info.app_session_id},
                {"status", "active"}
            };

            // Add PCC rules if available
            if (session_data.contains("pccRules") &&
                session_data["pccRules"].is_object()) {

                result["pcc_rules"] = session_data["pccRules"];
            }

            std::string result_str = result.dump();
            response->payload.assign(result_str.begin(), result_str.end());
        }
        else {
            // Error
            logger_->error("Failed to create app session: {}", pcf_response.second.dump());

            response->message_type = "pcf_error";

            nlohmann::json error = {
                {"error", "app_session_creation_failed"},
                {"message", pcf_response.second.dump()}
            };

            std::string error_str = error.dump();
            response->payload.assign(error_str.begin(), error_str.end());
        }
    }
    catch (const std::exception& e) {
        logger_->error("Error creating app session: {}", e.what());

        response->message_type = "pcf_error";

        nlohmann::json error = {
            {"error", "bad_request"},
            {"message", e.what()}
        };

        std::string error_str = error.dump();
        response->payload.assign(error_str.begin(), error_str.end());
    }
    return response;
}

af::communication::MessagePtr PcfHandler::update_app_session(
    const af::communication::MessagePtr& update_data) {

    logger_->info("Updating application session");

    auto response = std::make_shared<af::communication::Message>();
    response->correlation_id = update_data->correlation_id;

    try {
        // Extract payload as JSON
        std::string payload_str(update_data->payload.begin(), update_data->payload.end());
        auto request_data = nlohmann::json::parse(payload_str);

        if (!request_data.contains("app_session_id")) {
            throw std::runtime_error("Missing required field: app_session_id");
        }

        std::string app_session_id = request_data["app_session_id"];
        logger_->info("Updating app session: {}", app_session_id);

        // Get stored session info
        auto session_info = get_app_session_info(app_session_id);

        if (!session_info) {
            throw std::runtime_error("App session not found: " + app_session_id);
        }

        // Prepare update data
        nlohmann::json update_context;

        // Add media component information if available
        if (request_data.contains("media_components") &&
            request_data["media_components"].is_array()) {

            nlohmann::json media_components;

            for (const auto& component : request_data["media_components"]) {
                std::string media_component_id = component["media_component_id"];

                nlohmann::json media_component;

                // Set media type
                if (component.contains("media_type")) {
                    media_component["medType"] = component["media_type"];
                }

                // Set flow information if available
                if (component.contains("flows") && component["flows"].is_array()) {
                    nlohmann::json flows = nlohmann::json::array();

                    for (const auto& flow : component["flows"]) {
                        nlohmann::json flow_info;

                        if (flow.contains("flow_id")) {
                            flow_info["flowId"] = flow["flow_id"];
                        }

                        if (flow.contains("flow_descriptions") && flow["flow_descriptions"].is_array()) {
                            flow_info["flowDescriptions"] = flow["flow_descriptions"];
                        }

                        flows.push_back(flow_info);
                    }

                    media_component["fDescs"] = flows;
                }

                // Set QoS information if available
                if (component.contains("qos_info")) {
                    nlohmann::json qos_info;

                    if (component["qos_info"].contains("max_bw_ul")) {
                        qos_info["maxbrUl"] = component["qos_info"]["max_bw_ul"];
                    }

                    if (component["qos_info"].contains("max_bw_dl")) {
                        qos_info["maxbrDl"] = component["qos_info"]["max_bw_dl"];
                    }

                    if (component["qos_info"].contains("min_bw_ul")) {
                        qos_info["minbrUl"] = component["qos_info"]["min_bw_ul"];
                    }

                    if (component["qos_info"].contains("min_bw_dl")) {
                        qos_info["minbrDl"] = component["qos_info"]["min_bw_dl"];
                    }

                    media_component["medQoS"] = qos_info;
                }

                media_components[media_component_id] = media_component;
            }

            update_context["medComponents"] = media_components;
        }

        // Call PCF client to update the app session
        auto pcf_response = pcf_client_->update_app_session(app_session_id, update_context);

        if (pcf_response.first) {
            // Success
            logger_->info("App session updated successfully");

            // Update stored session information
            if (pcf_response.second.contains("medComponents") &&
                pcf_response.second["medComponents"].is_object()) {

                session_info->media_components.clear();

                for (auto& [media_id, _] : pcf_response.second["medComponents"].items()) {
                    session_info->media_components.push_back(media_id);
                }
            }

            session_info->app_session_context = pcf_response.second.dump();

            // Create success response
            response->message_type = "pcf_app_session_updated";

            nlohmann::json result = {
                {"app_session_id", app_session_id},
                {"status", "active"}
            };

            // Add PCC rules if available
            if (pcf_response.second.contains("pccRules") &&
                pcf_response.second["pccRules"].is_object()) {

                result["pcc_rules"] = pcf_response.second["pccRules"];
            }

            std::string result_str = result.dump();
            response->payload.assign(result_str.begin(), result_str.end());
        }
        else {
            // Error
            logger_->error("Failed to update app session: {}", pcf_response.second.dump());

            response->message_type = "pcf_error";

            nlohmann::json error = {
                {"error", "app_session_update_failed"},
                {"message", pcf_response.second.dump()}
            };

            std::string error_str = error.dump();
            response->payload.assign(error_str.begin(), error_str.end());
        }
    }
    catch (const std::exception& e) {
        logger_->error("Error updating app session: {}", e.what());

        response->message_type = "pcf_error";

        nlohmann::json error = {
            {"error", "bad_request"},
            {"message", e.what()}
        };

        std::string error_str = error.dump();
        response->payload.assign(error_str.begin(), error_str.end());
    }

    return response;
}

af::communication::MessagePtr PcfHandler::delete_app_session(
    const af::communication::MessagePtr& delete_data) {

    logger_->info("Deleting application session");

    auto response = std::make_shared<af::communication::Message>();
    response->correlation_id = delete_data->correlation_id;

    try {
        // Extract payload as JSON
        std::string payload_str(delete_data->payload.begin(), delete_data->payload.end());
        auto request_data = nlohmann::json::parse(payload_str);

        if (!request_data.contains("app_session_id")) {
            throw std::runtime_error("Missing required field: app_session_id");
        }

        std::string app_session_id = request_data["app_session_id"];
        logger_->info("Deleting app session: {}", app_session_id);

        // Get stored session info
        auto session_info = get_app_session_info(app_session_id);

        if (!session_info) {
            throw std::runtime_error("App session not found: " + app_session_id);
        }

        // Call PCF client to delete the app session
        bool success = pcf_client_->delete_app_session(app_session_id, {});

        if (success) {
            // Success
            logger_->info("App session deleted successfully");

            // Remove session from storage
            remove_app_session(app_session_id);

            // Create success response
            response->message_type = "pcf_app_session_deleted";

            nlohmann::json result = {
                {"app_session_id", app_session_id},
                {"status", "deleted"}
            };

            std::string result_str = result.dump();
            response->payload.assign(result_str.begin(), result_str.end());
        }
        else {
            // Error
            logger_->error("Failed to delete app session");

            response->message_type = "pcf_error";

            nlohmann::json error = {
                {"error", "app_session_deletion_failed"},
                {"message", "Failed to delete app session"}
            };

            std::string error_str = error.dump();
            response->payload.assign(error_str.begin(), error_str.end());
        }
    }
    catch (const std::exception& e) {
        logger_->error("Error deleting app session: {}", e.what());

        response->message_type = "pcf_error";

        nlohmann::json error = {
            {"error", "bad_request"},
            {"message", e.what()}
        };

        std::string error_str = error.dump();
        response->payload.assign(error_str.begin(), error_str.end());
    }

    return response;
}

af::communication::MessagePtr PcfHandler::get_app_session(
    const af::communication::MessagePtr& get_data) {

    logger_->info("Getting application session information");

    auto response = std::make_shared<af::communication::Message>();
    response->correlation_id = get_data->correlation_id;

    try {
        // Extract payload as JSON
        std::string payload_str(get_data->payload.begin(), get_data->payload.end());
        auto request_data = nlohmann::json::parse(payload_str);

        if (!request_data.contains("app_session_id")) {
            throw std::runtime_error("Missing required field: app_session_id");
        }

        std::string app_session_id = request_data["app_session_id"];
        logger_->info("Getting app session: {}", app_session_id);

        // Get stored session info
        auto session_info = get_app_session_info(app_session_id);

        if (!session_info) {
            throw std::runtime_error("App session not found: " + app_session_id);
        }

        // Call PCF client to get the app session
        auto pcf_response = pcf_client_->get_app_session(app_session_id);

        if (pcf_response.first) {
            // Success
            logger_->info("Got app session information successfully");

            // Create success response
            response->message_type = "pcf_app_session_info";

            // Update stored session information
            session_info->app_session_context = pcf_response.second.dump();

            // Add additional fields for easier consumption
            nlohmann::json result = pcf_response.second;
            result["app_session_id"] = app_session_id;

            if (session_info->ipv4_address.empty() == false) {
                result["ue_ipv4"] = session_info->ipv4_address;
            }

            if (session_info->ipv6_prefix.empty() == false) {
                result["ue_ipv6"] = session_info->ipv6_prefix;
            }

            std::string result_str = result.dump();
            response->payload.assign(result_str.begin(), result_str.end());
        }
        else {
            // Error
            logger_->error("Failed to get app session information: {}", pcf_response.second.dump());

            response->message_type = "pcf_error";

            nlohmann::json error = {
                {"error", "app_session_retrieval_failed"},
                {"message", pcf_response.second.dump()}
            };

            std::string error_str = error.dump();
            response->payload.assign(error_str.begin(), error_str.end());
        }
    }
    catch (const std::exception& e) {
        logger_->error("Error getting app session: {}", e.what());

        response->message_type = "pcf_error";

        nlohmann::json error = {
            {"error", "bad_request"},
            {"message", e.what()}
        };

        std::string error_str = error.dump();
        response->payload.assign(error_str.begin(), error_str.end());
    }

    return response;
}

af::communication::MessagePtr PcfHandler::handle_notification(
    const af::communication::MessagePtr& notification_data) {

    logger_->info("Handling PCF notification");

    auto response = std::make_shared<af::communication::Message>();
    response->correlation_id = notification_data->correlation_id;

    try {
        // Extract payload as JSON
        std::string payload_str(notification_data->payload.begin(), notification_data->payload.end());
        auto notification = nlohmann::json::parse(payload_str);

        // Extract notification type if available
        std::string notification_type = "unknown";

        if (notification.contains("event_type")) {
            notification_type = notification["event_type"];
        }

        logger_->info("Received notification of type: {}", notification_type);

        // Forward the notification to the AF Core
        forward_notification(notification_type, notification);

        // Create success response
        response->message_type = "pcf_notification_ack";

        nlohmann::json result = {
            {"status", "acknowledged"}
        };

        std::string result_str = result.dump();
        response->payload.assign(result_str.begin(), result_str.end());
    }
    catch (const std::exception& e) {
        logger_->error("Error handling notification: {}", e.what());

        response->message_type = "pcf_error";

        nlohmann::json error = {
            {"error", "notification_handling_failed"},
            {"message", e.what()}
        };

        std::string error_str = error.dump();
        response->payload.assign(error_str.begin(), error_str.end());
    }

    return response;
}

void PcfHandler::store_app_session(const std::string& session_id, const AppSessionInfo& session_info) {
    std::lock_guard<std::mutex> lock(app_sessions_mutex_);
    app_sessions_[session_id] = session_info;

    logger_->debug("Stored app session: {}", session_id);
}

std::shared_ptr<PcfHandler::AppSessionInfo> PcfHandler::get_app_session_info(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(app_sessions_mutex_);

    auto it = app_sessions_.find(session_id);
    if (it != app_sessions_.end()) {
        return std::make_shared<AppSessionInfo>(it->second);
    }

    logger_->debug("App session not found: {}", session_id);
    return nullptr;
}

bool PcfHandler::remove_app_session(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(app_sessions_mutex_);

    auto it = app_sessions_.find(session_id);
    if (it != app_sessions_.end()) {
        app_sessions_.erase(it);
        logger_->debug("Removed app session: {}", session_id);
        return true;
    }

    logger_->debug("App session not found for removal: {}", session_id);
    return false;
}

void PcfHandler::forward_notification(const std::string& notification_type,
                                     const nlohmann::json& notification_data) {

    logger_->info("Forwarding notification to AF Core: {}", notification_type);

    // Create message
    auto message = std::make_shared<af::communication::Message>();
    message->message_type = "network_event";

    // Add metadata
    message->metadata["event_type"] = notification_type;
    message->metadata["source"] = "pcf";

    // Prepare payload
    nlohmann::json payload = {
        {"event_type", notification_type},
        {"source", "pcf"},
        {"data", notification_data}
    };

    std::string payload_str = payload.dump();
    message->payload.assign(payload_str.begin(), payload_str.end());

    // Send asynchronously to AF Core
    if (core_comm_) {
        core_comm_->send_async(core_destination_, message, [this](const af::communication::MessagePtr& response) -> af::communication::MessagePtr {
            if (response) {
                logger_->debug("Received response from AF Core: {}", response->message_type);
            }
            return nullptr; // Return null pointer as we don't need to send a response back
        });
    }
    else {
        logger_->error("Cannot forward notification: no communication service");
    }
}

} // namespace southbound
} // namespace af