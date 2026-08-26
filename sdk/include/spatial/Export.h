#pragma once

// Export/import macro for the optional shared-library build.

#if defined(_WIN32) && defined(SPATIAL_SDK_SHARED)
    #if defined(SPATIAL_SDK_EXPORTS)
        #define SPATIAL_API __declspec(dllexport)
    #else
        #define SPATIAL_API __declspec(dllimport)
    #endif
#else
    #define SPATIAL_API
#endif
