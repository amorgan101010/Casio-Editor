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
