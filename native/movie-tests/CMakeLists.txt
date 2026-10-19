# The movies' pictures hashed (main.cpp): the native player's demultiplexer and disc reading with a main of its own. Built
# against our FFmpeg (native/tools/build_ffmpeg.sh's, the default) or another one's pkg-config folder, so two builds' hashes can be
# compared:
#
#   cmake -S native/movie-tests -B build/native/movie-tests -G Ninja && cmake --build build/native/movie-tests
#   cmake -S native/movie-tests -B build/native/movie-tests-system -G Ninja -DTWIN_FFMPEG_DIR=/opt/homebrew/opt/ffmpeg
cmake_minimum_required(VERSION 3.24)
project(TwinsanityMovieTests CXX)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
find_package(PkgConfig REQUIRED)
if(APPLE)
    set(TWIN_FFMPEG_OS macos)
else()
    string(TOLOWER ${CMAKE_SYSTEM_NAME} TWIN_FFMPEG_OS)
endif()
set(TWIN_FFMPEG_ARCH ${CMAKE_SYSTEM_PROCESSOR})
if(TWIN_FFMPEG_ARCH STREQUAL "aarch64")
    set(TWIN_FFMPEG_ARCH arm64)
endif()
set(TWIN_FFMPEG_DIR ${ROOT}/build/native/ffmpeg/${TWIN_FFMPEG_OS}-${TWIN_FFMPEG_ARCH} CACHE PATH "The FFmpeg to test")
set(ENV{PKG_CONFIG_LIBDIR} ${TWIN_FFMPEG_DIR}/lib/pkgconfig)
set(ENV{PKG_CONFIG_PATH} "")
# Found afresh each time (pkg-config's answer and the libraries' paths are cached otherwise), so TWIN_FFMPEG_DIR can change
set(FFMPEG_FOUND "" CACHE INTERNAL "")
get_cmake_property(TWIN_CACHED CACHE_VARIABLES)
foreach(variable ${TWIN_CACHED})
    if(variable MATCHES "^pkgcfg_lib_FFMPEG_")
        unset(${variable} CACHE)
    endif()
endforeach()
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavcodec libavutil)
message(STATUS "FFmpeg: ${TWIN_FFMPEG_DIR} (libavcodec ${FFMPEG_libavcodec_VERSION})")

add_executable(movie-tests main.cpp ${ROOT}/src/platform/native/movie/pss.cpp ${ROOT}/src/platform/native/disc.cpp)
target_include_directories(movie-tests PRIVATE ${ROOT}/native/include ${ROOT}/include ${ROOT}/src/platform/native)
target_compile_definitions(movie-tests PRIVATE TWIN_NATIVE)
target_compile_options(movie-tests PRIVATE -std=gnu++2c -fno-exceptions -fno-rtti -Wall)
target_link_libraries(movie-tests PRIVATE PkgConfig::FFMPEG)
