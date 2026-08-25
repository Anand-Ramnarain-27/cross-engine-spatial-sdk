set(SPATIAL_SDK_TARGET spatial_sdk)

set(SPATIAL_SDK_PUBLIC_HEADERS
    include/spatial/Version.h
    include/spatial/Export.h
)

set(SPATIAL_SDK_SOURCES
    src/Version.cpp
)

if(SPATIAL_SDK_BUILD_SHARED)
    add_library(${SPATIAL_SDK_TARGET} SHARED ${SPATIAL_SDK_SOURCES} ${SPATIAL_SDK_PUBLIC_HEADERS})
    target_compile_definitions(${SPATIAL_SDK_TARGET}
        PUBLIC SPATIAL_SDK_SHARED
        PRIVATE SPATIAL_SDK_EXPORTS
    )
else()
    add_library(${SPATIAL_SDK_TARGET} STATIC ${SPATIAL_SDK_SOURCES} ${SPATIAL_SDK_PUBLIC_HEADERS})
endif()

add_library(spatial::sdk ALIAS ${SPATIAL_SDK_TARGET})

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
