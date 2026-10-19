# The native graphics' harness: the native build's objects (the game, the platform layer, the retail data) with the harness's
# own entry in place of the game's (native/graphics-harness/harness.cpp). It writes the PCRTC's pictures as PNGs.
#
#   cmake -S native/graphics-harness -B build/native/harness -G Ninja && cmake --build build/native/harness
#   build/native/harness/graphics-harness build/native/harness/out
#
# Run it from the repository's folder (local.json gives the disc image). The retail data is the native build's
# (build/native/data.cpp, which its build makes).
cmake_minimum_required(VERSION 3.24)
project(TwinsanityGraphicsHarness CXX)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(CMAKE_CXX_EXTENSIONS ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -gline-tables-only -DNDEBUG")

find_package(SDL3 REQUIRED CONFIG)
find_package(OpenGL REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
# The movies' pictures (src/platform/native/movie/): FFmpeg's libavcodec, as the game links it
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET GLOBAL libavcodec libavutil)

file(GLOB GAME_SOURCES CONFIGURE_DEPENDS ${ROOT}/src/game/*.cpp)
set(CORE_SOURCES ${ROOT}/src/debug.cpp ${ROOT}/src/gcc2.cpp ${ROOT}/src/main.cpp)
file(STRINGS ${ROOT}/native/reused_ps2.txt REUSED_PS2 REGEX "^src/")
list(TRANSFORM REUSED_PS2 PREPEND ${ROOT}/)
file(GLOB_RECURSE PLATFORM_SOURCES CONFIGURE_DEPENDS ${ROOT}/src/platform/native/*.cpp)
list(FILTER PLATFORM_SOURCES EXCLUDE REGEX "/src/platform/native/entry\\.cpp$")
# The native build's generated C++ (the retail data, the scripts' layouts...), which its build makes; not its stand-ins
file(GLOB DATA_CPP CONFIGURE_DEPENDS ${ROOT}/build/native/*.cpp)
list(FILTER DATA_CPP EXCLUDE REGEX "/stubs\\.cpp$")

add_library(harnessobjects OBJECT ${GAME_SOURCES} ${CORE_SOURCES} ${REUSED_PS2} ${PLATFORM_SOURCES} ${DATA_CPP}
            ${CMAKE_CURRENT_SOURCE_DIR}/harness.cpp)
set(STUBS_CPP ${CMAKE_CURRENT_BINARY_DIR}/stubs.cpp)
add_custom_command(
    OUTPUT ${STUBS_CPP}
    COMMAND ${Python3_EXECUTABLE} native/tools/make_stubs.py ${STUBS_CPP} $<TARGET_OBJECTS:harnessobjects>
    WORKING_DIRECTORY ${ROOT}
    DEPENDS harnessobjects $<TARGET_OBJECTS:harnessobjects> ${ROOT}/native/tools/make_stubs.py
    COMMAND_EXPAND_LISTS)
add_executable(graphics-harness $<TARGET_OBJECTS:harnessobjects> ${STUBS_CPP})

foreach(target harnessobjects graphics-harness)
target_include_directories(${target} PRIVATE ${ROOT}/native/include ${ROOT}/include ${ROOT}/src/platform/native)
target_compile_definitions(${target} PRIVATE TWIN_NATIVE)
target_compile_options(${target} PRIVATE
    -std=gnu++2c -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-strict-aliasing -fno-delete-null-pointer-checks -fwrapv
    -ffp-contract=off -Wall -Wno-invalid-offsetof -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable
    -Wno-unknown-pragmas -Wno-missing-braces -Wno-mismatched-tags)
target_link_libraries(${target} PRIVATE SDL3::SDL3 OpenGL::GL PkgConfig::FFMPEG)
endforeach()
