cmake_minimum_required(VERSION 3.24)
project(guitar_mpe_transducer
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable testing
enable_testing()

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Fetch dependencies
include(cmake/Dependencies.cmake)

# Project include directory
include_directories(include)

# Transducer CLAP plugin library
add_library(guitar_mpe_transducer MODULE
    src/plugin_entry.cpp
)

target_link_libraries(guitar_mpe_transducer
    PRIVATE
        clap
)

target_include_directories(guitar_mpe_transducer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(guitar_mpe_transducer PROPERTIES
    PREFIX ""
    SUFFIX ".clap"
)

# Tests
add_subdirectory(tests)
