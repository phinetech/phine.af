/**
 * @file qod_notification_manager.h
 * @brief Manages CloudEvents notifications for CAMARA QualityOnDemand
 *
 * This class handles the delivery of CloudEvents notifications to registered
 * endpoints when QoD session status changes occur. It implements retry logic,
 * authentication, and queue management for reliable notification delivery.
 */

#pragma once

#include <memory>
#include <string>
#include <queue>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include <spdlog/spdlog.h>
#include "../../common/models/qod/qod_events.h"
#include "../../common/models/qod/qod_session.h"
#include "../../common/communication/include/message.h"

namespace af {
namespace core {
    class AfOrchestrator; // Forward declaration
}

namespace qod {

/**
 * @brief Configuration for notification delivery
 */
struct NotificationConfig {
    std::chrono::seconds default_timeout{10};           // HTTP request timeout
    int max_retry_attempts{3};                          // Maximum retry attempts
    std::chrono::seconds initial_retry_delay{1};        // Initial retry delay
    double retry_backoff_multiplier{2.0};               // Exponential backoff multiplier
    std::chrono::seconds max_retry_delay{30};           // Maximum retry delay
    size_t max_queue_size{10000};                       // Maximum notification queue size
    int worker_thread_count{4};                         // Number of worker threads
    bool enable_async_delivery{true};                   // Enable async notification delivery
    std::string user_agent{"CAMARA-QoD-AF/1.0"};       // User agent for HTTP requests
};

/**
 * @brief Notification delivery task
 */
struct NotificationTask {
    CloudEvent event;
    std::string sink;
    std::optional<SinkCredential> credential;
    int retry_count{0};
    std::chrono::system_clock::time_point next_retry_time;
    std::string session_id;
    std::function<void(const NotificationDeliveryResult&)> callback;
};

/**
 * @brief HTTP client wrapper for notification delivery
 */
class HttpNotificationClient {
public:
    HttpNotificationClient();
    ~HttpNotificationClient();
    
    /**
     * @brief Send HTTP POST request with CloudEvent
     * @param url Target URL
     * @param event CloudEvent to send
     * @param credential Optional authentication credential
     * @param timeout Request timeout
     * @return HTTP response code and body
     */
    std::pair<int, std::string> send_notification(
        const std::string& url,
        const CloudEvent& event,
        const std::optional<SinkCredential>& credential,
        std::chrono::seconds timeout);
    
private:
    // TODO: Add HTTP client members (e.g., CURL handle)
    
    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp);
    void setup_authentication(const std::optional<SinkCredential>& credential);
    void cleanup_headers();
};

/**
 * @brief Manages QoD CloudEvents notifications
 */
class QodNotificationManager : public INotificationDelivery {
public:
    /**
     * @brief Constructor
     * @param config Notification configuration
     */
    explicit QodNotificationManager(
        const NotificationConfig& config = NotificationConfig{});
    
    /**
     * @brief Destructor
     */
    ~QodNotificationManager();
    
    /**
     * @brief Initialize the notification manager
     * @param orchestrator Pointer to the orchestrator
     */
    void initialize(af::core::AfOrchestrator* orchestrator);
    
    /**
     * @brief Start the notification manager
     */
    void start();
    
    /**
     * @brief Stop the notification manager
     */
    void stop();
    
    /**
     * @brief Deliver a CloudEvent notification
     * @param event The CloudEvent to deliver
     * @param sink The target URL
     * @param credential Optional authentication credential
     * @return Delivery result
     */
    NotificationDeliveryResult deliver(
        const CloudEvent& event,
        const std::string& sink,
        const std::optional<SinkCredential>& credential = std::nullopt) override;
    
    /**
     * @brief Queue a notification for async delivery
     * @param event The CloudEvent to deliver
     * @param sink The target URL
     * @param credential Optional authentication credential
     * @param session_id QoD session ID for tracking
     * @param callback Optional callback for delivery result
     * @return true if queued successfully
     */
    bool queue_notification(
        const CloudEvent& event,
        const std::string& sink,
        const std::optional<SinkCredential>& credential,
        const std::string& session_id,
        std::function<void(const NotificationDeliveryResult&)> callback = nullptr);
    
    /**
     * @brief Cancel pending notifications for a session
     * @param session_id QoD session ID
     * @return Number of cancelled notifications
     */
    int cancel_session_notifications(const std::string& session_id);
    
    /**
     * @brief Get pending notification count
     * @return Number of pending notifications
     */
    size_t get_pending_count() const;
    
    /**
     * @brief Get statistics for a session
     * @param session_id QoD session ID
     * @return Map of statistic names to values
     */
    std::unordered_map<std::string, int> get_session_stats(
        const std::string& session_id) const;
    
    /**
     * @brief Initialize logger
     */
    void initializeLogger(spdlog::level::level_enum log_level = spdlog::level::info);

private:
    /**
     * @brief Worker thread function
     */
    void worker_thread();
    
    /**
     * @brief Process a notification task
     * @param task The task to process
     * @return true if successful, false if retry needed
     */
    bool process_notification(NotificationTask& task);
    
    /**
     * @brief Calculate next retry time
     * @param task The task to retry
     * @return Next retry time point
     */
    std::chrono::system_clock::time_point calculate_next_retry(
        const NotificationTask& task);
    
    /**
     * @brief Validate sink URL
     * @param sink URL to validate
     * @return true if valid
     */
    bool validate_sink(const std::string& sink);
    
    /**
     * @brief Validate credential expiry
     * @param credential Credential to validate
     * @return true if valid (not expired)
     */
    bool validate_credential(const std::optional<SinkCredential>& credential);
    
    /**
     * @brief Update session statistics
     * @param session_id Session ID
     * @param success Whether delivery was successful
     */
    void update_stats(const std::string& session_id, bool success);
    
    /**
     * @brief Clean up expired statistics
     */
    void cleanup_old_stats();
    
    // === Member Variables ===
    
    af::core::AfOrchestrator* orchestrator_;
    NotificationConfig config_;
    
    // Task queue and synchronization
    std::queue<NotificationTask> task_queue_;
    std::priority_queue<NotificationTask, 
                       std::vector<NotificationTask>,
                       std::function<bool(const NotificationTask&, const NotificationTask&)>> 
                       retry_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    // Worker threads
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> running_{false};
    
    // Session tracking
    std::unordered_map<std::string, std::vector<NotificationTask>> session_tasks_;
    mutable std::mutex session_mutex_;
    
    // Statistics
    struct SessionStats {
        int total_sent{0};
        int successful{0};
        int failed{0};
        int pending{0};
        std::chrono::system_clock::time_point last_update;
    };
    std::unordered_map<std::string, SessionStats> session_stats_;
    mutable std::mutex stats_mutex_;
    
    // HTTP clients pool
    std::vector<std::unique_ptr<HttpNotificationClient>> http_clients_;
    std::mutex clients_mutex_;
    
    // Logger
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace qod
} // namespace af