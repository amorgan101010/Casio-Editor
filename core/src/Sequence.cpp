#include "casioxw/Sequence.h"

#include <algorithm>

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

    juce::String sequenceToJson (const Sequence& seq)
    {
        juce::DynamicObject::Ptr root = new juce::DynamicObject();
        root->setProperty ("format", "casioxw-sequence");
        root->setProperty ("version", 1);
        root->setProperty ("channel", seq.channel);
        root->setProperty ("tempoBpm", seq.tempoBpm);
        root->setProperty ("stepsPerBeat", seq.stepsPerBeat);
        root->setProperty ("engineTag", seq.engineTag);

        juce::Array<juce::var> lockableArr;
        for (const auto& lp : seq.lockable)
        {
            juce::DynamicObject::Ptr o = new juce::DynamicObject();
            o->setProperty ("paramId", lp.paramId);
            o->setProperty ("instance", lp.instance);
            o->setProperty ("baseValue", lp.baseValue);
            lockableArr.add (juce::var (o.get()));
        }
        root->setProperty ("lockable", lockableArr);

        juce::Array<juce::var> stepsArr;
        for (const auto& s : seq.steps)
        {
            juce::DynamicObject::Ptr o = new juce::DynamicObject();
            o->setProperty ("note", s.note);
            o->setProperty ("velocity", s.velocity);
            o->setProperty ("enabled", s.enabled);
            o->setProperty ("gatePercent", s.gatePercent);

            juce::Array<juce::var> locksArr;
            for (const auto& lk : s.locks)
            {
                juce::DynamicObject::Ptr lo = new juce::DynamicObject();
                lo->setProperty ("paramId", lk.paramId);
                lo->setProperty ("instance", lk.instance);
                lo->setProperty ("value", lk.value);
                locksArr.add (juce::var (lo.get()));
            }
            o->setProperty ("locks", locksArr);
            stepsArr.add (juce::var (o.get()));
        }
        root->setProperty ("steps", stepsArr);

        return juce::JSON::toString (juce::var (root.get()));
    }

    std::optional<Sequence> sequenceFromJson (const juce::String& text)
    {
        const auto parsed = juce::JSON::parse (text);
        if (parsed.getDynamicObject() == nullptr)   // reject non-object JSON (arrays, scalars, garbage)
            return std::nullopt;

        Sequence seq;
        seq.channel      = (int) parsed.getProperty ("channel", 1);
        seq.tempoBpm     = (int) parsed.getProperty ("tempoBpm", 120);
        seq.stepsPerBeat = (int) parsed.getProperty ("stepsPerBeat", 4);
        seq.engineTag    = parsed.getProperty ("engineTag", juce::String()).toString();

        seq.lockable.clear();
        if (const auto* larr = parsed.getProperty ("lockable", {}).getArray())
            for (const auto& v : *larr)
            {
                LockableParam lp;
                lp.paramId   = v.getProperty ("paramId", "").toString();
                lp.instance  = (int) v.getProperty ("instance", 1);
                lp.baseValue = (int) v.getProperty ("baseValue", 0);
                seq.lockable.push_back (lp);
            }

        if (const auto* sarr = parsed.getProperty ("steps", {}).getArray())
        {
            const int n = juce::jmin (16, sarr->size());
            for (int i = 0; i < n; ++i)
            {
                const auto& v = sarr->getReference (i);
                Step s;
                s.note        = (int) v.getProperty ("note", 60);
                s.velocity    = (int) v.getProperty ("velocity", 100);
                s.enabled     = (bool) v.getProperty ("enabled", false);
                s.gatePercent = (int) v.getProperty ("gatePercent", 90);

                if (const auto* lk = v.getProperty ("locks", {}).getArray())
                    for (const auto& lv : *lk)
                    {
                        ParamLock pl;
                        pl.paramId  = lv.getProperty ("paramId", "").toString();
                        pl.instance = (int) lv.getProperty ("instance", 1);
                        pl.value    = (int) lv.getProperty ("value", 0);
                        s.locks.push_back (pl);
                    }

                seq.steps[(size_t) i] = std::move (s);
            }
        }

        return seq;
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

    void randomize (Sequence& seq, juce::Random& rng, const RandomizeOptions& options)
    {
        using Scale = RandomizeOptions::Scale;
        static const std::vector<int> minorPent { 0, 3, 5, 7, 10 };
        static const std::vector<int> majorPent { 0, 2, 4, 7, 9 };
        static const std::vector<int> naturalMinor { 0, 2, 3, 5, 7, 8, 10 };
        static const std::vector<int> major { 0, 2, 4, 5, 7, 9, 11 };
        static const std::vector<int> chromatic { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
        const auto& degrees = options.scale == Scale::majorPentatonic ? majorPent
                            : options.scale == Scale::naturalMinor    ? naturalMinor
                            : options.scale == Scale::major           ? major
                            : options.scale == Scale::chromatic       ? chromatic
                                                                      : minorPent;

        // Every in-range note whose pitch class sits on the scale (transposed to the root).
        std::vector<int> allowedNotes;
        const int lo = juce::jlimit (0, 127, juce::jmin (options.noteMin, options.noteMax));
        const int hi = juce::jlimit (0, 127, juce::jmax (options.noteMin, options.noteMax));
        for (int n = lo; n <= hi; ++n)
        {
            const int pc = ((n - options.rootNote) % 12 + 12) % 12;
            if (std::find (degrees.begin(), degrees.end(), pc) != degrees.end())
                allowedNotes.push_back (n);
        }
        if (allowedNotes.empty())          // degenerate range (< one scale step wide)
            allowedNotes.push_back (lo);

        // Empty lockableIndices == every lockable is eligible.
        std::vector<int> eligible = options.lockableIndices;
        if (eligible.empty())
            for (int i = 0; i < (int) seq.lockable.size(); ++i)
                eligible.push_back (i);

        const auto intIn = [&rng] (int minV, int maxV)
        {
            return minV + rng.nextInt (juce::jmax (1, maxV - minV + 1));
        };

        for (auto& step : seq.steps)
        {
            step.enabled = rng.nextFloat() < options.trigDensity;
            step.note = allowedNotes[(size_t) rng.nextInt ((int) allowedNotes.size())];
            step.velocity = juce::jlimit (1, 127, intIn (options.velocityMin, options.velocityMax));
            step.gatePercent = juce::jlimit (1, 100, intIn (options.gateMin, options.gateMax));

            step.locks.clear();
            for (const int idx : eligible)
            {
                if (idx < 0 || idx >= (int) seq.lockable.size())
                    continue;
                const auto& lp = seq.lockable[(size_t) idx];
                if (rng.nextFloat() < options.lockDensity)
                    step.locks.push_back (ParamLock { lp.paramId, lp.instance,
                                                     intIn (lp.minValue, lp.maxValue) });
            }
        }
    }

    void randomize (Sequence& seq, juce::Random& rng)
    {
        randomize (seq, rng, RandomizeOptions {});
    }
}
