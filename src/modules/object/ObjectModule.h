#pragma once

#include "core/MessageBus.h"
#include "core/ModuleBase.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace mv::modules {

// Loads the object palette JSON on Bootstrap so other modules know what
// shapes/colours are available. Spawned instances are tracked here for the
// state module's UNDO/REDO. The actual mesh loading for OBJ files is added in
// step 11.
class ObjectModule : public ModuleBase {
public:
    struct PaletteEntry {
        std::string name;
        std::array<float, 4> color{1.0f, 1.0f, 1.0f, 1.0f};
        float scale{1.0f};
    };

    ObjectModule();
    ~ObjectModule() override;

    PaletteEntry palette(const std::string& name);

protected:
    void on_init() override;
    void on_message(const AnyMessage& msg) override;

private:
    void load_palette();

    std::mutex mtx_;
    std::unordered_map<std::string, PaletteEntry> palette_;
};

}  // namespace mv::modules
