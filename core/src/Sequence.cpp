#include "casioxw/Sequence.h"

namespace casioxw
{
    std::optional<NoteEvent> stepEvent (const Sequence& seq, int stepIndex)
    {
        const auto& step = seq.steps[(size_t) stepIndex];
        if (! step.enabled)
            return std::nullopt;
        return NoteEvent { seq.channel, step.note, step.velocity };
    }

    double stepIntervalMs (const Sequence& seq)
    {
        const double msPerBeat = 60000.0 / (double) seq.tempoBpm;
        return msPerBeat / 4.0;
    }
}
