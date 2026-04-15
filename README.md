# ADM-OSC Panner

**VST3 / AU plugin for macOS Apple Silicon**

Receive, transmit and record ADM-OSC messages from any compatible DAW. Designed for immersive audio workflows using the [ADM-OSC](https://immersive-audio-live.github.io/ADM-OSC/) open protocol.

<img src="Assets/ADM-OSC%20Panner-fullview.png" width="400" alt="ADM-OSC Panner" />

---

## Features

- **OSC input** — Create an ADM-OSC Server (default: 9000)
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

Download the installer from the [Releases](../../releases) page:

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
6. Automate X, Y, Z in your DAW timeline to record and playback movement
7. Disable / Enable OSC input or output by clicking on "OSC In" or "OSC Out" in the OSC tab
8. When multiple instances share the same OSC input port, they automatically form a **HUB**: only one instance listens on the port and redistributes the incoming messages to all others.
9. When used in Polar mode, the plugin automatically converts the incoming polar coordinates to Cartesian before processing.

## OSC Address Format

```
**CARTESIAN** : /adm/obj/<id>/xyz  x y z
**POLAR** : /adm/obj/<id>/aed a e d
```

## License

Closed source — all rights reserved. 

---

*Developed by [PierrotAudioTools](https://github.com/PierrotAudioTools)*
