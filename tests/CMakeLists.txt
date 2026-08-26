include(FetchContent)

FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(catch2)

set(SPATIAL_SDK_TEST_TARGET spatial_sdk_tests)

set(SPATIAL_SDK_TEST_SOURCES
    core/VersionTests.cpp
    core/VectorTests.cpp
    core/Mat4Tests.cpp
    core/AABBTests.cpp
    core/SphereTests.cpp
    core/PlaneTests.cpp
    core/FrustumTests.cpp
    core/CoordinateSystemTests.cpp
)

add_executable(${SPATIAL_SDK_TEST_TARGET} ${SPATIAL_SDK_TEST_SOURCES})

target_link_libraries(${SPATIAL_SDK_TEST_TARGET}
    PRIVATE
        spatial::sdk
        Catch2::Catch2WithMain
)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(${SPATIAL_SDK_TEST_TARGET})
