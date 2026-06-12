cmake_minimum_required(VERSION 3.20)

project(MandelFarmGigaBrot
    VERSION 0.1.0
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(GNUInstallDirs)

add_library(mandel_core
    src/mandel_core.c
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

target_include_directories(test_mandel_types
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(test_mandel_core
    PRIVATE
        mandel_core
)

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_mandel_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test_mandel_types PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME mandel_core COMMAND test_mandel_core)
add_test(NAME mandel_types COMMAND test_mandel_types)
