/**
 * @file qod_notification_manager.cpp
 * @brief Implementation of the QoD notification manager
 */

#include "qod/qod_notification_manager.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <regex>
#include <random>
#include "af_orchestrator.h"

namespace af {
namespace qod {

// === HttpNotificationClient Implementation ===

HttpNotificationClient::HttpNotificationClient() {
    // TODO: implement constructor
    
}

HttpNotificationClient::~HttpNotificationClient() {
    cleanup_headers();

}

size_t HttpNotificationClient::write_callback(void* contents, size_t size, 
                                             size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), total_size);
    return total_size;
}

void HttpNotificationClient::cleanup_headers() {
    // TODO: implement header cleanup
}

void HttpNotificationClient::setup_authentication(
    const std::optional<SinkCredential>& credential) {
    
    if (!credential) {
        return;
    }
    
    if (credential->credential_type == SinkCredential::CredentialType::ACCESSTOKEN) {
        if (credential->access_token) {
            std::string auth_header = "Authorization: Bearer " + *credential->access_token;
        }
    }
}

std::pair<int, std::string> HttpNotificationClient::send_notification(
    const std::string& url,
    const CloudEvent& event,
    const std::optional<SinkCredential>& credential,
    std::chrono::seconds timeout) {
    
    // TODO: Implement method using nghttp2 or another HTTP library
    return {0, "Not implemented in this build"};
}

// === QodNotificationManager Implementation ===

QodNotificationManager::QodNotificationManager(const NotificationConfig& config)
        : config_(config), // Initialize config_ first
        retry_queue_(
            // Initialize retry_queue_ with a lambda directly
            [](const NotificationTask& a, const NotificationTask& b) {
                return a.next_retry_time > b.next_retry_time;
            }
        )
    {
    
    // Initialize logger
    initializeLogger(spdlog::level::debug);
    
    logger_->info("QoD Notification Manager created");
    
    // TODO: Initialize http lib globally

    // Set up retry queue comparator (earliest retry time first)
    auto comparator = [](const NotificationTask& a, const NotificationTask& b) {
        return a.next_retry_time > b.next_retry_time;
    };
    
    // Pre-create HTTP clients for worker threads
    for (int i = 0; i < config_.worker_thread_count; ++i) {
        http_clients_.push_back(std::make_unique<HttpNotificationClient>());
    }
}

QodNotificationManager::~QodNotificationManager() {
    stop();
    
    // Cleanup HTTP clients
}

void QodNotificationManager::initialize(af::core::AfOrchestrator* orchestrator) {
    orchestrator_ = orchestrator;
    logger_->info("QoD Notification Manager initialized");
}

void QodNotificationManager::initializeLogger(spdlog::level::level_enum log_level) {
    logger_ = spdlog::get("qod_notif_mgr");
    
    if (!logger_) {
        logger_ = spdlog::stdout_color_mt("qod_notif_mgr");
    }
    
    logger_->set_level(log_level);
    logger_->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] [%n] %v");
}

void QodNotificationManager::start() {
    logger_->info("Starting QoD Notification Manager");
    
    running_ = true;
    
    // Start worker threads
    for (int i = 0; i < config_.worker_thread_count; ++i) {
        worker_threads_.emplace_back(&QodNotificationManager::worker_thread, this);
    }
    
    logger_->info("Started {} notification worker threads", config_.worker_thread_count);
}

void QodNotificationManager::stop() {
    logger_->info("Stopping QoD Notification Manager");
    
    running_ = false;
    
    // Wake up all worker threads
    queue_cv_.notify_all();
    
    // Wait for worker threads to finish
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
    
    // Clear queues
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        std::queue<NotificationTask> empty;
        task_queue_.swap(empty);
    }
    
    logger_->info("QoD Notification Manager stopped");
}

NotificationDeliveryResult QodNotificationManager::deliver(
    const CloudEvent& event,
    const std::string& sink,
    const std::optional<SinkCredential>& credential) {
    
    logger_->debug("Delivering notification to: {}", sink);
    
    NotificationDeliveryResult result;
    result.timestamp = std::chrono::system_clock::now();
    
    // Validate sink URL
    if (!validate_sink(sink)) {
        result.success = false;
        result.http_status_code = 0;
        result.error_message = "Invalid sink URL";
        logger_->error("Invalid sink URL: {}", sink);
        return result;
    }
    
    // Validate credential if provided
    if (!validate_credential(credential)) {
        result.success = false;
        result.http_status_code = 0;
        result.error_message = "Expired or invalid credential";
        logger_->error("Expired or invalid credential for sink: {}", sink);
        return result;
    }
    
    try {
        // Use synchronous delivery if async is disabled
        if (!config_.enable_async_delivery) {
            HttpNotificationClient client;
            auto [http_code, response] = client.send_notification(
                sink, event, credential, config_.default_timeout);
            
            result.http_status_code = http_code;
            result.success = (http_code >= 200 && http_code < 300);
            
            if (!result.success) {
                result.error_message = "HTTP " + std::to_string(http_code) + ": " + response;
                logger_->warn("Notification delivery failed: {}", result.error_message);
            } else {
                logger_->debug("Notification delivered successfully");
            }
        } else {
            // Queue for async delivery
            NotificationTask task;
            task.event = event;
            task.sink = sink;
            task.credential = credential;
            task.retry_count = 0;
            task.next_retry_time = std::chrono::system_clock::now();
            
            // Extract session ID from event if available
            if (event.data.contains("sessionId")) {
                task.session_id = event.data["sessionId"];
            }
            
            bool queued = queue_notification(event, sink, credential, task.session_id);
            
            if (queued) {
                result.success = true;
                result.http_status_code = 202; // Accepted for processing
                result.error_message = "";
                logger_->debug("Notification queued for async delivery");
            } else {
                result.success = false;
                result.http_status_code = 503; // Service Unavailable
                result.error_message = "Queue full";
                logger_->error("Failed to queue notification - queue full");
            }
        }
    }
    catch (const std::exception& e) {
        result.success = false;
        result.http_status_code = 0;
        result.error_message = std::string("Exception: ") + e.what();
        logger_->error("Exception during notification delivery: {}", e.what());
    }
    
    return result;
}

bool QodNotificationManager::queue_notification(
    const CloudEvent& event,
    const std::string& sink,
    const std::optional<SinkCredential>& credential,
    const std::string& session_id,
    std::function<void(const NotificationDeliveryResult&)> callback) {
    
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    // Check queue size
    if (task_queue_.size() >= config_.max_queue_size) {
        logger_->error("Notification queue full ({} items)", task_queue_.size());
        return false;
    }
    
    // Create task
    NotificationTask task;
    task.event = event;
    task.sink = sink;
    task.credential = credential;
    task.session_id = session_id;
    task.callback = callback;
    task.retry_count = 0;
    task.next_retry_time = std::chrono::system_clock::now();
    
    // Add to queue
    task_queue_.push(task);
    
    // Track by session
    if (!session_id.empty()) {
        std::lock_guard<std::mutex> session_lock(session_mutex_);
        session_tasks_[session_id].push_back(task);
    }
    
    // Update stats
    update_stats(session_id, false); // Mark as pending
    
    // Notify worker thread
    queue_cv_.notify_one();
    
    logger_->debug("Notification queued for session: {}", session_id);
    
    return true;
}

int QodNotificationManager::cancel_session_notifications(const std::string& session_id) {
    int cancelled = 0;
    
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        
        auto it = session_tasks_.find(session_id);
        if (it != session_tasks_.end()) {
            cancelled = it->second.size();
            session_tasks_.erase(it);
            logger_->info("Cancelled {} notifications for session: {}", 
                         cancelled, session_id);
        }
    }
    
    // TODO: Also remove from task_queue_ and retry_queue_
    // This would require maintaining additional indices or rebuilding queues
    
    return cancelled;
}

size_t QodNotificationManager::get_pending_count() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return task_queue_.size() + retry_queue_.size();
}

std::unordered_map<std::string, int> QodNotificationManager::get_session_stats(
    const std::string& session_id) const {
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    std::unordered_map<std::string, int> result;
    
    auto it = session_stats_.find(session_id);
    if (it != session_stats_.end()) {
        result["total_sent"] = it->second.total_sent;
        result["successful"] = it->second.successful;
        result["failed"] = it->second.failed;
        result["pending"] = it->second.pending;
    }
    
    return result;
}

void QodNotificationManager::worker_thread() {
    logger_->debug("Notification worker thread started");
    
    // Get thread-local HTTP client
    HttpNotificationClient* http_client = nullptr;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        if (!http_clients_.empty()) {
            http_client = http_clients_.back().release();
            http_clients_.pop_back();
        }
    }
    
    if (!http_client) {
        http_client = new HttpNotificationClient();
    }
    
    while (running_) {
        NotificationTask task;
        bool has_task = false;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            // Wait for tasks or check retry queue
            queue_cv_.wait_for(lock, std::chrono::seconds(1), [this] {
                return !task_queue_.empty() || !retry_queue_.empty() || !running_;
            });
            
            if (!running_) {
                break;
            }
            
            // Check retry queue first
            auto now = std::chrono::system_clock::now();
            if (!retry_queue_.empty() && retry_queue_.top().next_retry_time <= now) {
                task = retry_queue_.top();
                retry_queue_.pop();
                has_task = true;
            }
            // Then check regular queue
            else if (!task_queue_.empty()) {
                task = task_queue_.front();
                task_queue_.pop();
                has_task = true;
            }
        }
        
        if (has_task) {
            // Process the notification
            auto [http_code, response] = http_client->send_notification(
                task.sink, task.event, task.credential, config_.default_timeout);
            
            bool success = (http_code >= 200 && http_code < 300);
            
            if (!success && task.retry_count < config_.max_retry_attempts) {
                // Schedule retry
                task.retry_count++;
                task.next_retry_time = calculate_next_retry(task);
                
                logger_->debug("Scheduling retry {} for notification to: {}", 
                             task.retry_count, task.sink);
                
                std::lock_guard<std::mutex> lock(queue_mutex_);
                retry_queue_.push(task);
            } else {
                // Final result (success or max retries reached)
                if (task.callback) {
                    NotificationDeliveryResult result;
                    result.success = success;
                    result.http_status_code = http_code;
                    result.error_message = success ? "" : response;
                    result.timestamp = std::chrono::system_clock::now();
                    
                    task.callback(result);
                }
                
                // Update statistics
                update_stats(task.session_id, success);
                
                if (success) {
                    logger_->debug("Notification delivered successfully to: {}", task.sink);
                } else {
                    logger_->error("Failed to deliver notification after {} attempts to: {}", 
                                  task.retry_count + 1, task.sink);
                }
            }
        }
    }
    
    // Return HTTP client to pool
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        http_clients_.push_back(std::unique_ptr<HttpNotificationClient>(http_client));
    }
    
    logger_->debug("Notification worker thread stopped");
}

bool QodNotificationManager::process_notification(NotificationTask& task) {
    // This method is not used in current implementation
    // (processing is done inline in worker_thread)
    return true;
}

std::chrono::system_clock::time_point QodNotificationManager::calculate_next_retry(
    const NotificationTask& task) {
    
    // Exponential backoff with jitter
    auto base_delay = config_.initial_retry_delay;
    for (int i = 1; i < task.retry_count; ++i) {
        base_delay = std::chrono::seconds(
            static_cast<long>(base_delay.count() * config_.retry_backoff_multiplier));
        
        if (base_delay > config_.max_retry_delay) {
            base_delay = config_.max_retry_delay;
            break;
        }
    }
    
    // Add jitter (±25%)
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.75, 1.25);
    
    auto jittered_delay = std::chrono::seconds(
        static_cast<long>(base_delay.count() * dis(gen)));
    
    return std::chrono::system_clock::now() + jittered_delay;
}

bool QodNotificationManager::validate_sink(const std::string& sink) {
    // Validate HTTPS URL format
    std::regex url_regex("^https://[^\\s]+$");
    return std::regex_match(sink, url_regex);
}

bool QodNotificationManager::validate_credential(
    const std::optional<SinkCredential>& credential) {
    
    if (!credential) {
        return true; // No credential is valid
    }
    
    if (credential->credential_type != SinkCredential::CredentialType::ACCESSTOKEN) {
        logger_->warn("Unsupported credential type");
        return false;
    }
    
    // Check if token has expired
    if (credential->access_token_expires_utc) {
        if (*credential->access_token_expires_utc < std::chrono::system_clock::now()) {
            logger_->warn("Access token has expired");
            return false;
        }
    }
    
    return true;
}

void QodNotificationManager::update_stats(const std::string& session_id, bool success) {
    if (session_id.empty()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    auto& stats = session_stats_[session_id];
    stats.last_update = std::chrono::system_clock::now();
    
    if (success) {
        stats.successful++;
        if (stats.pending > 0) {
            stats.pending--;
        }
    } else {
        stats.pending++;
    }
    
    stats.total_sent = stats.successful + stats.failed;
}

void QodNotificationManager::cleanup_old_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    auto now = std::chrono::system_clock::now();
    auto cutoff = now - std::chrono::hours(24); // Keep stats for 24 hours
    
    auto it = session_stats_.begin();
    while (it != session_stats_.end()) {
        if (it->second.last_update < cutoff) {
            it = session_stats_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace qod
} // namespace af