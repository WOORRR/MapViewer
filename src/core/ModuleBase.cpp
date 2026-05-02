#include "core/ModuleBase.h"

#include "core/MessageBus.h"

#include <chrono>
#include <utility>

namespace mv {

ModuleBase::ModuleBase(std::string name) : name_(std::move(name)) {}

ModuleBase::~ModuleBase() {
    stop();
    if (bus_ != nullptr) {
        bus_->unsubscribe(this);
    }
}

void ModuleBase::attach(MessageBus* bus) {
    bus_ = bus;
    on_init();
}

void ModuleBase::start() {
    if (running_.exchange(true, std::memory_order_acq_rel)) {
        return;  // already running
    }
    worker_ = std::thread(&ModuleBase::run_worker, this);
}

void ModuleBase::stop() {
    if (!running_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    // Push a sentinel-empty enqueue so the worker wakes quickly; we use the
    // shutdown message as a generic wakeup.
    queue_.enqueue(AnyMessage{ShutdownMsg{}});
    if (worker_.joinable()) {
        worker_.join();
    }
}

void ModuleBase::enqueue(AnyMessage msg) {
    queue_.enqueue(std::move(msg));
}

std::size_t ModuleBase::drain_one() {
    AnyMessage msg;
    if (!queue_.try_dequeue(msg)) {
        return 0;
    }
    on_message(msg);
    return 1;
}

std::size_t ModuleBase::drain_all(std::size_t max) {
    std::size_t handled = 0;
    AnyMessage msg;
    while (handled < max && queue_.try_dequeue(msg)) {
        on_message(msg);
        ++handled;
    }
    return handled;
}

void ModuleBase::run_worker() {
    using namespace std::chrono_literals;
    AnyMessage msg;
    while (running_.load(std::memory_order_acquire)) {
        if (queue_.wait_dequeue_timed(msg, std::chrono::milliseconds(20))) {
            on_message(msg);
        } else {
            on_idle();
        }
    }
    // drain remaining
    while (queue_.try_dequeue(msg)) {
        on_message(msg);
    }
}

}  // namespace mv
