#pragma once

#include "core/MessageBus.h"
#include "core/ModuleBase.h"

namespace mv::modules {

// Sound stub. Logs SoundPlayMsg to stderr with priority handling so callers
// can see the queue order; future work plugs miniaudio or XAudio2 here.
class SoundModule : public ModuleBase {
public:
    SoundModule();
    ~SoundModule() override;

protected:
    void on_init() override;
    void on_message(const AnyMessage& msg) override;
};

}  // namespace mv::modules
