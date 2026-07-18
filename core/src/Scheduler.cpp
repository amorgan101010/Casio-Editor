#include "casioxw/Scheduler.h"

namespace casioxw
{
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

            ScheduledEvent off;
            off.type      = ScheduledEvent::Type::noteOff;
            off.timeMs    = stepStartMs + stepGateMs (seq, stepIndex);
            off.stepIndex = stepIndex;
            off.channel   = ne->channel;
            off.note      = ne->note;
            out.push_back (off);
        }

        return out;
    }
}
