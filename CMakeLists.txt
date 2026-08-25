cmake_minimum_required(VERSION 3.21)

project(CrossEngineSpatialSDK
    VERSION 0.1.0
    DESCRIPTION "Cross-Engine Spatial Data Streaming SDK"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep all build output together regardless of which subdirectory defines the target.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(SPATIAL_SDK_BUILD_SHARED "Build the spatial SDK core as a shared library" OFF)
option(SPATIAL_SDK_BUILD_TESTS "Build the automated test suite" ON)
option(SPATIAL_SDK_BUILD_TOOLS "Build command-line tools (e.g. SpatialTileBuilder)" ON)
option(SPATIAL_SDK_BUILD_EXAMPLES "Build example / demo applications" ON)
option(SPATIAL_SDK_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_subdirectory(sdk)

if(SPATIAL_SDK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(SPATIAL_SDK_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

if(SPATIAL_SDK_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
