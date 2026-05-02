#include "core/SessionId.h"

#include <atomic>
#include <chrono>
#include <random>

namespace mv {

namespace {

SessionId mint_session() {
    const auto t  = static_cast<std::uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count());
    std::random_device rd;
    return (t ^ (static_cast<std::uint64_t>(rd()) << 32)) | 1ULL;  // never zero
}

}  // namespace

SessionId IdGen::session() {
    static const SessionId s = mint_session();
    return s;
}

EventId IdGen::next_event() {
    static std::atomic<EventId> counter{1};
    return counter.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace mv
