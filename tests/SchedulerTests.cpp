#include <catch2/catch_test_macros.hpp>

#include "casioxw/Scheduler.h"
#include "casioxw/Sequence.h"

// Pure look-ahead scheduler (SEQUENCER_HANDOFF.md S3). scheduleStep() turns (pattern, step, tempo)
// into a time-ordered event list with no MIDI I/O and no real time, so the timeline logic the
// real-time transport relies on is exercised headlessly here. The actual timestamped output
// (app/SequencerPanel + juce::MidiOutput background thread) stays a hardware-verified boundary.

namespace
{
    using casioxw::ScheduledEvent;

    // A sequence with two lockable params (cutoff base 100, resonance base 0), mirroring the
    // panel's kLockables, so param dedup + locks can be exercised.
    casioxw::Sequence makeSeq()
    {
        casioxw::Sequence seq;
        seq.tempoBpm = 120;      // 500 ms/beat
        seq.stepsPerBeat = 4;    // 16ths -> 125 ms/step
        seq.lockable = {
            casioxw::LockableParam { "cutoff", 1, 100, 0, 127 },
            casioxw::LockableParam { "reso",   1, 0,   0, 127 },
        };
        return seq;
    }

    int countType (const std::vector<ScheduledEvent>& evs, ScheduledEvent::Type t)
    {
        int n = 0;
        for (const auto& e : evs)
            if (e.type == t)
                ++n;
        return n;
    }
}

TEST_CASE ("scheduleStep: enabled step, prev=-1 emits all params then note-on then note-off", "[scheduler]")
{
    auto seq = makeSeq();
    seq.steps[0].enabled = true;
    seq.steps[0].note = 60;
    seq.steps[0].velocity = 100;
    seq.steps[0].gatePercent = 50;   // half the 125 ms step -> note-off at 62.5 ms

    const auto evs = casioxw::scheduleStep (seq, 0, -1, 0.0);

    // 2 params (both forced by prev=-1) + note-on + note-off.
    REQUIRE (evs.size() == 4);
    CHECK (countType (evs, ScheduledEvent::Type::paramChange) == 2);
    CHECK (countType (evs, ScheduledEvent::Type::noteOn) == 1);
    CHECK (countType (evs, ScheduledEvent::Type::noteOff) == 1);

    // Ordering: every paramChange precedes the note-on, which precedes the note-off.
    int lastParam = -1, noteOn = -1, noteOff = -1;
    for (int i = 0; i < (int) evs.size(); ++i)
    {
        if (evs[(size_t) i].type == ScheduledEvent::Type::paramChange) lastParam = i;
        if (evs[(size_t) i].type == ScheduledEvent::Type::noteOn)      noteOn   = i;
        if (evs[(size_t) i].type == ScheduledEvent::Type::noteOff)     noteOff  = i;
    }
    CHECK (lastParam < noteOn);
    CHECK (noteOn < noteOff);
}

TEST_CASE ("scheduleStep: note-on/off carry the step's note/velocity and absolute times", "[scheduler]")
{
    auto seq = makeSeq();
    seq.channel = 3;
    seq.steps[2].enabled = true;
    seq.steps[2].note = 64;
    seq.steps[2].velocity = 90;
    seq.steps[2].gatePercent = 80;

    const double stepStart = 250.0;   // step 2 at 125 ms/step
    const auto evs = casioxw::scheduleStep (seq, 2, 1, stepStart);

    const ScheduledEvent* on = nullptr;
    const ScheduledEvent* off = nullptr;
    for (const auto& e : evs)
    {
        if (e.type == ScheduledEvent::Type::noteOn)  on  = &e;
        if (e.type == ScheduledEvent::Type::noteOff) off = &e;
    }
    REQUIRE (on != nullptr);
    REQUIRE (off != nullptr);
    CHECK (on->channel == 3);
    CHECK (on->note == 64);
    CHECK (on->velocity == 90);
    CHECK (on->timeMs == stepStart);
    // note-off at stepStart + gate = 250 + 125*0.8 = 350
    CHECK (off->timeMs == stepStart + casioxw::stepGateMs (seq, 2));
    CHECK (off->note == 64);
    CHECK (off->timeMs > on->timeMs);
}

TEST_CASE ("scheduleStep: gate 100% note-off lands exactly at the next step boundary", "[scheduler]")
{
    auto seq = makeSeq();
    seq.steps[0].enabled = true;
    seq.steps[0].gatePercent = 100;

    const auto evs = casioxw::scheduleStep (seq, 0, 0, 0.0);
    const ScheduledEvent* off = nullptr;
    for (const auto& e : evs)
        if (e.type == ScheduledEvent::Type::noteOff)
            off = &e;
    REQUIRE (off != nullptr);
    CHECK (off->timeMs == casioxw::stepIntervalMs (seq));   // == one full step
}

TEST_CASE ("scheduleStep: disabled step emits its changed params but no note", "[scheduler]")
{
    auto seq = makeSeq();
    seq.steps[5].enabled = false;
    casioxw::setStepLock (seq, 5, "cutoff", 1, 40);   // a p-lock on a rest is valid

    const auto evs = casioxw::scheduleStep (seq, 5, -1, 0.0);
    CHECK (countType (evs, ScheduledEvent::Type::noteOn) == 0);
    CHECK (countType (evs, ScheduledEvent::Type::noteOff) == 0);
    CHECK (countType (evs, ScheduledEvent::Type::paramChange) == 2);   // prev=-1 forces both
}

TEST_CASE ("scheduleStep: dedup suppresses params unchanged since the previous step", "[scheduler]")
{
    auto seq = makeSeq();
    // Steps 0 and 1 both inherit base for every param -> step 1 vs step 0 has no param change.
    seq.steps[1].enabled = true;

    const auto evs = casioxw::scheduleStep (seq, 1, 0, 125.0);
    CHECK (countType (evs, ScheduledEvent::Type::paramChange) == 0);   // nothing changed
    CHECK (countType (evs, ScheduledEvent::Type::noteOn) == 1);        // note still emitted
}

TEST_CASE ("scheduleStep: only the changed param is emitted when a step locks one of several", "[scheduler]")
{
    auto seq = makeSeq();
    seq.steps[1].enabled = true;
    casioxw::setStepLock (seq, 1, "cutoff", 1, 40);   // cutoff changes 100 -> 40; reso stays 0

    const auto evs = casioxw::scheduleStep (seq, 1, 0, 125.0);
    REQUIRE (countType (evs, ScheduledEvent::Type::paramChange) == 1);
    for (const auto& e : evs)
        if (e.type == ScheduledEvent::Type::paramChange)
        {
            CHECK (e.paramId == "cutoff");
            CHECK (e.value == 40);
        }
}

TEST_CASE ("scheduleStep: leaving a lock reverts to base (emitted at the step after the lock)", "[scheduler]")
{
    auto seq = makeSeq();
    casioxw::setStepLock (seq, 0, "cutoff", 1, 40);   // step 0 locks cutoff to 40
    // step 1 inherits base (100) -> cutoff must be re-sent to revert.

    const auto evs = casioxw::scheduleStep (seq, 1, 0, 125.0);
    REQUIRE (countType (evs, ScheduledEvent::Type::paramChange) == 1);
    for (const auto& e : evs)
        if (e.type == ScheduledEvent::Type::paramChange)
        {
            CHECK (e.paramId == "cutoff");
            CHECK (e.value == 100);   // reverted to base
        }
}
