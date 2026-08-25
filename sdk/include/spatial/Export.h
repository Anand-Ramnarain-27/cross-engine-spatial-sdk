#pragma once

// Cross-platform export/import macro for the spatial SDK.
//
// The SDK can be built as either a static library (default) or a shared
// library (SPATIAL_SDK_BUILD_SHARED=ON). Consumers that link the shared
// build get correct dllimport decoration on Windows automatically via the
// SPATIAL_SDK_SHARED compile definition propagated by CMake; the SDK's own
// translation units additionally define SPATIAL_SDK_EXPORTS to switch to
// dllexport.

#if defined(_WIN32) && defined(SPATIAL_SDK_SHARED)
    #if defined(SPATIAL_SDK_EXPORTS)
        #define SPATIAL_API __declspec(dllexport)
    #else
        #define SPATIAL_API __declspec(dllimport)
    #endif
#else
    #define SPATIAL_API
#endif
