# EmoPlay

Cross-platform desktop media player (Windows / macOS / Linux) built on
**libmpv** and **Qt6 / QML**. The goal is to combine mpv's playback power and
script ecosystem with a complete, modern GUI: media library, advanced video
quality controls, subtitles, plugins and full compatibility with existing
`mpv.conf` / `input.conf` / scripts.

See [docs/roadmap.md](docs/roadmap.md) for the phase plan and current status.

## Status

Early development (P1-P2). A minimal player already opens and plays files
with a custom QML control bar; CI builds and unit-tests every push on Linux.

## Download

Prebuilt artifacts are published on the
[GitHub Releases](https://github.com/emoeem/mpv-emo/releases) page.

The current [v0.1.0-alpha](https://github.com/emoeem/mpv-emo/releases/tag/v0.1.0-alpha)
is an **alpha / experimental prerelease** — expect rough edges. Linux builds
are compiled, unit-tested and smoke-tested in CI; Windows and macOS artifacts
are CI-built but not yet verified on real machines. No code signing or
notarization yet: the macOS bundle is ad-hoc signed only, so Gatekeeper will
prompt on first launch (right-click the app → *Open*, or run
`xattr -dr com.apple.quarantine /Applications/EmoPlay.app`).

| Platform | Asset |
|---|---|
| Linux (x86_64) | `EmoPlay-0.1.0-alpha-linux-x86_64.AppImage` |
| Windows (x86_64) | `EmoPlay-0.1.0-alpha-windows-x86_64.zip` |
| macOS (Apple Silicon) | `EmoPlay-0.1.0-alpha-macos.dmg` |

## Build (Linux)

```bash
sudo apt install build-essential cmake ninja-build pkg-config \
    qt6-base-dev qt6-declarative-dev libmpv-dev \
    libgl1-mesa-dev libegl1-mesa-dev \
    qml6-module-qtquick-controls qml6-module-qtquick-layouts \
    qml6-module-qtquick-window qml6-module-qtquick-dialogs \
    qml6-module-qtqml-workerscript qml6-module-qtquick-templates
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/emoplay path/to/video.mkv
```

## Run tests

```bash
ctest --test-dir build --output-on-failure
```

## License

EmoPlay code is MIT-licensed (see [LICENSE](LICENSE)). mpv/libmpv itself is
LGPLv2.1+ and is dynamically linked, which keeps the combined work
redistributable under those terms. Third-party shader and script ecosystems
keep their own licenses.
