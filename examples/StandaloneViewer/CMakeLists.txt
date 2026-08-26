if(NOT WIN32)
    message(STATUS "StandaloneViewer uses Win32/Direct3D 11 and is only built on Windows; skipping.")
    return()
endif()

add_executable(StandaloneViewer
    src/main.cpp
    src/Win32Window.h
    src/Win32Window.cpp
    src/D3D11Renderer.h
    src/D3D11Renderer.cpp
    src/FlyCamera.h
    src/FlyCamera.cpp
)

target_link_libraries(StandaloneViewer PRIVATE spatial::sdk d3d11 dxgi d3dcompiler)
target_compile_features(StandaloneViewer PRIVATE cxx_std_20)
target_compile_definitions(StandaloneViewer PRIVATE UNICODE _UNICODE)

if(MSVC)
    target_compile_options(StandaloneViewer PRIVATE /W4)
    if(SPATIAL_SDK_WARNINGS_AS_ERRORS)
        target_compile_options(StandaloneViewer PRIVATE /WX)
    endif()
endif()
