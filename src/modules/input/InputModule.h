#pragma once

#include "core/MessageBus.h"
#include "core/ModuleBase.h"

namespace mv::modules {

// Parses CLI text submitted by the UI/TTS layers and routes the request as a
// structured message on the bus. Currently supports:
//   goto <lat> <lon>         — teleport camera to WGS84 lat/lon
//   teleport <x> <y> [z]     — teleport camera to UTM-K x,y(,z)
//   fov <int>                — set FoV delta (-60..+60)
//   load_obj <path>          — request OBJ spawn
//   undo / redo              — push UndoRequest / RedoRequest
//   trail on / trail off     — toggle location-log trail (forwarded as ConfigUpdatedMsg)
class InputModule : public ModuleBase {
public:
    InputModule();
    ~InputModule() override;

protected:
    void on_init() override;
    void on_message(const AnyMessage& msg) override;

private:
    void handle_cli(const std::string& line);
};

}  // namespace mv::modules
