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
    core/SpatialIndexTests.cpp
    core/SpatialIndexBenchmark.cpp
    data/ErrorTests.cpp
    data/TileIdTests.cpp
    data/TileSerializerTests.cpp
    data/DatasetSerializerTests.cpp
    data/TileIndexTests.cpp
    data/TileMemoryTests.cpp
    lod/ScreenSpaceErrorTests.cpp
    lod/DistanceLODTests.cpp
    lod/LODManagerTests.cpp
    streaming/ResourceStateTests.cpp
    streaming/RequestQueueTests.cpp
    streaming/WorkerPoolTests.cpp
    streaming/TileCacheTests.cpp
    streaming/StreamingManagerTests.cpp
    streaming/StreamingIntegrationTests.cpp
    rendering/ResourceHandleTests.cpp
    rendering/GPUResourceTests.cpp
    rendering/GPUUploadQueueTests.cpp
    rendering/StreamingRenderingIntegrationTests.cpp
    debug/DebugRendererTests.cpp
    SpatialWorldTests.cpp
    examples/FlyCameraTests.cpp
    tools/ProceduralCityTests.cpp
    ${CMAKE_SOURCE_DIR}/tools/TileBuilder/src/ProceduralCity.cpp
    ${CMAKE_SOURCE_DIR}/examples/StandaloneViewer/src/FlyCamera.cpp
)

add_executable(${SPATIAL_SDK_TEST_TARGET} ${SPATIAL_SDK_TEST_SOURCES})

target_include_directories(${SPATIAL_SDK_TEST_TARGET}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/tools/TileBuilder/src
        ${CMAKE_SOURCE_DIR}/examples/StandaloneViewer/src
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${SPATIAL_SDK_TEST_TARGET}
    PRIVATE
        spatial::sdk
        Catch2::Catch2WithMain
)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
# TEST_SPEC ~[.] excludes hidden-tagged tests (e.g. the SpatialIndex
# benchmarks) from the default ctest run; they still compile and can be run
# manually via spatial_sdk_tests.exe "[spatialindex][benchmark]".
catch_discover_tests(${SPATIAL_SDK_TEST_TARGET} TEST_SPEC ~[.])
