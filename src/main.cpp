// MapViewer prototype entry point.
//
// Constructs the MainModule (= bus + module registry), registers the render
// module on the main thread, and pumps the frame loop until the GL window is
// asked to close.

#include "core/MainModule.h"
#include "modules/config/ConfigModule.h"
#include "modules/data/DataModule.h"
#include "modules/input/InputModule.h"
#include "modules/locationlog/LocationLogModule.h"
#include "modules/log/LogModule.h"
#include "modules/object/ObjectModule.h"
#include "modules/render/RenderModule.h"
#include "modules/sound/SoundModule.h"
#include "modules/state/StateModule.h"
#include "modules/tts/TtsModule.h"
#include "modules/ui/UiModule.h"

#include <cstdio>
#include <exception>
#include <memory>

int main() {
    try {
        mv::MainModule app;

        auto render = std::make_shared<mv::modules::RenderModule>();
        auto config = std::make_shared<mv::modules::ConfigModule>();
        auto object = std::make_shared<mv::modules::ObjectModule>();
        auto data   = std::make_shared<mv::modules::DataModule>();
        auto ui     = std::make_shared<mv::modules::UiModule>();
        auto input  = std::make_shared<mv::modules::InputModule>();
        auto loc    = std::make_shared<mv::modules::LocationLogModule>();
        auto state  = std::make_shared<mv::modules::StateModule>();
        auto log    = std::make_shared<mv::modules::LogModule>();
        auto tts    = std::make_shared<mv::modules::TtsModule>();
        auto sound  = std::make_shared<mv::modules::SoundModule>();

        render->set_ui_module(ui.get());

        // Render must run on main thread (GL context). The rest get worker threads.
        app.register_module(render, /*main_thread=*/true);
        app.register_module(ui,     /*main_thread=*/false);
        app.register_module(input,  /*main_thread=*/false);
        app.register_module(config, /*main_thread=*/false);
        app.register_module(object, /*main_thread=*/false);
        app.register_module(data,   /*main_thread=*/false);
        app.register_module(loc,    /*main_thread=*/false);
        app.register_module(state,  /*main_thread=*/false);
        app.register_module(log,    /*main_thread=*/false);
        app.register_module(tts,    /*main_thread=*/false);
        app.register_module(sound,  /*main_thread=*/false);

        app.init(/*config_path=*/"assets/config/settings.json");
        app.start();

        while (!render->should_close()) {
            render->frame();
        }

        app.shutdown("user_close");
        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "fatal: %s\n", ex.what());
        return 1;
    }
}
