#include <catch2/catch_test_macros.hpp>

#include "casioxw/Sequence.h"

// Sequencer MVP (SEQUENCER_HANDOFF.md) — pure logic only: stepEvent()/stepIntervalMs() have no
// MIDI I/O or real time, so the actual note-off/timing correctness (app/SequencerPanel.cpp's
// timerCallback) is a hardware-verified boundary, same as the rest of the app/ layer.

TEST_CASE ("stepEvent: disabled step is a rest", "[sequence]")
{
    casioxw::Sequence seq;
    seq.steps[3].enabled = false;
    CHECK_FALSE (casioxw::stepEvent (seq, 3).has_value());
}

TEST_CASE ("stepEvent: enabled step carries the sequence's channel + the step's note/velocity", "[sequence]")
{
    casioxw::Sequence seq;
    seq.channel = 5;
    seq.steps[7].enabled = true;
    seq.steps[7].note = 67;
    seq.steps[7].velocity = 88;

    const auto event = casioxw::stepEvent (seq, 7);
    REQUIRE (event.has_value());
    CHECK (event->channel == 5);
    CHECK (event->note == 67);
    CHECK (event->velocity == 88);
}

TEST_CASE ("stepEvent: steps are independent", "[sequence]")
{
    casioxw::Sequence seq;
    seq.steps[0].enabled = true;
    seq.steps[0].note = 40;
    seq.steps[1].enabled = false;

    CHECK (casioxw::stepEvent (seq, 0).has_value());
    CHECK_FALSE (casioxw::stepEvent (seq, 1).has_value());
}

TEST_CASE ("stepIntervalMs: default rate is 16th notes (4 steps per beat)", "[sequence]")
{
    casioxw::Sequence seq;
    seq.tempoBpm = 120;
    CHECK (casioxw::stepIntervalMs (seq) == 125.0);   // 60000/120 = 500ms/beat, /4 = 125ms/step

    seq.tempoBpm = 60;
    CHECK (casioxw::stepIntervalMs (seq) == 250.0);

    seq.tempoBpm = 240;
    CHECK (casioxw::stepIntervalMs (seq) == 62.5);
}

TEST_CASE ("stepIntervalMs: rate/time-scale scales with stepsPerBeat", "[sequence]")
{
    casioxw::Sequence seq;
    seq.tempoBpm = 120;   // 500 ms/beat

    seq.stepsPerBeat = 1;   CHECK (casioxw::stepIntervalMs (seq) == 500.0);   // 1/4 notes
    seq.stepsPerBeat = 2;   CHECK (casioxw::stepIntervalMs (seq) == 250.0);   // 1/8
    seq.stepsPerBeat = 3;   CHECK (casioxw::stepIntervalMs (seq) == 500.0 / 3.0);  // 1/8 triplet
    seq.stepsPerBeat = 4;   CHECK (casioxw::stepIntervalMs (seq) == 125.0);   // 1/16
    seq.stepsPerBeat = 8;   CHECK (casioxw::stepIntervalMs (seq) == 62.5);    // 1/32
}

TEST_CASE ("stepGateMs: note length is gatePercent of the step interval", "[sequence]")
{
    casioxw::Sequence seq;
    seq.tempoBpm = 120;     // 125 ms/step at 1/16

    seq.steps[0].gatePercent = 100;  CHECK (casioxw::stepGateMs (seq, 0) == 125.0);
    seq.steps[0].gatePercent = 50;   CHECK (casioxw::stepGateMs (seq, 0) == 62.5);
    seq.steps[0].gatePercent = 20;   CHECK (casioxw::stepGateMs (seq, 0) == 25.0);

    seq.steps[0].gatePercent = 0;    CHECK (casioxw::stepGateMs (seq, 0) == 1.25);  // clamped to 1%
}

// ---- p-locks -------------------------------------------------------------------------------

namespace
{
    casioxw::Sequence seqWithCutoff (int base)
    {
        casioxw::Sequence seq;
        seq.lockable.push_back (casioxw::LockableParam { "tssFLTFcoff", 1, base });
        return seq;
    }
}

TEST_CASE ("effectiveParamValue: an unlocked step inherits the base value", "[sequence][plock]")
{
    auto seq = seqWithCutoff (90);
    const auto v = casioxw::effectiveParamValue (seq, 4, "tssFLTFcoff", 1);
    REQUIRE (v.has_value());
    CHECK (*v == 90);
}

TEST_CASE ("effectiveParamValue: a locked step overrides the base for that step only", "[sequence][plock]")
{
    auto seq = seqWithCutoff (90);
    casioxw::setStepLock (seq, 4, "tssFLTFcoff", 1, 20);

    CHECK (*casioxw::effectiveParamValue (seq, 4, "tssFLTFcoff", 1) == 20);   // locked step
    CHECK (*casioxw::effectiveParamValue (seq, 5, "tssFLTFcoff", 1) == 90);   // neighbour = base
}

TEST_CASE ("effectiveParamValue: nullopt for a parameter the sequence doesn't know", "[sequence][plock]")
{
    auto seq = seqWithCutoff (90);
    CHECK_FALSE (casioxw::effectiveParamValue (seq, 0, "tssFLTFreso", 1).has_value());
}

TEST_CASE ("setStepLock: updates an existing lock in place rather than duplicating", "[sequence][plock]")
{
    auto seq = seqWithCutoff (90);
    casioxw::setStepLock (seq, 3, "tssFLTFcoff", 1, 10);
    casioxw::setStepLock (seq, 3, "tssFLTFcoff", 1, 40);

    CHECK (seq.steps[3].locks.size() == 1);
    CHECK (*casioxw::effectiveParamValue (seq, 3, "tssFLTFcoff", 1) == 40);
}

TEST_CASE ("clearStepLock / clearStepLocks revert to base", "[sequence][plock]")
{
    auto seq = seqWithCutoff (90);
    seq.lockable.push_back (casioxw::LockableParam { "tssFLTFreso", 1, 5 });
    casioxw::setStepLock (seq, 2, "tssFLTFcoff", 1, 10);
    casioxw::setStepLock (seq, 2, "tssFLTFreso", 1, 30);

    casioxw::clearStepLock (seq, 2, "tssFLTFcoff", 1);
    CHECK (*casioxw::effectiveParamValue (seq, 2, "tssFLTFcoff", 1) == 90);   // reverted
    CHECK (*casioxw::effectiveParamValue (seq, 2, "tssFLTFreso", 1) == 30);   // still locked

    casioxw::clearStepLocks (seq, 2);
    CHECK (seq.steps[2].locks.empty());
    CHECK (*casioxw::effectiveParamValue (seq, 2, "tssFLTFreso", 1) == 5);
}

TEST_CASE ("effectiveParamValues: one entry per lockable param, in order, honouring locks", "[sequence][plock]")
{
    auto seq = seqWithCutoff (90);
    seq.lockable.push_back (casioxw::LockableParam { "tssFLTFreso", 1, 5 });
    casioxw::setStepLock (seq, 1, "tssFLTFreso", 1, 33);

    const auto values = casioxw::effectiveParamValues (seq, 1);
    REQUIRE (values.size() == 2);
    CHECK (values[0].paramId == "tssFLTFcoff");
    CHECK (values[0].value == 90);          // unlocked -> base
    CHECK (values[1].paramId == "tssFLTFreso");
    CHECK (values[1].value == 33);          // locked
}

TEST_CASE ("setBaseValue: changes what unlocked steps resolve to", "[sequence][plock]")
{
    auto seq = seqWithCutoff (90);
    casioxw::setBaseValue (seq, "tssFLTFcoff", 1, 64);
    CHECK (*casioxw::effectiveParamValue (seq, 0, "tssFLTFcoff", 1) == 64);
}

// ---- shiftSteps ----------------------------------------------------------------------------

TEST_CASE ("shiftSteps: right by 1 moves each step's content one later, wrapping", "[sequence][shift]")
{
    casioxw::Sequence seq;
    for (int i = 0; i < 16; ++i)
        seq.steps[(size_t) i].note = i;   // tag each step by index

    casioxw::shiftSteps (seq, 1);
    CHECK (seq.steps[1].note == 0);    // step 0's content is now at step 1
    CHECK (seq.steps[0].note == 15);   // step 15 wrapped around to step 0
    CHECK (seq.steps[5].note == 4);
}

TEST_CASE ("shiftSteps: left by 1 re-anchors the pattern one step earlier", "[sequence][shift]")
{
    casioxw::Sequence seq;
    for (int i = 0; i < 16; ++i)
        seq.steps[(size_t) i].note = i;

    casioxw::shiftSteps (seq, -1);
    CHECK (seq.steps[0].note == 1);    // what was on step 1 now starts the pattern
    CHECK (seq.steps[15].note == 0);   // step 0 wrapped to the end
}

TEST_CASE ("shiftSteps: carries the whole step (gate + locks), and full rotation is identity", "[sequence][shift]")
{
    casioxw::Sequence seq;
    seq.lockable.push_back (casioxw::LockableParam { "tssFLTFcoff", 1, 90 });
    seq.steps[2].gatePercent = 42;
    casioxw::setStepLock (seq, 2, "tssFLTFcoff", 1, 17);

    casioxw::shiftSteps (seq, 3);
    CHECK (seq.steps[5].gatePercent == 42);
    REQUIRE (seq.steps[5].locks.size() == 1);
    CHECK (seq.steps[5].locks[0].value == 17);

    casioxw::shiftSteps (seq, 13);   // 3 + 13 == 16 == identity
    CHECK (seq.steps[2].gatePercent == 42);
    CHECK (seq.steps[2].locks[0].value == 17);
}

// ---- randomize -----------------------------------------------------------------------------

TEST_CASE ("randomize: notes/velocities/lock values stay in range", "[sequence][random]")
{
    auto seq = seqWithCutoff (90);
    seq.lockable[0].minValue = 0;
    seq.lockable[0].maxValue = 127;

    juce::Random rng (12345);
    casioxw::randomize (seq, rng);

    for (const auto& step : seq.steps)
    {
        CHECK (step.note >= 0);
        CHECK (step.note <= 127);
        CHECK (step.velocity >= 1);
        CHECK (step.velocity <= 127);
        CHECK (step.gatePercent >= 1);
        CHECK (step.gatePercent <= 100);
        for (const auto& lock : step.locks)
        {
            CHECK (lock.value >= 0);
            CHECK (lock.value <= 127);
            CHECK (lock.paramId == "tssFLTFcoff");   // only ever locks known lockable params
        }
    }
}

TEST_CASE ("randomize: deterministic for a given seed", "[sequence][random]")
{
    auto a = seqWithCutoff (90);
    auto b = seqWithCutoff (90);

    juce::Random rngA (777);
    juce::Random rngB (777);
    casioxw::randomize (a, rngA);
    casioxw::randomize (b, rngB);

    for (int i = 0; i < 16; ++i)
    {
        CHECK (a.steps[(size_t) i].enabled     == b.steps[(size_t) i].enabled);
        CHECK (a.steps[(size_t) i].note        == b.steps[(size_t) i].note);
        CHECK (a.steps[(size_t) i].velocity    == b.steps[(size_t) i].velocity);
        CHECK (a.steps[(size_t) i].gatePercent == b.steps[(size_t) i].gatePercent);
        REQUIRE (a.steps[(size_t) i].locks.size() == b.steps[(size_t) i].locks.size());
    }
}

TEST_CASE ("randomize: leaves channel/tempo/rate and the lockable set untouched", "[sequence][random]")
{
    auto seq = seqWithCutoff (90);
    seq.channel = 7;
    seq.tempoBpm = 100;
    seq.stepsPerBeat = 3;

    juce::Random rng (1);
    casioxw::randomize (seq, rng);

    CHECK (seq.channel == 7);
    CHECK (seq.tempoBpm == 100);
    CHECK (seq.stepsPerBeat == 3);
    REQUIRE (seq.lockable.size() == 1);
    CHECK (seq.lockable[0].baseValue == 90);   // base untouched; only per-step locks change
}
