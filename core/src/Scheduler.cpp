#include "casioxw/Scheduler.h"

namespace casioxw
{
    namespace
    {
        // How many steps until the NEXT enabled trig on this same line (any pitch), wrapping the
        // loop -- true monophonic behaviour: a line has exactly one voice, so any subsequent trig
        // supersedes whatever it was still sustaining, not just a same-pitch one. Deliberately
        // takes only `seq` (one track/voice's own steps) rather than anything broader: poly
        // sub-voices are separate Sequence objects scheduled via their own scheduleStep() call
        // (SequencerPanel's synthExtraVoices / PcmTrackControl::extraVoices), so this can never see
        // -- and therefore never cut across -- another voice's steps; polyphony between voices on
        // the same part is preserved by construction, not by a check in here. Always resolves
        // within seq.stepCount steps -- worst case, a line with only ONE trig in the whole
        // pattern still "retriggers" itself at the top of the next lap -- so a note is never left
        // with no eventual cut point. Uses stepCount, NOT seq.steps.size() (always kMaxSteps=64
        // now that step count is configurable) -- steps beyond stepCount are inert and must never
        // be wrapped into this search.
        int stepsUntilNextTrig (const Sequence& seq, int stepIndex)
        {
            const int n = juce::jlimit (1, (int) seq.steps.size(), seq.stepCount);
            for (int delta = 1; delta <= n; ++delta)
                if (seq.steps[(size_t) ((stepIndex + delta) % n)].enabled)
                    return delta;
            return n;   // unreachable (delta == n always matches stepIndex itself); safety net
        }
    }

    std::vector<ScheduledEvent> scheduleStep (const Sequence& seq, int stepIndex,
                                              int prevStepIndex, double stepStartMs)
    {
        std::vector<ScheduledEvent> out;

        // ---- 1. Parameter changes (dedup vs the previous step) -------------------------------
        // Params first so a locked/base value is in place before the note sounds. Only include a
        // param whose effective value actually changed since prevStepIndex; prevStepIndex == -1
        // forces every param out (first step of playback establishes the whole sound).
        for (const auto& pv : effectiveParamValues (seq, stepIndex))
        {
            bool changed = true;
            if (prevStepIndex >= 0)
            {
                const auto prev = effectiveParamValue (seq, prevStepIndex, pv.paramId, pv.instance);
                if (prev.has_value() && *prev == pv.value)
                    changed = false;
            }
            else if (prevStepIndex == kPrevStepBaseline)
            {
                // Device is already at base (post-Sync / post-stop): emit only genuine locks, i.e.
                // params whose effective value on this step differs from their base value.
                for (const auto& lp : seq.lockable)
                    if (lp.paramId == pv.paramId && lp.instance == pv.instance)
                    {
                        if (pv.value == lp.baseValue)
                            changed = false;
                        break;
                    }
            }
            // prevStepIndex == kPrevStepFresh (-1): changed stays true -> full establish.
            if (! changed)
                continue;

            ScheduledEvent e;
            e.type      = ScheduledEvent::Type::paramChange;
            e.timeMs    = stepStartMs;
            e.stepIndex = stepIndex;
            e.paramId   = pv.paramId;
            e.instance  = pv.instance;
            e.value     = pv.value;
            out.push_back (std::move (e));
        }

        // ---- 2 & 3. Note on/off (self-contained pair) ----------------------------------------
        // Each enabled step emits its own note-on at the step start and its own note-off at
        // stepStart + gate. At gate 100% the note-off lands exactly at the next step boundary, so a
        // full-length note reads as legato with no explicit "release the previous note" bookkeeping.
        if (const auto ne = stepEvent (seq, stepIndex))
        {
            ScheduledEvent on;
            on.type      = ScheduledEvent::Type::noteOn;
            on.timeMs    = stepStartMs;
            on.stepIndex = stepIndex;
            on.channel   = ne->channel;
            on.note      = ne->note;
            on.velocity  = ne->velocity;
            out.push_back (on);

            // A gate above 100% sustains over rest steps, but this line is monophonic (one voice):
            // ANY subsequent trig -- same pitch or not -- supersedes whatever note is still
            // sustaining, exactly like a mono synth's own retrigger behaviour. It's also a hard
            // MIDI requirement whenever the trig shares this note's pitch: note-off is pitch-scoped,
            // not voice-scoped, so two overlapping note-ons/offs for the same pitch race on the
            // receiver (hold-count/voice-steal behaviour varies, but the overlap itself is the bug,
            // not any one synth's handling of it -- see bug-332). Capping the note-off at the
            // earlier of the gate and the next trig removes the overlap entirely; at gate<=100% the
            // cap is never tighter than the gate itself (the next trig, if any, is never closer than
            // the step this note-off already lands on), so short/normal notes are unaffected.
            const double cutMs = (double) stepsUntilNextTrig (seq, stepIndex) * stepIntervalMs (seq);

            ScheduledEvent off;
            off.type      = ScheduledEvent::Type::noteOff;
            off.timeMs    = stepStartMs + juce::jmin (stepGateMs (seq, stepIndex), cutMs);
            off.stepIndex = stepIndex;
            off.channel   = ne->channel;
            off.note      = ne->note;
            out.push_back (off);
        }

        return out;
    }
}
