# ADM-OSC Panner

**VST3 / AU plugin for macOS Apple Silicon**

Receive, transmit and record ADM-OSC messages from any compatible DAW. Designed for immersive audio workflows using the [ADM-OSC](https://immersive-audio-live.github.io/ADM-OSC/) open protocol.

![ADM-OSC Panner](Assets/ADM-OSC%20Panner-fullview.png)

---

## Features

- **OSC input** — listen on a configurable port (default: 9000)
- **OSC output** — send to any IP and port (default: 127.0.0.1:9001)
- **ADM-OSC protocol** — Cartesian and Polar coordinate formats
- **Object-based positioning** — per-object XYZ control
- **Visual panner** — real-time Top View and Rear View display
- **DAW automation** — all parameters are recordable and automatable

## Requirements

- macOS 12 or later
- Apple Silicon (ARM) — M1/M2/M3/M4
- Compatible DAW (Pro Tools, Logic Pro, Reaper, Nuendo, etc.)

## Installation

Download the installer for your format from the [Releases](../../releases) page:

| Installer | Format |
|-----------|--------|
| `ADM-OSC Panner … AU.pkg` | Audio Unit (AU) only |
| `ADM-OSC Panner … VST3.pkg` | VST3 only |
| `ADM-OSC Panner … AU-VST3.pkg` | AU + VST3 |

Double-click the `.pkg` file and follow the installer steps. The plugin is installed in the standard system plug-in folders:

- AU → `/Library/Audio/Plug-Ins/Components/`
- VST3 → `/Library/Audio/Plug-Ins/VST3/`

Restart your DAW after installation.

## Usage

1. Insert **ADM-OSC Panner** on any audio track
2. Set **OSC In Port** to the port your source is sending to (default: 9000)
3. Set **OSC Out IP / Port** to your destination (default: 127.0.0.1:9001)
4. Select the **Object** number
5. Move the XYZ position — the Top and Rear views update in real time
6. Automate X, Y, Z in your DAW timeline to record movement

## OSC Address Format

```
/adm/obj/<id>/cartesian  x y z
```

## License

Closed source — all rights reserved. Free to use for personal and professional projects.

---

*Developed by [PierrotAudioTools](https://github.com/PierrotAudioTools)*
