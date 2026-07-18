# Sequencer Development — Handoff Brief

> Kickoff brief for a **fresh Claude session** picking up the XW-P1 software sequencer, working
> **in parallel** with a separate agent that is extending the tone editor beyond the Solo Synth.
> This is the *start-here + coordination* layer. The full design lives in the roadmap (below) —
> read that first.

## Mission

Build a **software-brain step sequencer** for the Casio XW-P1 inside the existing JUCE editor: the
software is the clock/brain, streaming MIDI to the synth as a sound module. Full feasibility
analysis, tier breakdown, architecture, and phased plan:

**`~/.claude/plans/so-right-now-i-ve-gleaming-backus.md`** ← the roadmap. Read it first.

## Status update (2026-07-18) — NRPN transport landed

- Sequencer p-lock transport is now **NRPN-first** in `app/SequencerPanel::paramMessages()`, with
  **SysEx fallback** for unmapped/non-7-bit values.
- NRPN address mapping is derived from `ParamInfo` metadata (`block` + `addr` + `instance`) for
  Solo Synth blocks:
  - OSC/PWM/Etc -> MSB `0x30..0x35`, LSB `addr`
  - LFO -> MSB `0x36/0x37`, LSB `addr-91`
  - Total Filter -> MSB `0x38`, LSB `addr-72`
- Because NRPN is channel voice, `paramMessages()` now takes channel and call sites pass
  `sequence.channel` (immediate sends + scheduled look-ahead path).
- Build + tests are green after this change (`ctest` 58/58).

**Next session's first checks (hardware-gated):**
1. Verify p-lock behavior/timing on hardware with NRPN path active (especially dense locks).
2. Decide whether to add a CC fast path for any hardware-verified absolute controls.
3. If lockable-param set grows, decide whether to move transport map data from panel-local logic
   into shared param metadata (currently intentionally local to sequencer scope).

## Read-first (cold-start checklist)

1. `.wolf/OPENWOLF.md` — operating protocol; follow it **every turn**.
2. `.wolf/cerebrum.md` — decisions / learnings / do-not-repeat (the sequencer decision + all
   hardware facts are logged here, most recent entries at the bottom of the Decision Log).
3. `.wolf/anatomy.md` — file map with token estimates; **read this before opening any file**.
4. `~/.claude/plans/so-right-now-i-ve-gleaming-backus.md` — the sequencer roadmap.
5. `reference/midi-spec.md` + `reference/PROTOCOL.md` — XW-P1 SysEx/MIDI facts (part/channel model
   §1.1 & §5.2; value encoders; the "no bulk dump" model).
6. `.wolf/buglog.json` — check before fixing any bug/error.
7. `XWP1_1B_EN.pdf` (repo root) — official User's Guide; Step Sequencer chapter **E-49–E-62** is the
   sequencer's musical spec (parts, 16 steps, note/vel/tie, 8 variations, chains).

## Current repo state (what already exists)

- **`casioxw_core`** (GUI-less, headless-testable): `ParamModel` (parses `params/xwp1.json`),
  `SysExCodec` (encode/decode individual params — golden-tested **and** live-verified on hardware),
  `MidiIO` (SysEx send/receive + device enum), `NoteNames`.
- **`app/`** (JUCE standalone): a working **Solo Synth editor** (live-verified against the real
  synth), `ParamControl` (data-driven widgets), `EnvelopeDisplay`.
- **`tests/`**: Catch2 + ctest, green. The Solo Synth tone fully round-trips live against the real
  XW-P1 (ALSA client 40, "CASIO USB-MIDI").
- A **parallel agent** is extending the editor to more tone types / Performance / Mixer. See
  coordination below — you share this repo with them.

## Your first milestone — single-track Solo Synth sequencer

No Performance, no multitimbral, **no reverse-engineering**. Drive the already-implemented Solo
Synth on ONE MIDI channel and prove the whole engine end-to-end. Suggested chunks (commit per chunk
per OpenWolf; the letter prefix keeps them distinct from the editor agent's chunk numbers):

- **S1 — MidiIO channel-voice extension.** Add Note On/Off, CC, Program Change, MIDI clock/start/
  stop send alongside the existing `sendFrame(SysExFrame)`. Additive, non-breaking. Tests: build
  messages, assert bytes. ⚠️ `MidiIO` is a shared file — see coordination.
- **S2 — Sequence document model (core).** `Song → Chain → Pattern → Track → Step`, with p-lock
  lanes + per-step trig conditions/microtiming. JSON serialize/deserialize (reuse `ParamModel`'s
  `juce::JSON` idioms). Catch2 round-trip tests. New files — no collisions.
- **S3 — Pure Scheduler (core).** `(pattern, tick, tempo) → time-ordered MIDI events`. **No real
  time inside** → fully unit-testable (the project's "pure core function" pattern, cerebrum
  addendum 3). New files.
- **S4 — Transport + live playback.** Real-time driver using **look-ahead + timestamped output**
  (`juce::MidiOutput::sendBlockOfMessages` with real timestamps), NOT a bare timer callback — jitter
  matters and this machine runs games in the background. Plays a hand-built pattern on the Solo
  Synth. **Human-run** hardware verify (you can't touch the synth — the owner runs it; give them a
  clear procedure).
- **S5 — SequencerPanel step grid (app/).** 16-step × track grid (note/vel/tie) + transport.
  Build-verified here; interaction verified by owner (no display in this env). ⚠️ Touches `Main.cpp`
  + `app/CMakeLists.txt` — see coordination.
- **S6 — P-locks on the Solo Synth.** Per-step parameter overrides encoded via the **existing**
  `SysExCodec`. The Solo Synth is the ideal p-lock testbed — all its params already round-trip.

After the slice works: advanced trigs (conditions/microtiming/retrigs/polymeter — mostly
Scheduler+model), then multitimbral (more tracks → more channels against a preset Performance),
then song/arrangement mode, then the optional Tier-3 synth-write bridge (reverse-engineering).

## Parallel-work coordination — CRITICAL (two agents, one repo)

**[UPDATED 2026-07-17] The repo is now PUBLIC with GitHub-enforced branch protection on `main`:
a PR is required to merge, the `build-and-test` CI check must pass and be up to date, no
force-push, no deletion — this applies even to the repo owner (`enforce_admins: true`). Working
directly on `main` is no longer possible even if you wanted to; both a local hook and GitHub
itself will reject it.**

**The project's git model is now a fixed 3-tier workflow — use it, don't invent your own:**
`main` (protected, PR-only) ← `feature/*` branches (UNPROTECTED — merge into these directly with
plain `git merge`, no PR, as soon as a chunk's own tests are green) ← `sub/*` action-item branches
for individual chunks. Concretely for you: create one feature branch off current `main` (e.g.
`feature/sequencer`) as your working line — either directly or in a separate `git worktree` for
isolation from the tone-editor agent. Land S1–S6 as sub-branches merged straight into
`feature/sequencer` (no PR per chunk — that was explicitly rejected in favor of fast iteration).
Only when the whole first milestone works do you open ONE PR from `feature/sequencer` into `main`.

The working tree at `main` is clean as of this handoff (commit `2275b21`) — Chunk 7c and its
follow-up visual-review fixes (Chunk 7d) are fully merged, nothing in flight. Branch from there.

One more state change since this doc was first drafted: `reference/lua/` (franky's extracted
CTRLR panel Lua) is now gitignored and scrubbed from git history — his own copyrighted work with
no clear license, not safe to publish now that the repo is public. The files are still present on
disk locally (only `tools/gen_xwp1.py`/`tools/gen_golden.lua` need them, to regenerate the already-
committed `params/xwp1.json`/`tests/golden/`). You shouldn't need to touch them for sequencer work,
but if you go looking for `reference/lua/*.lua` in `git log`/`git show`, it won't be there — that's
expected, not a bug. See `.wolf/cerebrum.md` Decision Log (2026-07-17, "Repo made PUBLIC...") for
the full rationale.

**Files you OWN (create/edit freely):**
- New: `core/include/casioxw/Sequence*.h` + `Scheduler*.h`, `core/src/Sequence*.cpp` +
  `Scheduler*.cpp`; `app/SequencerPanel.{h,cpp}`; `tests/Sequence*Tests.cpp`,
  `tests/SchedulerTests.cpp`.

**HOT shared files (minimal, localized edits; expect merge conflicts):**
- `app/Main.cpp` — you add your panel/tab; the editor agent adds theirs. Isolated lines only.
- `app/CMakeLists.txt`, `core/CMakeLists.txt`, `tests/CMakeLists.txt` — both append sources/targets;
  append at the end so merges are trivial.
- `core/include/casioxw/MidiIO.{h}` + `core/src/MidiIO.cpp` — you extend (channel-voice); the editor
  agent probably doesn't touch it, but check before large edits.

**Do NOT modify (editor agent's domain / read-only for you):**
- `params/xwp1.json`, `params/xwp1.schema.json`, `tools/gen_xwp1.py` — the param map is theirs. You
  **read** `xwp1.json` via `ParamModel` (for p-lock param metadata). Need new metadata? Flag it to
  the owner, don't edit.
- Existing tone UI: `SoloSynthPanel`, `ParamControl`, `EnvelopeDisplay` and their tests.

**Shared `.wolf/` memory:** both agents read/write cerebrum, memory, anatomy, buglog. **Append,
don't rewrite;** keep entries scoped + dated so you don't clobber the other agent's notes.

## Hard rules (OpenWolf + project conventions)

- **Follow `.wolf/OPENWOLF.md` every turn**: anatomy before reading, cerebrum before coding, log
  bugs to `buglog.json`, append to `memory.md`, update `cerebrum.md` on learnings/corrections.
- **JSON is the single source of truth** — never hardcode param data; read via `ParamModel`.
- **Pure-core-function pattern** — testable logic (model, scheduler, decisions) goes in
  `casioxw_core` with **no JUCE GUI type in the signature**; GUI stays in `app/`. This is what makes
  the engine headless-testable in this display-less environment.
- **Hardware gates are human-run.** You cannot touch the synth. Build-verify + unit-test everything
  possible, then hand the owner a clear manual test procedure for anything needing the real XW-P1.
- **Throttle builds** (games run concurrently): `nice -n 19 ionice -c 3 cmake --build build -- -j4`.
- **Commit per chunk** on completion + verify; `Co-Authored-By` trailer required.
- **No whole-tone/bulk dump** for individual params — the codec is param-by-param request/response.

## Hardware facts already established

- XW-P1 receives **multitimbrally only in Performance mode with a configured Performance** — but
  presets already provide this, so develop against those; the single-Solo-Synth first slice needs
  none of it.
- Solo Synth edits round-trip **live** against the real synth (proven, chunks 6b/7b).
- The synth's front panel does **not** live-update on incoming SysEx edits (re-enter the page to
  confirm) — irrelevant to note playback, but don't let it confuse a hardware check.
