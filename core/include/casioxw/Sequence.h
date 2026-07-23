#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <optional>
#include <vector>

namespace casioxw
{
    /** Hard capacity of a Sequence's step array -- the largest a configurable pattern can ever be.
        `Sequence::stepCount` (1..kMaxSteps) is the actual, currently-active pattern length; slots
        from stepCount..kMaxSteps-1 stay allocated but inert (not played, not rotated, not
        randomized) -- same "inert rather than deleted" precedent as an unselected engine's p-locks
        -- so shrinking stepCount never destroys previously-authored steps beyond the new length. */
    constexpr int kMaxSteps = 64;

    /** Longest a step's gate can run for a pattern of `stepCount` steps: the WHOLE pattern's worth
        (stepCount*100%), clamped to kMaxSteps -- a note locked to full gate on every step always
        sustains across the entire sequence, at any configured length. Replaces the old flat 1600%
        (16 steps' worth) now that step count is configurable; every existing call site already has
        a `Sequence` (or its stepCount) in scope, so this takes stepCount explicitly rather than
        assuming 16. */
    int maxGatePercent (int stepCount);

    /** A per-step parameter-lock: "on this step, force <paramId>/<instance> to <value>", overriding
        the track's base value for that parameter. Encoded on playback via the existing SysExCodec,
        exactly like a live tone edit — a p-lock IS a tone edit scheduled to a step. */
    struct ParamLock
    {
        juce::String paramId;
        int instance = 1;   // 1-based, matches SysExCodec::encode()'s instance
        int value = 0;
    };

    /** One step of a note sequence (up to kMaxSteps long). Disabled == a rest. `locks` holds any
        per-step parameter overrides (empty on a plain note step). No tie / microtiming / trig
        conditions — flat MVP scope (see SEQUENCER_HANDOFF.md), not the full roadmap's Track/Pattern
        model. */
    struct Step
    {
        int note = 60;   // C4
        int velocity = 100;
        bool enabled = false;
        int gatePercent = 90;   // note length as % of the step interval (1..maxGatePercent(stepCount));
                                 // <100 = gap before next, >100 = sustains into following step(s)
        std::vector<ParamLock> locks;
    };

    /** A parameter the sequence can p-lock, together with its track-wide *base* value — the value
        applied on every step that does NOT lock this parameter. (The XW-P1's own params carry no
        usable factory default for these — Filter Cutoff/Resonance default to JSON null — so "away
        from default" means "away from this user-set base".) */
    struct LockableParam
    {
        juce::String paramId;
        int instance = 1;
        int baseValue = 0;
        int minValue = 0;     // parameter range, seeded from ParamInfo — bounds randomized locks
        int maxValue = 127;
    };

    /** A single-track, single-channel note sequence with per-step parameter locks. `steps` is
        always allocated at kMaxSteps capacity; `stepCount` (1..kMaxSteps) is the actual pattern
        length everything else (playback, JSON, shiftSteps, randomize) operates over — slots at or
        beyond it are simply inert, never played/rotated/randomized (see kMaxSteps's doc comment).
        stepCount is set the SAME on every Sequence-backed track/voice in a SequencerPanel (one
        global control, not a per-track setting) but lives here, per-Sequence, so every pure
        function below that already takes a Sequence has everything it needs without a second
        parameter threaded through. */
    struct Sequence
    {
        std::array<Step, kMaxSteps> steps {};
        int stepCount = 16;   // 1..kMaxSteps
        int channel = 1;     // MIDI channel, 1-16
        int tempoBpm = 120;
        int stepsPerBeat = 4;   // rate/time-scale: 4 = 16th notes, 2 = 8ths, 8 = 32nds, 3 = 8th triplets

        /** The p-lockable parameters this sequence knows about, with their base values. The
            SequencerPanel seeds this (Filter Cutoff + Resonance to start); it is the single list
            that both the UI's param-control row and the playback engine iterate. */
        std::vector<LockableParam> lockable;

        /** Opaque app-defined tag naming which tone engine (e.g. "soloSynth"/"hexLayer"/
            "drawbarOrgan") this sequence's `lockable` set targets -- core does not interpret it,
            just carries it through JSON round-trips. SequencerPanel uses it to pick which
            per-engine lockable table to rebuild on load. Empty on very old files, which predate
            multi-engine support and should be treated as "soloSynth" by the reader. */
        juce::String engineTag;
    };

    /** channel/note/velocity a step sends, or std::nullopt for a disabled step (a rest). */
    struct NoteEvent
    {
        int channel;
        int note;
        int velocity;
    };

    /** A resolved parameter value to apply at a step: paramId/instance + the effective value. */
    struct ParamValue
    {
        juce::String paramId;
        int instance = 1;
        int value = 0;
    };

    // ---- Pure functions (no MIDI I/O, no real time — Catch2-testable headless) ----------------

    /** Note the step sends, or nullopt for a disabled step. */
    std::optional<NoteEvent> stepEvent (const Sequence& seq, int stepIndex);

    /** Milliseconds per step at the sequence's tempo and rate. One step = one (1 / (stepsPerBeat*4))
        note; stepsPerBeat=4 -> 16th notes (16 steps = one 4/4 bar), 2 -> 8ths, 8 -> 32nds,
        3 -> 8th-note triplets. */
    double stepIntervalMs (const Sequence& seq);

    /** How long a step's note sounds, in ms: stepIntervalMs * gatePercent/100 (clamped
        1..maxGatePercent(seq.stepCount)%). The playback engine note-offs at this point; at 100%
        that's exactly the next step boundary (legato), and above 100% it lands one or more steps
        later (the note sustains through/over any intervening steps' own note-ons). Reads the raw
        value through snapGatePercent(), so a stored value that isn't itself a legal gate (e.g.
        hand-edited JSON, or one left over from a longer stepCount that has since been shrunk)
        still times out at the value it will DISPLAY as, not some in-between fraction of a step. */
    double stepGateMs (const Sequence& seq, int stepIndex);

    /** Snap a raw gate value to the nearest legal one for a pattern of `stepCount` steps: any
        integer 1..100 (percent of one step, freely tunable), or an exact multiple of 100 above
        that, up to maxGatePercent(stepCount) -- once a gate sustains past its own step it can only
        do so in whole-step increments (2x, 3x, ... up to stepCount x), never a fraction of a step.
        Values between two legal multiples round UP to the next one (dragging just past 100% lands
        on 2x, not back on 100%); already-legal values (including every 1..100) pass through
        unchanged, so this is idempotent. */
    int snapGatePercent (int raw, int stepCount);

    /** The lock a step holds for a given parameter, or nullptr if that parameter is unlocked on
        that step (so it inherits the base value). */
    const ParamLock* findStepLock (const Step& step, const juce::String& paramId, int instance);

    /** Effective value of a lockable parameter at a step: the step's lock value if present, else
        the parameter's base value. nullopt if paramId/instance isn't a known lockable parameter. */
    std::optional<int> effectiveParamValue (const Sequence& seq, int stepIndex,
                                            const juce::String& paramId, int instance);

    /** Every lockable parameter's effective value at a step, in `seq.lockable` order. This is what
        playback resolves per step; the caller diffs it against what's currently applied and only
        re-sends the ones that changed (the parameter analogue of the note-off dedup). */
    std::vector<ParamValue> effectiveParamValues (const Sequence& seq, int stepIndex);

    // ---- Small mutators (also pure of I/O; kept in core so they're testable) ------------------

    /** Set the base (unlocked) value of a known lockable parameter. No-op if not lockable. */
    void setBaseValue (Sequence& seq, const juce::String& paramId, int instance, int value);

    /** Add or update a step's lock for a parameter. */
    void setStepLock (Sequence& seq, int stepIndex, const juce::String& paramId, int instance, int value);

    /** Remove a step's lock for a parameter, if present (reverts that param to base on that step). */
    void clearStepLock (Sequence& seq, int stepIndex, const juce::String& paramId, int instance);

    /** Remove every lock on a step. */
    void clearStepLocks (Sequence& seq, int stepIndex);

    /** Serialize a sequence to a JSON string: stepCount, every step slot up to kMaxSteps (note/
        velocity/enabled/gatePercent/locks), channel/tempo/rate, and each lockable param's base
        value. Round-trips with sequenceFromJson(). (min/max are metadata re-seeded from the param
        model on load, so they aren't stored.) */
    juce::String sequenceToJson (const Sequence& seq);

    /** Parse a sequence from a sequenceToJson() string. std::nullopt if the text isn't a valid
        sequence object; missing fields fall back to defaults (stepCount defaults to 16 -- every
        file predating configurable step count is exactly a 16-step file) and at most kMaxSteps
        step slots are read, so a slightly-off or future-versioned file degrades rather than
        crashes. */
    std::optional<Sequence> sequenceFromJson (const juce::String& text);

    /** Rotate the active `seq.stepCount` steps (note/gate/velocity/enable/locks move together) by
        `delta` positions, wrapping within that window: delta>0 shifts later/right (step i ->
        i+delta), delta<0 earlier/left. Steps at/beyond stepCount are left untouched. Lets a pattern
        be re-anchored to a different starting step without re-authoring it. */
    void shiftSteps (Sequence& seq, int delta);

    /** Tuning for randomize() — trig density, note range/scale/root, lock density, and which
        lockable params may receive locks at all. Defaults reproduce a moderate musical starting
        point (close to the original hardcoded behaviour, but with a much lower lock density now
        that the lockable set is large). Edited from SequencerPanel's Rnd options call-out. */
    struct RandomizeOptions
    {
        enum class Scale { minorPentatonic, majorPentatonic, naturalMinor, major, chromatic };

        float trigDensity = 0.6f;     // chance a step's trig is enabled
        float lockDensity = 0.1f;     // chance per (eligible param, step) of a p-lock
        int   noteMin  = 48;          // C3 — notes drawn from [noteMin, noteMax], snapped to scale
        int   noteMax  = 72;          // C5
        int   rootNote = 0;           // pitch class 0=C .. 11=B the scale degrees offset from
        Scale scale = Scale::minorPentatonic;
        int   gateMin = 30,  gateMax = 100;       // percent
        int   velocityMin = 70, velocityMax = 127;

        /** Indices into Sequence::lockable eligible to receive random locks. Empty == every
            lockable is eligible. SequencerPanel passes only the continuous (Slider-kind) params
            by default so filter type / LFO wave / sync don't jump per step. */
        std::vector<int> lockableIndices;
    };

    /** Randomize the musical content of a sequence in place, tuned by `options`: per-step trig,
        note (snapped to options.scale over [noteMin, noteMax]), velocity, gate, and p-locks
        (each eligible lockable param locks with probability lockDensity per step, to a random
        in-range value). Leaves channel / tempo / rate / the lockable set + base values untouched.
        `rng` makes this deterministic for a given (seed, options) — the whole reason it's a pure
        core function, so the UI's Randomize button is exercised headlessly
        (tests/SequenceTests.cpp). */
    void randomize (Sequence& seq, juce::Random& rng, const RandomizeOptions& options);

    /** Default-options overload (kept for existing callers/tests). */
    void randomize (Sequence& seq, juce::Random& rng);
}
