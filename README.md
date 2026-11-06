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
