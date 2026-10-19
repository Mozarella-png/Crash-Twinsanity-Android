# The sound's test harness: the native build's objects (less its entry) with a main of its own that drives Platform::Audio and
# Platform::Stream the way the game's sound code does and writes what the emulated SPU2 makes to WAV files. See native/AUDIO.md.
#
#   cmake -S native/audio-tests -B build/native/audio-tests -G Ninja && cmake --build build/native/audio-tests
#   build/native/audio-tests/audio-tests music 8 20 out.wav
cmake_minimum_required(VERSION 3.24)
project(TwinsanityAudioTests CXX)

find_package(SDL3 REQUIRED CONFIG)
find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. native)
set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

add_library(audio_tests_main OBJECT main.cpp)
set(HARNESS_OBJECTS "$<FILTER:$<TARGET_OBJECTS:game>,EXCLUDE,/entry\\.cpp\\.o$>")
set(HARNESS_STUBS ${CMAKE_CURRENT_BINARY_DIR}/stubs.cpp)
add_custom_command(
    OUTPUT ${HARNESS_STUBS}
    COMMAND ${Python3_EXECUTABLE} native/tools/make_stubs.py ${HARNESS_STUBS} ${HARNESS_OBJECTS}
            $<TARGET_OBJECTS:audio_tests_main>
    WORKING_DIRECTORY ${ROOT}
    DEPENDS game audio_tests_main ${ROOT}/native/tools/make_stubs.py
    COMMAND_EXPAND_LISTS)
add_executable(audio-tests $<TARGET_OBJECTS:audio_tests_main> ${HARNESS_OBJECTS} ${HARNESS_STUBS})
foreach(target audio_tests_main audio-tests)
target_include_directories(${target} PRIVATE ${ROOT}/native/include ${ROOT}/include ${ROOT}/src/platform/native)
target_compile_definitions(${target} PRIVATE TWIN_NATIVE)
target_compile_options(${target} PRIVATE -std=gnu++2c -fno-exceptions -fno-rtti -Wall -Wno-unused-function)
target_link_libraries(${target} PRIVATE SDL3::SDL3 OpenGL::GL PkgConfig::FFMPEG)
endforeach()
