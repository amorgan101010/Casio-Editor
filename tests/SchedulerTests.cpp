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

TEST_CASE ("scheduleStep: gate above 100% lands the note-off past the next step boundary", "[scheduler]")
{
    auto seq = makeSeq();
    seq.steps[0].enabled = true;
    seq.steps[0].gatePercent = 300;   // 3 steps' worth -- ties over steps 1 and 2

    const auto evs = casioxw::scheduleStep (seq, 0, 0, 0.0);
    const ScheduledEvent* off = nullptr;
    for (const auto& e : evs)
        if (e.type == ScheduledEvent::Type::noteOff)
            off = &e;
    REQUIRE (off != nullptr);
    CHECK (off->timeMs == casioxw::stepIntervalMs (seq) * 3.0);
}

TEST_CASE ("scheduleStep: a long gate is cut short by an earlier same-pitch retrig, not overrun", "[scheduler]")
{
    auto seq = makeSeq();
    seq.steps[0].enabled = true;
    seq.steps[0].note = 60;
    seq.steps[0].gatePercent = 800;   // would otherwise sustain 8 steps -- 1000 ms

    seq.steps[2].enabled = true;      // same pitch, 2 steps later -- must cut step 0's note here
    seq.steps[2].note = 60;

    const auto evs = casioxw::scheduleStep (seq, 0, 0, 0.0);
    const ScheduledEvent* off = nullptr;
    for (const auto& e : evs)
        if (e.type == ScheduledEvent::Type::noteOff)
            off = &e;
    REQUIRE (off != nullptr);
    CHECK (off->timeMs == 2.0 * casioxw::stepIntervalMs (seq));   // cut at step 2's onset, not step 8's
}

TEST_CASE ("scheduleStep: a long gate is ALSO cut short by a DIFFERENT-pitch retrig -- one line, one voice", "[scheduler]")
{
    auto seq = makeSeq();
    seq.steps[0].enabled = true;
    seq.steps[0].note = 60;
    seq.steps[0].gatePercent = 300;    // would otherwise sustain 3 steps -- 375 ms

    seq.steps[1].enabled = true;       // different pitch, one step later -- still cuts step 0:
    seq.steps[1].note = 64;            // a track/voice is monophonic, so ANY trig supersedes it.

    const auto evs = casioxw::scheduleStep (seq, 0, 0, 0.0);
    const ScheduledEvent* off = nullptr;
    for (const auto& e : evs)
        if (e.type == ScheduledEvent::Type::noteOff)
            off = &e;
    REQUIRE (off != nullptr);
    CHECK (off->timeMs == casioxw::stepIntervalMs (seq));         // cut at step 1's onset, not step 3's
    CHECK (off->note   == 60);                                    // the note-off is still for step 0's OWN pitch
}

TEST_CASE ("scheduleStep: a note with no other trig at all is capped by its own next lap", "[scheduler]")
{
    auto seq = makeSeq();
    seq.steps[5].enabled = true;
    seq.steps[5].note = 67;
    seq.steps[5].gatePercent = casioxw::kMaxGatePercent;   // full 16 steps -- no other step is enabled

    const auto evs = casioxw::scheduleStep (seq, 5, 4, 625.0);   // step 5 at 5*125ms
    const ScheduledEvent* off = nullptr;
    for (const auto& e : evs)
        if (e.type == ScheduledEvent::Type::noteOff)
            off = &e;
    REQUIRE (off != nullptr);
    // Cut at its own next occurrence: step 5 again, one full 16-step lap later.
    CHECK (off->timeMs == 625.0 + 16.0 * casioxw::stepIntervalMs (seq));
}

TEST_CASE ("scheduleStep: poly voices never cut each other -- each is its own Sequence", "[scheduler]")
{
    // A poly "sub-track" is modelled as an entirely separate casioxw::Sequence (SequencerPanel's
    // synthExtraVoices / PcmTrackControl::extraVoices), scheduled via its own scheduleStep() call.
    // There is no shared state between them at this layer, so voice B's steps existing at all can't
    // shorten voice A's gate -- this test exists to make that isolation explicit and regression-
    // proof, not just an emergent property of "we happened to pass a different seq".
    auto voiceA = makeSeq();
    voiceA.steps[0].enabled = true;
    voiceA.steps[0].note = 60;
    voiceA.steps[0].gatePercent = casioxw::kMaxGatePercent;   // full 16 steps

    auto voiceB = makeSeq();
    voiceB.steps[1].enabled = true;    // fires one step after voiceA's onset
    voiceB.steps[1].note = 60;         // even the SAME pitch as voiceA -- still a different Sequence

    const auto evs = casioxw::scheduleStep (voiceA, 0, 0, 0.0);
    const ScheduledEvent* off = nullptr;
    for (const auto& e : evs)
        if (e.type == ScheduledEvent::Type::noteOff)
            off = &e;
    REQUIRE (off != nullptr);
    CHECK (off->timeMs == 16.0 * casioxw::stepIntervalMs (voiceA));   // full 16 steps, untouched
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

TEST_CASE ("scheduleStep: prev=baseline emits no params when the step only inherits base", "[scheduler]")
{
    auto seq = makeSeq();
    seq.steps[0].enabled = true;   // step 0 has no locks -> every param inherits base

    // Device is already at base at play-start, so the first step must send only the note, never
    // the redundant baseline dump (contrast the prev=-1 case, which forces all params out).
    const auto evs = casioxw::scheduleStep (seq, 0, casioxw::kPrevStepBaseline, 0.0);
    CHECK (countType (evs, ScheduledEvent::Type::paramChange) == 0);
    CHECK (countType (evs, ScheduledEvent::Type::noteOn) == 1);
}

TEST_CASE ("scheduleStep: prev=baseline emits only the params the first step locks away from base", "[scheduler]")
{
    auto seq = makeSeq();
    seq.steps[0].enabled = true;
    casioxw::setStepLock (seq, 0, "cutoff", 1, 40);   // cutoff locked 100 -> 40; reso stays at base 0

    const auto evs = casioxw::scheduleStep (seq, 0, casioxw::kPrevStepBaseline, 0.0);
    REQUIRE (countType (evs, ScheduledEvent::Type::paramChange) == 1);   // only the genuine lock
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
