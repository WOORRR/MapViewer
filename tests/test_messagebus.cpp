#include "core/MainModule.h"
#include "core/MessageBus.h"
#include "core/Messages.h"
#include "core/ModuleBase.h"
#include "core/SessionId.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <set>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
namespace mv_test = ::mv;

namespace {

// A subscriber module that records every LogEvent it receives.
class CounterModule : public mv_test::ModuleBase {
public:
    explicit CounterModule(std::string name)
        : ModuleBase(std::move(name)) {}

    int seen() const { return seen_.load(std::memory_order_acquire); }
    std::set<mv_test::EventId> ids() {
        std::lock_guard<std::mutex> lk(mtx_);
        return ids_;
    }

protected:
    void on_init() override {
        bus_->subscribe<mv_test::LogEventMsg>(this);
    }

    void on_message(const mv_test::AnyMessage& msg) override {
        if (auto* log = std::get_if<mv_test::LogEventMsg>(&msg)) {
            {
                std::lock_guard<std::mutex> lk(mtx_);
                ids_.insert(log->header.event_id);
            }
            seen_.fetch_add(1, std::memory_order_release);
        }
        // Ignore ShutdownMsg etc.
    }

private:
    std::atomic<int> seen_{0};
    std::mutex mtx_;
    std::set<mv_test::EventId> ids_;
};

}  // namespace

TEST(MessageBus, FanOutToAllSubscribers) {
    constexpr int kPublishers   = 4;
    constexpr int kSubscribers  = 4;
    constexpr int kMsgsPerPub   = 10'000;

    mv_test::MainModule main_mod;

    std::vector<std::shared_ptr<CounterModule>> subs;
    subs.reserve(kSubscribers);
    for (int i = 0; i < kSubscribers; ++i) {
        auto m = std::make_shared<CounterModule>("counter-" + std::to_string(i));
        main_mod.register_module(m);
        subs.push_back(m);
    }

    main_mod.init(/*config_path*/ "");
    main_mod.start();

    std::vector<std::thread> publishers;
    publishers.reserve(kPublishers);
    for (int p = 0; p < kPublishers; ++p) {
        publishers.emplace_back([&main_mod, p]() {
            for (int i = 0; i < kMsgsPerPub; ++i) {
                mv_test::LogEventMsg msg;
                msg.level     = 2;
                msg.scope     = "test";
                msg.kv_json   = R"({"pub":)" + std::to_string(p) + "}";
                main_mod.bus().publish(msg);
            }
        });
    }
    for (auto& t : publishers) {
        t.join();
    }

    const int expected = kPublishers * kMsgsPerPub;
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    bool all_ready = false;
    while (std::chrono::steady_clock::now() < deadline) {
        all_ready = true;
        for (auto& s : subs) {
            if (s->seen() < expected) {
                all_ready = false;
                break;
            }
        }
        if (all_ready) {
            break;
        }
        std::this_thread::sleep_for(20ms);
    }

    main_mod.shutdown("test_done");

    for (std::size_t i = 0; i < subs.size(); ++i) {
        EXPECT_EQ(subs[i]->seen(), expected) << "subscriber " << i << " short";
        EXPECT_EQ(static_cast<int>(subs[i]->ids().size()), expected)
            << "subscriber " << i << " saw duplicate or missing event_ids";
    }
}

TEST(MessageBus, EventIdMonotonic) {
    const auto a = mv_test::IdGen::next_event();
    const auto b = mv_test::IdGen::next_event();
    const auto c = mv_test::IdGen::next_event();
    EXPECT_LT(a, b);
    EXPECT_LT(b, c);
}

TEST(MessageBus, AutoFillsHeader) {
    mv_test::MainModule main_mod;
    auto sub = std::make_shared<CounterModule>("hdr-check");
    main_mod.register_module(sub);
    main_mod.init("");
    main_mod.start();

    mv_test::LogEventMsg msg;  // header zero-initialised
    msg.scope = "auto";
    main_mod.bus().publish(msg);

    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (sub->seen() == 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(5ms);
    }
    main_mod.shutdown("test_done");

    EXPECT_EQ(sub->seen(), 1);
    const auto seen_ids = sub->ids();
    ASSERT_EQ(seen_ids.size(), 1u);
    EXPECT_NE(*seen_ids.begin(), 0u);  // event_id was filled in
}
