#pragma once

#include <array>
#include <optional>

namespace casioxw
{
    /** One step of a 16-step note sequence. Disabled == a rest; no tie, no p-lock, no per-step
        length — deliberately flat MVP scope (see SEQUENCER_HANDOFF.md), not the full
        Song->Chain->Pattern->Track model from the sequencer roadmap. */
    struct Step
    {
        int note = 60;   // C4
        int velocity = 100;
        bool enabled = false;
    };

    /** A single-track, single-channel, 16-step note sequence. In-memory only — no JSON
        persistence in the MVP. */
    struct Sequence
    {
        std::array<Step, 16> steps {};
        int channel = 1;     // MIDI channel, 1-16
        int tempoBpm = 120;
    };

    /** channel/note/velocity a step sends, or std::nullopt for a disabled step (a rest). */
    struct NoteEvent
    {
        int channel;
        int note;
        int velocity;
    };

    /** Pure function, no MIDI I/O or real time involved — Catch2-testable headless. */
    std::optional<NoteEvent> stepEvent (const Sequence& seq, int stepIndex);

    /** Milliseconds per step at the sequence's tempo. Each step is a 16th note (4 steps/beat,
        the standard step-sequencer convention — 16 steps = one 4/4 bar). */
    double stepIntervalMs (const Sequence& seq);
}
