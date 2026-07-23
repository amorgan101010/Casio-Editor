#include "casioxw/Scheduler.h"

namespace casioxw
{
    namespace
    {
        // How many steps until THIS step's own note (same pitch) next triggers again, wrapping
        // the loop. Always resolves within seq.steps.size() steps -- worst case, a pitch with no
        // other occurrence still retriggers itself at the top of the next lap -- so a note is
        // never left with no eventual cut point.
        int stepsUntilNextSameNoteTrig (const Sequence& seq, int stepIndex)
        {
            const int note = seq.steps[(size_t) stepIndex].note;
            const int n = (int) seq.steps.size();
            for (int delta = 1; delta <= n; ++delta)
            {
                const auto& s = seq.steps[(size_t) ((stepIndex + delta) % n)];
                if (s.enabled && s.note == note)
                    return delta;
            }
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

            // A gate above 100% sustains over rest/other-pitch steps, but MIDI note-off is
            // pitch-scoped, not voice-scoped: if this pitch retriggers (another enabled step with
            // the SAME note) before this gate would naturally end, that retrigger's own note-on
            // must be what governs from then on, or the two overlapping note-ons/offs for the same
            // pitch race on the receiver (hold-count/voice-steal behaviour varies, but the overlap
            // itself is the bug, not any one synth's handling of it -- see bug-332). Capping the
            // note-off at the earlier of the gate and the next same-pitch trig removes the overlap
            // entirely; at gate<=100% the cap is never tighter than the gate itself (the next
            // same-pitch trig, if any, is never closer than the step this note-off already lands
            // on), so short/normal notes are unaffected.
            const double cutMs = (double) stepsUntilNextSameNoteTrig (seq, stepIndex) * stepIntervalMs (seq);

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
