# The native VU0 maths' tests (see tests.cpp and native/MATH.md):
#
#   cmake -S native/math-tests -B build/native/math-tests -G Ninja && cmake --build build/native/math-tests
#   build/native/math-tests/math_tests [runs per function] [seed]
cmake_minimum_required(VERSION 3.24)
project(TwinsanityMathTests CXX)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(CMAKE_CXX_EXTENSIONS ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -gline-tables-only")
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# The retail microcode as a header, and the PS2 side's maths made the reference's C++
set(GENERATED ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(PS2 ${ROOT}/src/platform/ps2)
set(REFERENCE_SOURCES ${GENERATED}/ref_math.cpp ${GENERATED}/ref_matrices.cpp ${GENERATED}/ref_collisionmaths.cpp
    ${GENERATED}/ref_bounds.cpp)
add_custom_command(
    OUTPUT ${GENERATED}/microcode.h
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GENERATED}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/vudis.py --header ${GENERATED}/microcode.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/vudis.py ${ROOT}/assets/vutext.textbin.bin)
add_custom_command(
    OUTPUT ${REFERENCE_SOURCES}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_reference.py ${GENERATED}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_reference.py ${PS2}/math.cpp ${PS2}/matrices.cpp ${PS2}/collisionmaths.cpp
        ${PS2}/bounds.cpp)

file(GLOB NATIVE_SOURCES CONFIGURE_DEPENDS ${ROOT}/src/platform/native/math/*.cpp)
set(REFERENCE_CORE ${CMAKE_CURRENT_SOURCE_DIR}/reference/refvu0.cpp ${CMAKE_CURRENT_SOURCE_DIR}/reference/refasm.cpp)

add_executable(math_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests.cpp ${CMAKE_CURRENT_SOURCE_DIR}/pcsx2/probe_common.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pcsx2/probe_native.cpp ${NATIVE_SOURCES} ${REFERENCE_CORE} ${REFERENCE_SOURCES}
    ${GENERATED}/microcode.h)
target_include_directories(math_tests PRIVATE ${ROOT}/native/include ${ROOT}/include ${ROOT}/src/platform/native/math
    ${CMAKE_CURRENT_SOURCE_DIR}/reference ${CMAKE_CURRENT_SOURCE_DIR}/pcsx2 ${GENERATED})
target_compile_definitions(math_tests PRIVATE TWIN_NATIVE)
# The native build's flags; the reference's float code is told the rounding mode changes (it runs the host FPU rounding towards
# zero)
target_compile_options(math_tests PRIVATE -std=gnu++2c -fno-strict-aliasing -fno-delete-null-pointer-checks -fwrapv
    -ffp-contract=off -Wall -Wno-invalid-offsetof -Wno-unused-variable -Wno-unused-function -Wno-missing-braces)
set_source_files_properties(${REFERENCE_CORE} ${REFERENCE_SOURCES} PROPERTIES COMPILE_OPTIONS "-frounding-math")
