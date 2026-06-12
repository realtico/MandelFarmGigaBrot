cmake_minimum_required(VERSION 3.20)

project(MandelFarmGigaBrot
    VERSION 0.1.0
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(GNUInstallDirs)

set(MFGIGABROT_DEFAULT_NANQUIM_ROOT "${PROJECT_SOURCE_DIR}/../Nanquim")
if(DEFINED ENV{NANQUIM_ROOT})
    set(MFGIGABROT_DEFAULT_NANQUIM_ROOT "$ENV{NANQUIM_ROOT}")
endif()

set(NANQUIM_ROOT "${MFGIGABROT_DEFAULT_NANQUIM_ROOT}" CACHE PATH "Path to a local Nanquim checkout")
option(MFGIGABROT_BUILD_NANQUIM_DEMO "Build the Nanquim Mandelbrot demo when Nanquim is available" ON)

add_library(mandel_core STATIC
    src/mandel_core.c
    src/mandel_palette.c
    src/mandel_render.c
)

target_include_directories(mandel_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(mandel_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()

add_executable(test_mandel_core
    tests/unit/test_mandel_core.c
)

add_executable(test_mandel_types
    tests/unit/test_mandel_types.c
)

add_executable(test_mandel_render
    tests/unit/test_mandel_render.c
)

add_executable(test_mandel_palette
    tests/unit/test_mandel_palette.c
)

add_executable(mandel-render
    apps/mandel-render/main.c
)

target_include_directories(test_mandel_types
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(test_mandel_core
    PRIVATE
        mandel_core
)

target_link_libraries(test_mandel_render
    PRIVATE
        mandel_core
)

target_link_libraries(test_mandel_palette
    PRIVATE
        mandel_core
)

target_link_libraries(mandel-render
    PRIVATE
        mandel_core
)

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_mandel_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test_mandel_types PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test_mandel_render PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test_mandel_palette PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(mandel-render PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME mandel_core COMMAND test_mandel_core)
add_test(NAME mandel_types COMMAND test_mandel_types)
add_test(NAME mandel_render COMMAND test_mandel_render)
add_test(NAME mandel_palette COMMAND test_mandel_palette)
add_test(
    NAME mandel_render_cli
    COMMAND mandel-render
        --width 64
        --height 48
        --center-re -0.5
        --center-im 0.0
        --scale 3.0
        --max-iter 128
        --output "${CMAKE_BINARY_DIR}/mandel-smoke.ppm"
)

set_target_properties(mandel-render PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(MFGIGABROT_BUILD_NANQUIM_DEMO)
    if(EXISTS "${NANQUIM_ROOT}/CMakeLists.txt")
        set(NQ_BUILD_EXAMPLES OFF CACHE BOOL "Build Nanquim examples" FORCE)
        add_subdirectory("${NANQUIM_ROOT}" "${CMAKE_BINARY_DIR}/_deps/nanquim")

        add_executable(mandelbrot_demo
            examples/mandelbrot_demo.c
        )

        target_link_libraries(mandelbrot_demo
            PRIVATE
                mandel_core
                nanquim
        )

        if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(mandelbrot_demo PRIVATE -Wall -Wextra -Wpedantic -Wno-strict-prototypes)
        endif()

        set_target_properties(mandelbrot_demo PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        )
    else()
        message(STATUS "Nanquim not found at NANQUIM_ROOT='${NANQUIM_ROOT}'. Skipping mandelbrot_demo.")
    endif()
endif()
