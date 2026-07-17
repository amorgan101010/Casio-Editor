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
        const int spb = seq.stepsPerBeat > 0 ? seq.stepsPerBeat : 4;
        return msPerBeat / (double) spb;
    }

    double stepGateMs (const Sequence& seq, int stepIndex)
    {
        const int pct = juce::jlimit (1, 100, seq.steps[(size_t) stepIndex].gatePercent);
        return stepIntervalMs (seq) * (double) pct / 100.0;
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

    void shiftSteps (Sequence& seq, int delta)
    {
        constexpr int n = 16;
        const int d = ((delta % n) + n) % n;   // normalize to 0..15 (a left shift comes in negative)
        if (d == 0)
            return;

        std::array<Step, n> rotated {};
        for (int i = 0; i < n; ++i)
            rotated[(size_t) ((i + d) % n)] = seq.steps[(size_t) i];
        seq.steps = std::move (rotated);
    }

    void randomize (Sequence& seq, juce::Random& rng)
    {
        static const int pentatonic[] = { 0, 3, 5, 7, 10 };   // C minor pentatonic degrees
        constexpr int root = 48;                              // C3

        for (auto& step : seq.steps)
        {
            step.enabled = rng.nextFloat() < 0.6f;

            const int octave = rng.nextInt (2);              // 0..1
            const int degree = pentatonic[rng.nextInt (5)];
            step.note = root + octave * 12 + degree;

            step.velocity = 70 + rng.nextInt (58);           // 70..127
            step.gatePercent = 30 + rng.nextInt (71);        // 30..100 (avoid all-stab sequences)

            step.locks.clear();
            for (const auto& lp : seq.lockable)
                if (rng.nextFloat() < 0.3f)
                {
                    const int span = lp.maxValue - lp.minValue + 1;
                    const int value = lp.minValue + rng.nextInt (juce::jmax (1, span));
                    step.locks.push_back (ParamLock { lp.paramId, lp.instance, value });
                }
        }
    }
}
