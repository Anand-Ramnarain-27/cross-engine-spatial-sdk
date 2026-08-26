# StandaloneViewer

The SDK's first complete demonstration and primary development/debugging
environment: a real window (Win32), a real `IRenderer` backend (Direct3D
11), and `StreamingManager` + `LODManager` + `DebugRenderer` wired together
against a real dataset on disk.

See [docs/getting_started.md](../../docs/getting_started.md) for how to
generate a dataset and run this, and
[docs/architecture.md](../../docs/architecture.md) for how the pieces fit
together.

## Controls

- `WASD` — move
- `Space` / `Ctrl` — up / down
- Hold right mouse button — look around
- `F1` — toggle debug tile-bounds visualization
- `Esc` — quit
