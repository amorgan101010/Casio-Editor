# Casio XW-P1 Editor

Linux-native JUCE editor for the Casio XW-P1 synth, with:

- data-driven tone editors for Solo Synth, PCM (Melody) Engine, Drawbar Organ, and Hex Layer (SysEx encode/decode, sync, grouped UI),
- a 16-step sequencer with per-step parameter locks, drum tracks, and independent PCM melodic tracks,
- a headless core (`casioxw_core`) designed for deterministic unit tests.

## Current status

- App version: `0.19.0` (source: `app/AppVersion.h`)
- Core version: `0.1.0` (source: `core/include/casioxw/CoreVersion.h`)
- CI: build + test on every push/PR to `main`
- Test suite: Catch2 via CTest (core codec/model/MIDI/sequencer coverage), 92/92 test cases passing

## Versioning rule

- Feature branches merged into `main` must bump `app/AppVersion.h` using semantic versioning
  (`MAJOR.MINOR.PATCH` with optional prerelease/build metadata).
- CI enforces this on PRs where the source branch name starts with `feature/`.

## Features

### Solo Synth editor

- Parameter metadata loaded from `params/xwp1.json` (single source of truth)
- Data-driven controls (`slider`, `toggle`, `combo`) with enum support
- Grouped parameter layout + envelope visualization
- Connect/sync flow against real MIDI devices

### PCM (Melody) Engine editor

- Category `0x05` tone editor (Attack/Release/Cutoff, Vibrato, Octave Shift, Touch Sense)
- Reuses the shared MIDI connection opened from the Solo Synth tab
- Hardware-verified addressing, including the corrected Volume register (Tone `0x03`/`0x08`,
  not the Melody-category address the manual's own table lists)

### Drawbar Organ editor

- Category `0x07` tone editor: 9-drawbar bank (harmonic display order, hardware-confirmed SysEx
  instance mapping), Percussion, Key-on/off Click, Rotary Type, Vibrato Rate/Depth
- Drawbar fader writes go out via the synth's live NRPN performance-fader path (the SysEx
  edit-buffer write persists but doesn't reach the running voice in real time); every other organ
  control writes and reads live via SysEx
- Inverted vertical fader (down = loudest, matching the hardware drawbar convention)

### Hex Layer editor

- Category `0x08` tone editor (XW-P1 only): a Block/Layer navigator (Layer 1-6, plus a
  hex-layer-wide "Global" block) over 31 params -- per-layer Pan/Pitch/Amp/Filter/Effects/Range
  offsets, plus Detune Number and a shared Pitch/Amp LFO pair
- Pitch Lock was shipped and then removed after hardware testing found no effect on pitch
  bend/transpose and no corresponding setting in the synth's own menu
- NOT YET hardware-verified -- no Lua SysEx source exists for this domain (franky's panel drives
  Hex Layer live via NRPN, not SysEx), so budget a read/write/audible check before trusting it the
  way Solo Synth is trusted; see `params/xwp1.json`'s `hexLayer` section note for the full caveat

### Sequencer

- 16-step note sequencer with gate, velocity, rate, shift, and randomize
- Per-step parameter locks (Elektron-style workflow) for Solo Synth params, edited through a
  pageable hardware-LCD-style parameter window (`ParamPageDisplay`)
- Drum Tracks: 5 independent channel-voice lanes (Performance parts 8-12) with their own step
  triggers, mute, channel, and p-lockable velocity
- PCM Tracks: 4 independent melodic lanes (Bass/Solo 1/Solo 2/Chords, Performance parts 13-16),
  each with its own note/gate/velocity p-lock page
- Save/load sequence JSON (`.xwseq`) as a solo sequence, a drum sequence, or a combined sequence set
- Look-ahead scheduling with timestamped MIDI output and a deep start-of-playback prime to avoid
  tempo lurch
- Parameter transport: SysEx (`SysExCodec`), per-vt encoded — the earlier NRPN-shortcut transport
  was found to corrupt signed/scaled values and was reverted

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

1. Pick MIDI input/output ports on the Solo Synth tab,
2. Connect,
3. Use the Solo Synth / PCM Engine / Organ / Hex Layer tabs for tone editing/sync (PCM Engine,
   Organ, and Hex Layer reuse the Solo Synth tab's connection),
4. Use the Sequencer tab for playback and p-lock sequencing across the synth, drum, and PCM tracks.

## Notes

- This project targets the Casio XW-P1 workflow and data model specifically.
- Parameter mapping is generated data; avoid hardcoding parameter definitions in UI code.
- Not affiliated with Casio.
