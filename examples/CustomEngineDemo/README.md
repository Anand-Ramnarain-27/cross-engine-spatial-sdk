# CustomEngineDemo

Unlike `examples/UnityDemo` and `examples/UnrealDemo`, the custom-engine
integration (Phase 12) doesn't live in this folder — it lives in the
consuming engine's own repository, since a custom C++ engine integration
means editing that engine's own source, not shipping a self-contained
demo project here.

This repo's side of it is just `sdk/CMakeLists.txt`'s
`SPATIAL_SDK_CUSTOM_ENGINE_STAGE_DIR` option, which stages the built
library and headers into a consuming project. See
[docs/custom_engine_integration.md](../../docs/custom_engine_integration.md)
for the full design and the concrete result.
