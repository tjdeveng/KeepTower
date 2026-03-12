#pragma once

#include "../../utils/Log.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

#include <glibmm/dispatcher.h>

namespace KeepTower::YubiKeyInternal {

class AsyncRunner final {
public:
    AsyncRunner() noexcept {
        m_dispatcher.connect([this]() {
            std::function<void()> callback;
            {
                std::lock_guard<std::mutex> lock(m_callback_mutex);
                callback = std::move(m_pending_callback);
            }
            if (callback) {
                callback();
            }
        });
    }

    ~AsyncRunner() noexcept {
        shutdown();
    }

    AsyncRunner(const AsyncRunner&) = delete;
    AsyncRunner& operator=(const AsyncRunner&) = delete;
    AsyncRunner(AsyncRunner&&) = delete;
    AsyncRunner& operator=(AsyncRunner&&) = delete;

    [[nodiscard]] bool is_busy() const noexcept {
        return m_is_busy.load(std::memory_order_acquire);
    }

    void cancel() noexcept {
        if (is_busy()) {
            KeepTower::Log::warning("YubiKeyManager: Cancellation requested");
            m_cancel_requested.store(true, std::memory_order_release);
        }
    }

    void shutdown() noexcept {
        cancel();
        join();
    }

    void join() noexcept {
        if (m_worker_thread && m_worker_thread->joinable()) {
            m_worker_thread->join();
        }
    }

    template <typename WorkFn, typename UiFn>
    bool start(WorkFn&& work, UiFn&& ui_callback) {
        if (is_busy()) {
            return false;
        }

        // Wait for previous thread to finish
        join();

        m_is_busy.store(true, std::memory_order_release);
        m_cancel_requested.store(false, std::memory_order_release);

        m_worker_thread = std::make_unique<std::thread>(
            [this,
             work = std::forward<WorkFn>(work),
             ui_callback = std::forward<UiFn>(ui_callback)]() mutable {
                // Check for cancellation before starting
                if (m_cancel_requested.load(std::memory_order_acquire)) {
                    KeepTower::Log::info("YubiKeyManager: Operation cancelled before starting");
                    m_is_busy.store(false, std::memory_order_release);
                    return;
                }

                auto result = work();

                // Check for cancellation after operation
                if (m_cancel_requested.load(std::memory_order_acquire)) {
                    KeepTower::Log::info("YubiKeyManager: Operation cancelled after completion");
                    m_is_busy.store(false, std::memory_order_release);
                    return;
                }

                // Schedule callback on UI thread
                {
                    std::lock_guard<std::mutex> lock(m_callback_mutex);
                    m_pending_callback =
                        [ui_callback = std::move(ui_callback), result = std::move(result)]() mutable {
                            ui_callback(std::move(result));
                        };
                }

                m_is_busy.store(false, std::memory_order_release);
                m_dispatcher.emit();
            });

        return true;
    }

private:
    std::atomic<bool> m_is_busy{false};
    std::atomic<bool> m_cancel_requested{false};
    std::unique_ptr<std::thread> m_worker_thread;

    std::mutex m_callback_mutex;
    std::function<void()> m_pending_callback;
    Glib::Dispatcher m_dispatcher;
};

} // namespace KeepTower::YubiKeyInternal
