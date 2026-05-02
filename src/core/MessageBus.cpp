#include "core/MessageBus.h"

#include <algorithm>

namespace mv {

void MessageBus::unsubscribe(ModuleBase* mod) {
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& [_, vec] : subs_) {
        vec.erase(std::remove(vec.begin(), vec.end(), mod), vec.end());
    }
}

}  // namespace mv
