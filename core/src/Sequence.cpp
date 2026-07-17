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

    const ParamLock* findStepLock (const Step& step, const juce::String& paramId, int instance)
    {
        for (const auto& lock : step.locks)
            if (lock.paramId == paramId && lock.instance == instance)
                return &lock;
        return nullptr;
    }

    std::optional<int> effectiveParamValue (const Sequence& seq, int stepIndex,
                                            const juce::String& paramId, int instance)
    {
        // Only a known lockable parameter has an effective value (it needs a base to fall back to).
        const LockableParam* base = nullptr;
        for (const auto& lp : seq.lockable)
            if (lp.paramId == paramId && lp.instance == instance)
            {
                base = &lp;
                break;
            }
        if (base == nullptr)
            return std::nullopt;

        if (const auto* lock = findStepLock (seq.steps[(size_t) stepIndex], paramId, instance))
            return lock->value;
        return base->baseValue;
    }

    std::vector<ParamValue> effectiveParamValues (const Sequence& seq, int stepIndex)
    {
        std::vector<ParamValue> out;
        out.reserve (seq.lockable.size());
        for (const auto& lp : seq.lockable)
        {
            const auto v = effectiveParamValue (seq, stepIndex, lp.paramId, lp.instance);
            out.push_back (ParamValue { lp.paramId, lp.instance, v.value_or (lp.baseValue) });
        }
        return out;
    }

    void setBaseValue (Sequence& seq, const juce::String& paramId, int instance, int value)
    {
        for (auto& lp : seq.lockable)
            if (lp.paramId == paramId && lp.instance == instance)
            {
                lp.baseValue = value;
                return;
            }
    }

    void setStepLock (Sequence& seq, int stepIndex, const juce::String& paramId, int instance, int value)
    {
        auto& step = seq.steps[(size_t) stepIndex];
        for (auto& lock : step.locks)
            if (lock.paramId == paramId && lock.instance == instance)
            {
                lock.value = value;
                return;
            }
        step.locks.push_back (ParamLock { paramId, instance, value });
    }

    void clearStepLock (Sequence& seq, int stepIndex, const juce::String& paramId, int instance)
    {
        auto& locks = seq.steps[(size_t) stepIndex].locks;
        for (auto it = locks.begin(); it != locks.end(); ++it)
            if (it->paramId == paramId && it->instance == instance)
            {
                locks.erase (it);
                return;
            }
    }

    void clearStepLocks (Sequence& seq, int stepIndex)
    {
        seq.steps[(size_t) stepIndex].locks.clear();
    }
}
