#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "af_orchestrator.h"
#include "southbound_handler_factory.h"

namespace af::app {

class BundledAfRuntime {
public:
    enum class LifecycleState {
        stopped,
        starting,
        running,
        stopping,
    };

    explicit BundledAfRuntime(std::string config_path);
    ~BundledAfRuntime();

    void initialize();
    void start();
    void wait();
    void requestShutdown();
    bool isRunning() const;
    LifecycleState getState() const;

private:
    void finalizeShutdown();

    std::string config_path_;
    std::unique_ptr<af::core::AfOrchestrator> orchestrator_;
    SouthboundHandlerList southbound_handlers_;
    std::vector<std::thread> southbound_threads_;
    std::thread cleanup_thread_;
    mutable std::mutex lifecycle_mutex_;
    std::condition_variable lifecycle_cv_;
    LifecycleState state_{LifecycleState::stopped};
};

} // namespace af::app
