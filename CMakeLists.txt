cmake_minimum_required(VERSION 3.25)
project(mapviewer LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /utf-8 /permissive- /Zc:__cplusplus /Zc:preprocessor)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

find_package(Threads REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(concurrentqueue CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(GeographicLib CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

add_library(mv_core STATIC
    src/core/SessionId.cpp
    src/core/MessageBus.cpp
    src/core/ModuleBase.cpp
    src/core/MainModule.cpp
    src/geo/CoordTransform.cpp
    src/geo/Picking.cpp
    src/render/opengl/GLContext.cpp
    src/render/opengl/GLShader.cpp
    src/render/opengl/GLRenderer.cpp
    src/modules/render/RenderModule.cpp
    src/modules/config/ConfigModule.cpp
    src/modules/object/ObjectModule.cpp
    src/modules/data/DataModule.cpp
    src/modules/ui/UiModule.cpp
    src/modules/input/InputModule.cpp
    src/modules/locationlog/LocationLogModule.cpp
    src/modules/state/StateModule.cpp
    src/modules/log/LogModule.cpp
    src/modules/tts/TtsModule.cpp
    src/modules/sound/SoundModule.cpp
)
target_include_directories(mv_core PUBLIC src)
target_link_libraries(mv_core
    PUBLIC
        concurrentqueue::concurrentqueue
        Threads::Threads
        GeographicLib::GeographicLib
        glad::glad
        glm::glm
        glfw
        nlohmann_json::nlohmann_json
        imgui::imgui
)
if(WIN32)
    target_link_libraries(mv_core PUBLIC opengl32)
endif()

add_executable(mapviewer src/main.cpp)
target_link_libraries(mapviewer PRIVATE mv_core)

set_target_properties(mapviewer PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

enable_testing()

add_executable(test_messagebus tests/test_messagebus.cpp)
target_link_libraries(test_messagebus PRIVATE mv_core GTest::gtest GTest::gtest_main)
add_test(NAME test_messagebus COMMAND test_messagebus)

add_executable(test_coord tests/test_coord.cpp)
target_link_libraries(test_coord PRIVATE mv_core GTest::gtest GTest::gtest_main)
add_test(NAME test_coord COMMAND test_coord)

add_executable(test_picking tests/test_picking.cpp)
target_link_libraries(test_picking PRIVATE mv_core GTest::gtest GTest::gtest_main)
add_test(NAME test_picking COMMAND test_picking)
