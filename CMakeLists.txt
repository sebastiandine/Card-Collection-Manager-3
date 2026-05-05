cmake_minimum_required(VERSION 3.22)

# Detect the toolchain BEFORE project() so we can pick clang or fall back to
# MinGW-w64 g++ on Windows. This file is no-op on non-Windows hosts.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Toolchain.cmake)

project(ccm
    VERSION 0.1.0
    DESCRIPTION "Card Collection Manager 3 - C++/wxWidgets desktop app"
    LANGUAGES CXX
)

if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Generate compile_commands.json for tooling (clangd, ccls, etc.).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# All built artifacts land in a predictable place.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Project-wide options.
option(CCM_USE_SYSTEM_WX "Use a system-installed wxWidgets via find_package instead of FetchContent" OFF)
option(CCM_BUILD_TESTS "Build unit tests for ccm_core" ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Dependencies.cmake)

add_subdirectory(core)
add_subdirectory(ui_wx)
add_subdirectory(app)

if(CCM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "")
message(STATUS "============================================================")
message(STATUS "  ${PROJECT_NAME} ${PROJECT_VERSION}")
message(STATUS "  Compiler:        ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "  System wx:       ${CCM_USE_SYSTEM_WX}")
message(STATUS "  Build tests:     ${CCM_BUILD_TESTS}")
message(STATUS "============================================================")
message(STATUS "")
