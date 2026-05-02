#pragma once

#include "core/Messages.h"
#include "core/ModuleBase.h"
#include "core/SessionId.h"

#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace mv {

// Type-keyed pub/sub. Subscribers are ModuleBase pointers; publishers send a
// concrete message struct M. We fan out by enqueuing AnyMessage{M} to each
// subscriber's per-module queue.
//
// Threading: subscribe/unsubscribe and publish all take a single mutex. The
// fan-out copies happen while the lock is held — fine for a prototype with a
// handful of subscribers per topic. Heavier topics could move to a copy-on-
// write subscriber table.
class MessageBus {
public:
    template <typename M>
    void subscribe(ModuleBase* mod) {
        std::lock_guard<std::mutex> lk(mtx_);
        subs_[std::type_index(typeid(M))].push_back(mod);
    }

    void unsubscribe(ModuleBase* mod);

    template <typename M>
    void publish(M msg) {
        if (msg.header.event_id == 0) {
            msg.header.event_id = IdGen::next_event();
        }
        if (msg.header.session_id == 0) {
            msg.header.session_id = IdGen::session();
        }

        std::vector<ModuleBase*> targets;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            const auto it = subs_.find(std::type_index(typeid(M)));
            if (it == subs_.end()) {
                return;
            }
            targets = it->second;
        }
        AnyMessage am{std::move(msg)};
        for (ModuleBase* mod : targets) {
            mod->enqueue(am);
        }
    }

private:
    std::mutex mtx_;
    std::unordered_map<std::type_index, std::vector<ModuleBase*>> subs_;
};

}  // namespace mv
