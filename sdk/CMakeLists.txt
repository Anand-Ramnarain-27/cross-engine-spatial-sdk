include(FetchContent)

find_package(Threads REQUIRED)

# Used only inside DatasetSerializer.cpp; linked PRIVATE.
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

set(SPATIAL_SDK_TARGET spatial_sdk)

set(SPATIAL_SDK_PUBLIC_HEADERS
    include/spatial/Version.h
    include/spatial/Export.h
    include/spatial/Error.h
    include/spatial/core/Vec2.h
    include/spatial/core/Vec3.h
    include/spatial/core/Vec4.h
    include/spatial/core/Mat4.h
    include/spatial/core/Plane.h
    include/spatial/core/AABB.h
    include/spatial/core/Sphere.h
    include/spatial/core/Frustum.h
    include/spatial/core/CoordinateSystem.h
    include/spatial/core/SpatialIndex.h
    include/spatial/core/CameraParams.h
    include/spatial/lod/DistanceLOD.h
    include/spatial/lod/ScreenSpaceError.h
    include/spatial/lod/LODManager.h
    include/spatial/data/TileId.h
    include/spatial/data/Metadata.h
    include/spatial/data/Material.h
    include/spatial/data/Mesh.h
    include/spatial/data/TileLOD.h
    include/spatial/data/Tile.h
    include/spatial/data/DatasetManifest.h
    include/spatial/data/TileSerializer.h
    include/spatial/data/DatasetSerializer.h
    include/spatial/data/TileIndex.h
    include/spatial/streaming/ResourceState.h
    include/spatial/streaming/TileRequest.h
    include/spatial/streaming/RequestQueue.h
    include/spatial/streaming/WorkerPool.h
    include/spatial/streaming/StreamingManager.h
)

set(SPATIAL_SDK_PRIVATE_HEADERS
    src/data/BinaryIO.h
)

set(SPATIAL_SDK_SOURCES
    src/Version.cpp
    src/data/TileSerializer.cpp
    src/data/DatasetSerializer.cpp
    src/data/TileIndex.cpp
    src/streaming/RequestQueue.cpp
    src/streaming/WorkerPool.cpp
    src/streaming/StreamingManager.cpp
)

if(SPATIAL_SDK_BUILD_SHARED)
    add_library(${SPATIAL_SDK_TARGET} SHARED ${SPATIAL_SDK_SOURCES} ${SPATIAL_SDK_PUBLIC_HEADERS} ${SPATIAL_SDK_PRIVATE_HEADERS})
    target_compile_definitions(${SPATIAL_SDK_TARGET}
        PUBLIC SPATIAL_SDK_SHARED
        PRIVATE SPATIAL_SDK_EXPORTS
    )
else()
    add_library(${SPATIAL_SDK_TARGET} STATIC ${SPATIAL_SDK_SOURCES} ${SPATIAL_SDK_PUBLIC_HEADERS} ${SPATIAL_SDK_PRIVATE_HEADERS})
endif()

add_library(spatial::sdk ALIAS ${SPATIAL_SDK_TARGET})

target_link_libraries(${SPATIAL_SDK_TARGET}
    PRIVATE nlohmann_json::nlohmann_json
    PUBLIC Threads::Threads
)

target_include_directories(${SPATIAL_SDK_TARGET}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(${SPATIAL_SDK_TARGET} PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(${SPATIAL_SDK_TARGET} PRIVATE /W4)
    if(SPATIAL_SDK_WARNINGS_AS_ERRORS)
        target_compile_options(${SPATIAL_SDK_TARGET} PRIVATE /WX)
    endif()
else()
    target_compile_options(${SPATIAL_SDK_TARGET} PRIVATE -Wall -Wextra -Wpedantic)
    if(SPATIAL_SDK_WARNINGS_AS_ERRORS)
        target_compile_options(${SPATIAL_SDK_TARGET} PRIVATE -Werror)
    endif()
endif()
