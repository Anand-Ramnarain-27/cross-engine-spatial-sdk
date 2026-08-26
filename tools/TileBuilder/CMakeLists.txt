add_executable(SpatialTileBuilder
    src/main.cpp
    src/ProceduralCity.h
    src/ProceduralCity.cpp
)

target_link_libraries(SpatialTileBuilder PRIVATE spatial::sdk)
target_compile_features(SpatialTileBuilder PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(SpatialTileBuilder PRIVATE /W4)
    if(SPATIAL_SDK_WARNINGS_AS_ERRORS)
        target_compile_options(SpatialTileBuilder PRIVATE /WX)
    endif()
else()
    target_compile_options(SpatialTileBuilder PRIVATE -Wall -Wextra -Wpedantic)
    if(SPATIAL_SDK_WARNINGS_AS_ERRORS)
        target_compile_options(SpatialTileBuilder PRIVATE -Werror)
    endif()
endif()
