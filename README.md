# EmoPlay

Cross-platform desktop media player (Windows / macOS / Linux) built on
**libmpv** and **Qt6 / QML**. The goal is to combine mpv's playback power and
script ecosystem with a complete, modern GUI: media library, advanced video
quality controls, subtitles, plugins and full compatibility with existing
`mpv.conf` / `input.conf` / scripts.

See [docs/roadmap.md](docs/roadmap.md) for the phase plan and current status.

## Status

Active development. Phases P0–P8 are implemented (player core, config
compatibility, quality engine, media library, music mode, plugin store);
P9 packaging/release pipeline and the update checker ship, and P10 test /
release hardening is ongoing. CI builds and unit-tests every push (22 test
suites) and smoke-boots the QML UI on Linux; see
[docs/testing-report.md](docs/testing-report.md) for coverage and the
real-machine test matrix.

## Features

- **Core playback** — libmpv embedded via QQuickFramebufferObject; audio /
  subtitle track switching, external subtitles, playlist, resume-aware play
  history.
- **mpv config compatibility (P4)** — first-run import of existing
  `mpv.conf` / `input.conf` / scripts with per-file decisions; embedding-
  critical options are forced and user config directories are never written.
- **Quality engine (P5)** — content-aware shader groups (Anime4K /
  SSimSuperRes), HDR→SDR tone-mapping, frame-drop auto-downgrade, plus
  VapourSynth frame-interpolation presets and ICC display profiles
  (honest external dependencies: scripts, runtimes and profiles stay yours).
- **Media library (P6)** — folder list with background scanning, searchable
  drawer grid, ffmpeg-generated thumbnails that degrade gracefully when
  ffmpeg is absent.
- **Music mode (P7)** — pure-audio detection, synced `.lrc` lyrics and a
  10-band equalizer with presets.
- **Plugin store (P8)** — `manager.json`-compatible manifests, built-in
  catalog, git-driven install / update / uninstall and online manifest
  sources; enabled plugins merge into mpv's `scripts` option without
  replacing your imported list.
- **Update checker (P9)** — one GitHub releases check per launch with a
  control-bar badge when a newer release exists; it never downloads or
  installs anything by itself.
- **Three-platform release pipeline (P9)** — a tag push builds and publishes
  Linux AppImage, Windows zip and macOS dmg automatically.

## Download

Prebuilt artifacts are published on the
[GitHub Releases](https://github.com/emoeem/mpv-emo/releases) page.

The current [v0.2.0-beta](https://github.com/emoeem/mpv-emo/releases/tag/v0.2.0-beta)
(published 2026-11-07) is a **beta prerelease** containing the player core,
config import (P4), quality engine v1 (P5) and media library (P6) — music
mode, the plugin store and the update checker are already merged on `main`
and will ship in the next release. Expect rough edges. Linux builds are
compiled, unit-tested and smoke-tested in CI; Windows and macOS artifacts
are CI-built but not yet verified on real machines.

| Platform | Asset |
|---|---|
| Linux (x86_64) | `EmoPlay-0.2.0-beta-linux-x86_64.AppImage` |
| Windows (x86_64) | `EmoPlay-0.2.0-beta-windows-x86_64.zip` |
| macOS (Apple Silicon) | `EmoPlay-0.2.0-beta-macos.dmg` |

**v1.0-rc is not released yet.** The rc cut is gated on the approved
optimization backlog ([docs/backlog.md](docs/backlog.md), R1–R6), the
real-machine test matrix in [docs/testing-report.md](docs/testing-report.md),
and code signing / notarization — none of which have landed. Earlier
prereleases (v0.1.0-alpha) remain available on the releases page.

No code signing or notarization on any prerelease: the macOS bundle is
ad-hoc signed only, so Gatekeeper will prompt on first launch (right-click
the app → *Open*, or run
`xattr -dr com.apple.quarantine /Applications/EmoPlay.app`).

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
