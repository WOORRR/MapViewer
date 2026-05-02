#pragma once

#include "core/Messages.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>

// concurrentqueue header (vcpkg ships it under moodycamel/ prefix)
#include <moodycamel/blockingconcurrentqueue.h>

namespace mv {

class MessageBus;

// Base class for every sub-module. Owns one worker thread (unless run on the
// main thread, in which case the host pumps the queue via drain_all()).
//
// Lifecycle:
//   1. construct
//   2. attach(bus) — module records the bus pointer, registers subscriptions in on_init()
//   3. start() — spawns worker (skip for main-thread modules; pump via drain_all())
//   4. enqueue(...) is called by MessageBus::publish<T> on subscribers
//   5. stop() — joins the worker (no-op for main-thread modules)
class ModuleBase {
public:
    explicit ModuleBase(std::string name);
    virtual ~ModuleBase();

    ModuleBase(const ModuleBase&) = delete;
    ModuleBase& operator=(const ModuleBase&) = delete;

    void attach(MessageBus* bus);  // calls on_init()
    void start();                  // worker thread; opt-in
    void stop();                   // joins worker; safe to call multiple times

    void enqueue(AnyMessage msg);

    // For main-thread modules: pump up to `max` messages and return how many
    // were processed. drain_all returns when the queue is observed empty.
    std::size_t drain_one();
    std::size_t drain_all(std::size_t max = 1024);

    const std::string& name() const { return name_; }
    bool worker_running() const { return running_.load(std::memory_order_acquire); }

protected:
    virtual void on_init() {}                              // subscribe to topics here
    virtual void on_message(const AnyMessage& msg) = 0;    // handle one message
    virtual void on_idle() {}                              // called when queue empty in worker

    MessageBus* bus_{nullptr};

private:
    void run_worker();

    std::string name_;
    moodycamel::BlockingConcurrentQueue<AnyMessage> queue_;
    std::atomic<bool> running_{false};
    std::thread worker_;
};

}  // namespace mv
