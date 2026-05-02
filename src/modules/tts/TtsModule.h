#pragma once

#include "core/MessageBus.h"
#include "core/ModuleBase.h"

namespace mv::modules {

// Voice command stub — re-emits as TtsRecognizedMsg the way a real STT engine
// would. No live mic input yet; tests/CLI can synthesise events by sending a
// TtsRecognizedMsg directly.
class TtsModule : public ModuleBase {
public:
    TtsModule();
    ~TtsModule() override;

protected:
    void on_init() override;
    void on_message(const AnyMessage& msg) override;
};

}  // namespace mv::modules
