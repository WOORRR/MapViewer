#include "modules/sound/SoundModule.h"

#include <cstdio>

namespace mv::modules {

SoundModule::SoundModule() : ModuleBase("sound") {}
SoundModule::~SoundModule() = default;

void SoundModule::on_init() {
    bus_->subscribe<SoundPlayMsg>(this);
}

void SoundModule::on_message(const AnyMessage& msg) {
    if (auto* s = std::get_if<SoundPlayMsg>(&msg)) {
        const char* mode = (s->mode == SoundPlayMsg::Mode::Queue)   ? "queue"
                          : (s->mode == SoundPlayMsg::Mode::Preempt) ? "preempt"
                                                                       : "play";
        std::fprintf(stderr, "[sound:%s] %s prio=%d\n", mode, s->clip_id.c_str(), s->priority);
    }
}

}  // namespace mv::modules
