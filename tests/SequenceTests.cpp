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

TEST_CASE ("stepIntervalMs: 16th note at given BPM (4 steps per beat)", "[sequence]")
{
    casioxw::Sequence seq;
    seq.tempoBpm = 120;
    CHECK (casioxw::stepIntervalMs (seq) == 125.0);   // 60000/120 = 500ms/beat, /4 = 125ms/step

    seq.tempoBpm = 60;
    CHECK (casioxw::stepIntervalMs (seq) == 250.0);

    seq.tempoBpm = 240;
    CHECK (casioxw::stepIntervalMs (seq) == 62.5);
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
