#pragma once

#include "casioxw/Sequence.h"

#include <juce_core/juce_core.h>

#include <vector>

namespace casioxw
{
    /** One timed thing the sequencer wants to emit: a note-on, a note-off, or a parameter change
        (a p-lock or a base value). `timeMs` is absolute, measured from the transport's step-0 start
        (step 0 begins at 0.0); the real-time transport adds its own wall-clock reference before
        handing these to the MIDI output thread. This is the abstract event the pure scheduler
        produces — how a `paramChange` becomes wire bytes (SysEx today; NRPN/CC next) is the
        transport layer's concern, deliberately NOT baked in here. */
    struct ScheduledEvent
    {
        enum class Type { paramChange, noteOn, noteOff };

        double timeMs = 0.0;      // absolute, relative to transport start (step 0 start == 0)
        Type   type   = Type::noteOn;
        int    stepIndex = 0;     // which step produced this event (for a UI playhead later)

        // Note payload (type == noteOn / noteOff).
        int channel  = 1;
        int note     = 0;
        int velocity = 0;

        // Param payload (type == paramChange).
        juce::String paramId;
        int instance = 1;
        int value    = 0;
    };

    /** Every event a single step contributes to the timeline, with absolute times, given the ms at
        which the step starts (`stepStartMs`). This is the project's pure "(pattern, step, tempo) ->
        time-ordered MIDI events" core (SEQUENCER_HANDOFF.md S3): no MIDI I/O, no real time, so it is
        exhaustively Catch2-testable. The real-time transport (S4) calls it per step inside a
        look-ahead loop and feeds the results to a timestamped output.

        Contents & ordering of the returned vector:
          1. `paramChange` events (at `stepStartMs`), so a locked/base value lands on the synth
             BEFORE the note sounds. Emitted with a dedup: a param is included only when this step's
             effective value differs from `prevStepIndex`'s — the parameter analogue of the note-off
             dedup, so an unchanging base isn't re-sent every step. `prevStepIndex == -1` forces every
             lockable param out (used for the first step fed at play-start, to establish the sound).
          2. `noteOn` at `stepStartMs` — only if the step is enabled.
          3. `noteOff` at `stepStartMs + stepGateMs(seq, stepIndex)` — only if the step is enabled.

        A disabled (rest) step still contributes its changed param events (a p-lock on a rest is
        valid), just no note. */
    std::vector<ScheduledEvent> scheduleStep (const Sequence& seq, int stepIndex,
                                              int prevStepIndex, double stepStartMs);
}
