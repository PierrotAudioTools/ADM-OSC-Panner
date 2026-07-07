# ADM-OSC Panner

## VST3 / AU plugin for macOS and Windows

Receive, transmit and record ADM-OSC messages from any compatible DAW. Designed for immersive audio workflows using the [ADM-OSC](https://immersive-audio-live.github.io/ADM-OSC/) open protocol.

![ADM-OSC Panner](Assets/ADM-OSC%20Panner-fullview.png)

---

## Features

- **OSC input** — Create an ADM-OSC Server (default: 4001)
- **OSC output** — send to any IP and port (default: 127.0.0.1:4001)
- **ADM-OSC protocol** — Cartesian and Polar coordinate formats
- **Object-based positioning** — per-object XYZ control
- **Visual panner** — real-time Top View and Rear View display
- **DAW automation** — all parameters are recordable and automatable

## Requirements

- macOS 12 or later
- Apple Silicon (ARM) — M1/M2/M3/M4
- Compatible DAW (Logic Pro, Reaper, Nuendo, etc.)

### Windows build requirements

- Windows 10 or later
- Visual Studio 2022 Build Tools or Visual Studio 2022 with C++ support
- CMake 3.20 or later
- A VST3-compatible DAW such as Reaper, Cubase or Nuendo

## Installation

Download the installer from the [Releases](../../releases) page:

| Installer | Format |
| --------- | ------ |
| `ADM-OSC Panner … AU.pkg` | Audio Unit (AU) only |
| `ADM-OSC Panner … VST3.pkg` | VST3 only |
| `ADM-OSC Panner … AU-VST3.pkg` | AU + VST3 |

Double-click the `.pkg` file and follow the installer steps. The plugin is installed in the standard system plug-in folders:

- AU → `/Library/Audio/Plug-Ins/Components/`
- VST3 → `/Library/Audio/Plug-Ins/VST3/`

Restart your DAW after installation.

## Building on Windows

This repository can build a Windows VST3 plugin with CMake and Visual Studio 2022.

### Configure

```powershell
cmake -S . -B build-win -G "Visual Studio 17 2022" -A x64
```

### Build

```powershell
cmake --build build-win --config Release --target ADM_OSC_Music_Panner_VST3
```

### Build output

The built plugin is generated here:

```text
build-win/ADM_OSC_Music_Panner_artefacts/Release/VST3/ADM-OSC Panner.vst3
```

### Install on Windows

Copy the built `.vst3` bundle to:

```text
C:\Program Files\Common Files\VST3\
```

Administrator permission may be required to copy into that folder. Restart your DAW after installation.

## Usage

1. Insert **ADM-OSC Panner** on any audio track
2. Set **OSC In Port** to the port your source is sending to (default: 4001)
3. Set **OSC Out IP / Port** to your destination (default: 127.0.0.1:4001)
4. Select the **Object** number
5. Move the XYZ position — the Top and Rear views update in real time
6. Automate X, Y, Z in your DAW timeline to record and playback movement
7. Disable / Enable OSC input or output by clicking on "OSC In" or "OSC Out" in the OSC tab
8. When multiple instances share the same OSC input port, they automatically form a **HUB**: only one instance listens on the port and redistributes the incoming messages to all others.
9. When used in Polar mode, the plugin automatically converts the incoming polar coordinates to Cartesian before processing.

## OSC Address Format

Supported OSC input/output address variants:

```text
CARTESIAN
/adm/obj/<id>/x      x
/adm/obj/<id>/y      y
/adm/obj/<id>/z      z
/adm/obj/<id>/xy     x y
/adm/obj/<id>/xyz    x y z

POLAR
/adm/obj/<id>/a      a
/adm/obj/<id>/e      e
/adm/obj/<id>/d      d
/adm/obj/<id>/aed    a e d
```

## License

ADM-OSC Panner is open source software released under the GNU General Public License v3.0.
See [`LICENSE`](LICENSE) for the full license text.

## Beta Disclaimer

ADM-OSC Panner is currently in beta. Use at your own risk. Behavior, compatibility and features may change in future versions.

---

*Developed by [PierrotAudioTools](https://github.com/PierrotAudioTools)*
