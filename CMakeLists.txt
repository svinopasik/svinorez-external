cmake_minimum_required(VERSION 3.16)

project(cs2-linux-hack
    VERSION 1.0.0
    DESCRIPTION "External CS2 Hack for Linux (ESP, Aimbot, Triggerbot, RCS, GUI)"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(X11 REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(XFT REQUIRED xft)  # freetype/fontconfig include paths for Xft headers

foreach(comp Xext Xfixes Xrender Xft Xrandr Xtst)
    if(NOT X11_${comp}_FOUND)
        message(FATAL_ERROR
            "X11 extension '${comp}' not found. Install: libxext-dev libxfixes-dev "
            "libxrender-dev libxft-dev libxrandr-dev libxtst-dev (Debian/Ubuntu) or "
            "libxext libxfixes libxrender libxft libxrandr libxtst (Arch)")
    endif()
endforeach()

add_library(cs2esp_common STATIC
    src/common/json.cpp
    src/common/config.cpp
    src/common/log.cpp
    src/common/memory.cpp
    src/common/ipc.cpp
)
target_include_directories(cs2esp_common PUBLIC include)
target_compile_options(cs2esp_common PRIVATE -Wall -Wextra)

add_executable(cs2espd
    src/cs2espd/main.cpp
    src/cs2espd/game.cpp
    src/cs2espd/esp.cpp
    src/cs2espd/overlay.cpp
    src/cs2espd/triggerbot.cpp
    src/cs2espd/aimbot.cpp
    src/cs2espd/rcs.cpp
    src/cs2espd/menu.cpp
)
target_include_directories(cs2espd PRIVATE ${XFT_INCLUDE_DIRS})
target_link_libraries(cs2espd PRIVATE
    cs2esp_common
    Threads::Threads
    ${XFT_LIBRARIES}
    ${X11_X11_LIB}
    ${X11_Xext_LIB}
    ${X11_Xfixes_LIB}
    ${X11_Xrender_LIB}
    ${X11_Xft_LIB}
    ${X11_Xrandr_LIB}
    ${X11_Xtst_LIB}
)
target_compile_options(cs2espd PRIVATE -Wall -Wextra)

add_executable(espctl
    src/espctl/main.cpp
)
target_link_libraries(espctl PRIVATE
    cs2esp_common
    Threads::Threads
)
target_compile_options(espctl PRIVATE -Wall -Wextra)
