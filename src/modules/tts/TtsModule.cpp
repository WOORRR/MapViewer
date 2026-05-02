#include "modules/tts/TtsModule.h"

namespace mv::modules {

TtsModule::TtsModule() : ModuleBase("tts") {}
TtsModule::~TtsModule() = default;

void TtsModule::on_init() {
    // Subscribed to nothing for the prototype. A future SAPI-backed
    // implementation will publish TtsRecognizedMsg from a worker thread that
    // owns the recognition engine.
}

void TtsModule::on_message(const AnyMessage&) {}

}  // namespace mv::modules
