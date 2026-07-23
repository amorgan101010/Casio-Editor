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
             dedup, so an unchanging base isn't re-sent every step. `prevStepIndex >= 0` diffs against
             that real step; two negative sentinels cover the first step fed at play-start (see
             kPrevStepFresh / kPrevStepBaseline below).
          2. `noteOn` at `stepStartMs` — only if the step is enabled.
          3. `noteOff` at `stepStartMs + min(stepGateMs(seq, stepIndex), cut)` — only if the step is
             enabled, where `cut` is the time until the NEXT enabled trig on this same line (`seq`),
             any pitch, wrapping the loop if needed. `seq` is one voice's own steps only (a poly
             sub-voice is a separate Sequence, scheduled via its own scheduleStep() call), so this
             never sees — and can never cut across — another voice's steps; polyphony between
             voices is preserved by construction. At gate<=100% `cut` is never the tighter bound (the
             next trig, if any, is never closer than this note-off's own step boundary), so this only
             matters above 100%: a long gate sustains over rest steps but gets cut at the next trig,
             same pitch or not — true monophonic retrigger behaviour, and also a hard MIDI
             requirement whenever the trig shares this note's pitch (note-off is pitch-scoped, not
             voice-scoped, so two overlapping note-ons/offs for one pitch is an ambiguity on ANY
             receiver, not a per-synth quirk to special-case around; bug-332).

        A disabled (rest) step still contributes its changed param events (a p-lock on a rest is
        valid), just no note. */

    /** First-step `prevStepIndex` sentinels (there is no real previous step at play-start):
          * kPrevStepFresh (-1): force EVERY lockable param out — a full establish that assumes the
            device state is unknown.
          * kPrevStepBaseline (-2): the device is ALREADY at every param's base value (true after a
            Sync, or after stop()'s reset-to-base), so emit ONLY the params this step locks away from
            base, never the redundant baseline. This is the normal play-start: base mirrors the
            device, so re-dumping the whole baseline just floods the synth. */
    constexpr int kPrevStepFresh    = -1;
    constexpr int kPrevStepBaseline = -2;

    std::vector<ScheduledEvent> scheduleStep (const Sequence& seq, int stepIndex,
                                              int prevStepIndex, double stepStartMs);
}
