# Casio XW-P1 Editor

Linux-native JUCE editor for the Casio XW-P1 synth, with:

- a data-driven Solo Synth editor (SysEx encode/decode, sync, grouped UI),
- a 16-step sequencer with per-step parameter locks,
- a headless core (`casioxw_core`) designed for deterministic unit tests.

## Current status

- App version: `0.6.0-sequencer`
- CI: build + test on every push/PR to `main`
- Test suite: Catch2 via CTest (core codec/model/MIDI/sequencer coverage)

## Features

### Solo Synth editor

- Parameter metadata loaded from `params/xwp1.json` (single source of truth)
- Data-driven controls (`slider`, `toggle`, `combo`) with enum support
- Grouped parameter layout + envelope visualization
- Connect/sync flow against real MIDI devices

### Sequencer

- 16-step note sequencer with gate, velocity, rate, shift, and randomize
- Per-step parameter locks (Elektron-style workflow)
- Save/load sequence JSON (`.xwseq`)
- Look-ahead scheduling with timestamped MIDI output
- Parameter transport: NRPN-first where mapped, SysEx fallback

### Core library (`casioxw_core`)

- `ParamModel`: parses and indexes JSON parameter schema
- `SysExCodec`: XW-P1 parameter frame encode/decode
- `MidiIO`: MIDI device management, input queueing, immediate + scheduled output
- `Sequence` + `Scheduler`: pure sequencer model/scheduling logic

## Repository layout

```text
app/                 JUCE GUI app (editor + sequencer)
core/                GUI-less model/codec/midi/sequencer logic
tests/               Catch2 tests (run through ctest)
params/              Parameter schema and generated map
reference/           Protocol + MIDI implementation notes
tools/               Generators and utility tools
```

## Build

### Prerequisites (Arch Linux)

```bash
sudo pacman -Syu --needed \
  base-devel cmake ninja \
  alsa-lib freetype2 fontconfig \
  libx11 libxcomposite libxcursor libxext \
  libxinerama libxrandr libxrender \
  webkit2gtk gtk3 \
  glu mesa
```

### Prerequisites (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  cmake ninja-build \
  libasound2-dev libfreetype-dev libfontconfig1-dev \
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
  libxinerama-dev libxrandr-dev libxrender-dev \
  libwebkit2gtk-4.1-dev libgtk-3-dev \
  libglu1-mesa-dev mesa-common-dev
```

### Configure, build, test

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## Run

```bash
./build/app/CasioXWEditor_artefacts/Debug/Casio\ XW-P1\ Editor
```

In the app:

1. Pick MIDI input/output ports,
2. Connect,
3. Use the Solo Synth tab for tone editing/sync,
4. Use the Sequencer tab for playback and p-lock sequencing.

## Notes

- This project targets the Casio XW-P1 workflow and data model specifically.
- Parameter mapping is generated data; avoid hardcoding parameter definitions in UI code.
- Not affiliated with Casio.
