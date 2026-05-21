#include "bundled_af_runtime.h"

#include <chrono>
#include <exception>
#include <iostream>
#include <stdexcept>

#include "af_component.h"

namespace af::app {

namespace {
constexpr auto kCleanupInterval = std::chrono::minutes(10);
}

BundledAfRuntime::BundledAfRuntime(std::string config_path)
    : config_path_(std::move(config_path)) {}

BundledAfRuntime::~BundledAfRuntime() {
    try {
        requestShutdown();
        finalizeShutdown();
    } catch (...) {
        // Best-effort shutdown only.
    }
}

void BundledAfRuntime::initialize() {
    {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);
        if (state_ != LifecycleState::stopped) {
            throw std::logic_error("BundledAfRuntime::initialize() requires the runtime to be stopped");
        }
    }

    std::cout << "[1/2] Initializing southbound handlers..." << std::endl;
    southbound_handlers_ = create_southbound_handlers(config_path_);

    if (southbound_handlers_.empty()) {
        std::cout << "  No southbound handlers active after applying build-time and runtime configuration." << std::endl;
    } else {
        for (auto& handler : southbound_handlers_) {
            std::cout << "  - Initializing " << handler->getName() << "..." << std::endl;
            handler->initialize();
        }
        std::cout << "  Southbound handlers initialized." << std::endl;
    }

    std::cout << "[2/2] Initializing AF Core (orchestrator)..." << std::endl;
    orchestrator_ = std::make_unique<af::core::AfOrchestrator>(config_path_);
    orchestrator_->initialize();
    std::cout << "  AF Core initialized." << std::endl;
}

void BundledAfRuntime::start() {
    {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);
        if (state_ != LifecycleState::stopped) {
            throw std::logic_error("BundledAfRuntime::start() requires the runtime to be stopped");
        }
        if (!orchestrator_) {
            throw std::logic_error("BundledAfRuntime::start() requires initialize() to run first");
        }
        state_ = LifecycleState::starting;
    }
    lifecycle_cv_.notify_all();

    std::cout << "Starting bundled AF Core and southbound components..." << std::endl;

    southbound_threads_.clear();
    southbound_threads_.reserve(southbound_handlers_.size());
    try {
        for (auto& handler : southbound_handlers_) {
            southbound_threads_.emplace_back([this, handler = handler.get()]() {
                try {
                    handler->start();
                } catch (const std::exception& e) {
                    std::cerr << handler->getName() << " error: " << e.what() << std::endl;
                    requestShutdown();
                }
            });
        }

        orchestrator_->start();
        cleanup_thread_ = std::thread([this]() {
            while (true) {
                std::unique_lock<std::mutex> lock(lifecycle_mutex_);
                const bool stopping = lifecycle_cv_.wait_for(lock, kCleanupInterval, [this]() {
                    return state_ == LifecycleState::stopping ||
                           state_ == LifecycleState::stopped;
                });

                if (stopping) {
                    return;
                }

                auto* orchestrator = orchestrator_.get();
                lock.unlock();

                if (!orchestrator) {
                    continue;
                }

                auto subscription_manager = orchestrator->get_subscription_manager();
                if (subscription_manager) {
                    int removed = subscription_manager->cleanup_expired_subscriptions();
                    if (removed > 0) {
                        std::cout << "Cleaned up " << removed << " expired subscriptions" << std::endl;
                    }
                }
            }
        });

        {
            std::lock_guard<std::mutex> lock(lifecycle_mutex_);
            if (state_ == LifecycleState::starting) {
                state_ = LifecycleState::running;
            }
        }
        lifecycle_cv_.notify_all();
    } catch (const std::exception& e) {
        std::cerr << "AF Core error: " << e.what() << std::endl;
        requestShutdown();
        finalizeShutdown();
        throw;
    }
}

void BundledAfRuntime::wait() {
    if (orchestrator_) {
        orchestrator_->wait();
    }

    requestShutdown();
    finalizeShutdown();
}

void BundledAfRuntime::finalizeShutdown() {
    for (auto& southbound_thread : southbound_threads_) {
        if (southbound_thread.joinable()) {
            southbound_thread.join();
        }
    }
    southbound_threads_.clear();

    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }

    orchestrator_.reset();
    southbound_handlers_.clear();

    {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);
        state_ = LifecycleState::stopped;
    }
    lifecycle_cv_.notify_all();
}

void BundledAfRuntime::requestShutdown() {
    {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);
        if (state_ == LifecycleState::stopping || state_ == LifecycleState::stopped) {
            return;
        }
        state_ = LifecycleState::stopping;
    }
    lifecycle_cv_.notify_all();

    if (orchestrator_) {
        std::cout << "Stopping AF Core..." << std::endl;
        orchestrator_->stop();
    }

    for (auto it = southbound_handlers_.rbegin(); it != southbound_handlers_.rend(); ++it) {
        if (*it) {
            std::cout << "Stopping " << (*it)->getName() << "..." << std::endl;
            (*it)->stop();
        }
    }
}

bool BundledAfRuntime::isRunning() const {
    const auto state = getState();
    return state == LifecycleState::starting || state == LifecycleState::running;
}

BundledAfRuntime::LifecycleState BundledAfRuntime::getState() const {
    std::lock_guard<std::mutex> lock(lifecycle_mutex_);
    return state_;
}

} // namespace af::app
