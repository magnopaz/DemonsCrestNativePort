cmake_minimum_required(VERSION 3.20)
project(DemonsCrestNativePort C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(FetchContent)

# raylib is fetched at configure time; it is not vendored.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(raylib)

add_executable(demonscrest
    src/main.c
    src/game.c
    src/assets.c
    src/anim.c
    src/level.c
    src/input.c
    src/player.c
    src/projectile.c
    src/object.c
    src/camera.c
    src/render.c
    external/cJSON/cJSON.c
)

target_include_directories(demonscrest PRIVATE
    src
    external/cJSON
)

target_link_libraries(demonscrest PRIVATE raylib m)

if(MSVC)
    target_compile_options(demonscrest PRIVATE /W4)
else()
    target_compile_options(demonscrest PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# The generated ROM-derived assets are intentionally not part of the build.
# They are created by tools/dc_extract/extract.py and loaded at runtime.
add_custom_target(extract-assets
    COMMAND ${CMAKE_COMMAND} -E echo "Run: python3 tools/dc_extract/extract.py --rom <rom> --dump captures/graveyard --area 1 --screens 9,10 --out assets/generated"
    VERBATIM
)
