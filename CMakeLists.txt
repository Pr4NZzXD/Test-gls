cmake_minimum_required(VERSION 3.20)
project(GrannyChams C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
        src
        src/icons
        src/menu
        src/modules
        font
        vendor/imgui
)

set(IMGUI_SOURCES
        vendor/imgui/imgui.cpp
        vendor/imgui/imgui_draw.cpp
        vendor/imgui/imgui_tables.cpp
        vendor/imgui/imgui_widgets.cpp
        vendor/imgui/imgui_impl_opengl3.cpp
)

set(KAHANIUM_SOURCES
        src/main.cpp
        src/il2cpp_api.cpp
        src/config.cpp
        src/menu/menu.cpp
        src/modules/modules.cpp
        src/modules/esp_enemies.cpp
        src/modules/esp_items.cpp
        src/modules/flat_textures.cpp
        src/modules/game_unlocks.cpp
        src/modules/auto_farm.cpp
        src/modules/player_mods.cpp
        src/modules/reshade.cpp
        src/modules/scene_explorer.cpp
        src/modules/speedhack.cpp
        src/modules/Aim.cpp
        src/modules/Aim.h
        src/modules/logger.cpp
        src/modules/logger.h
        src/modules/macro.cpp
        src/modules/speedrun.cpp
        src/modules/speedrun.h
        src/modules/macro.h
        src/menu/ui_theme.h
        src/menu/ui_anim.cpp
        src/menu/ui_anim.h
        src/menu/ui_widgets.cpp
        src/menu/ui_widgets.h
)

add_library(GrannyChams SHARED
        ${IMGUI_SOURCES}
        ${KAHANIUM_SOURCES}
)

target_link_libraries(GrannyChams PRIVATE
        GLESv3
        EGL
        log
        dl
        atomic
        m
        android
)