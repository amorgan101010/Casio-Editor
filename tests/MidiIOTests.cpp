#include <catch2/catch_test_macros.hpp>

#include "casioxw/MidiIO.h"
#include "casioxw/SysExCodec.h"

#include <juce_core/juce_core.h>

#include <vector>

// Chunk 6a — the offline-buildable half of MIDI I/O. No real MIDI port is opened anywhere in
// this file: device enumeration is exercised as-is (headless CI has zero MIDI devices, which
// must not crash), and the receive path is driven directly via handleRawSysEx() with hand-built
// byte buffers rather than through a real juce::MidiInput callback.
//
// NOTE (see .wolf/buglog.json): calling juce::MidiInput/MidiOutput::getAvailableDevices() from a
// plain console binary (no juce::MessageManager, no JUCE event loop — Catch2 owns main()) prints
// harmless "JUCE Assertion failure" lines and a "Leaked objects detected: AsyncUpdater" notice at
// process exit, from JUCE's ALSA hot-plug watcher (MidiDeviceListConnectionBroadcaster) expecting
// a running message loop. This does NOT fail the run (exit code 0, assertion count unaffected).
// Tried fixing it with a static juce::ScopedJuceInitialiser_GUI to give it a MessageManager —
// that made things WORSE (a real "double free or corruption" at teardown, because the watcher's
// static destruction order relative to the initialiser's is undefined), so it was reverted.
// Conclusion: leave it alone; do not manage JUCE's MessageManager lifecycle from this headless
// test binary.

namespace
{

    std::vector<std::uint8_t> parseHexFrame (const juce::String& hex)
    {
        std::vector<std::uint8_t> bytes;
        for (const auto& tok : juce::StringArray::fromTokens (hex, " ", ""))
            if (tok.isNotEmpty())
                bytes.push_back ((std::uint8_t) tok.getHexValue32());
        return bytes;
    }

    casioxw::ParamModel loadModel()
    {
        return casioxw::ParamModel::fromFile (juce::File (CASIOXW_PARAMS_JSON));
    }
}

TEST_CASE ("MidiDevices: enumeration and substring lookup do not crash with zero devices", "[midiio][devices]")
{
    std::vector<juce::MidiDeviceInfo> inputs, outputs;
    REQUIRE_NOTHROW (inputs = casioxw::MidiDevices::availableInputs());
    REQUIRE_NOTHROW (outputs = casioxw::MidiDevices::availableOutputs());

    // "CASIO" default-select helper: must not crash and must return nullopt (not garbage) when
    // no matching device — or no device at all — is present. Headless CI typically has zero.
    const auto in  = casioxw::MidiDevices::findInputContaining ("CASIO");
    const auto out = casioxw::MidiDevices::findOutputContaining ("CASIO");
    if (inputs.empty())
        REQUIRE_FALSE (in.has_value());
    if (outputs.empty())
        REQUIRE_FALSE (out.has_value());

    WARN ("MIDI inputs seen: " << inputs.size() << " | outputs seen: " << outputs.size());
}

TEST_CASE ("MidiIO: handleRawSysEx feeds a hand-built frame through the receive queue and it decodes correctly",
          "[midiio][receive]")
{
    casioxw::MidiIO io;
    REQUIRE (io.pendingCount() == 0);

    // Golden vector: tssOSCsw instance=1 value=1 (act=0x01 set, single nf value byte).
    const auto frame = parseHexFrame (
        "f0 44 16 03 7f 01 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 f7");

    io.handleRawSysEx (frame.data(), (int) frame.size());
    REQUIRE (io.pendingCount() == 1);

    const auto drained = io.drainReceived();
    REQUIRE (drained.size() == 1);
    REQUIRE (drained[0] == frame);
    REQUIRE (io.pendingCount() == 0);          // drainReceived() empties the queue
    REQUIRE (io.drainReceived().empty());      // idempotent when nothing new arrived

    const casioxw::SysExCodec codec (loadModel());
    const auto decoded = codec.decode (drained[0]);
    REQUIRE (decoded.ok);
    REQUIRE_FALSE (decoded.ambiguous);
    REQUIRE (decoded.paramId == "tssOSCsw");
    REQUIRE (decoded.instance == 1);
    REQUIRE (decoded.value == 1);
}

TEST_CASE ("MidiIO: handleRawSysEx queues multiple frames in arrival order", "[midiio][receive]")
{
    casioxw::MidiIO io;
    const auto frameA = parseHexFrame (
        "f0 44 16 03 7f 01 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 f7");
    const auto frameB = parseHexFrame (
        "f0 44 16 03 7f 01 09 00 00 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 00 f7");

    io.handleRawSysEx (frameA.data(), (int) frameA.size());
    io.handleRawSysEx (frameB.data(), (int) frameB.size());
    REQUIRE (io.pendingCount() == 2);

    const auto drained = io.drainReceived();
    REQUIRE (drained.size() == 2);
    REQUIRE (drained[0] == frameA);
    REQUIRE (drained[1] == frameB);
}

TEST_CASE ("MidiIO: handleRawSysEx ignores malformed input rather than queueing garbage",
          "[midiio][receive]")
{
    casioxw::MidiIO io;

    const std::vector<std::uint8_t> notSysex { 0x90, 0x40, 0x7F };   // note-on, no F0/F7 at all
    io.handleRawSysEx (notSysex.data(), (int) notSysex.size());
    REQUIRE (io.pendingCount() == 0);

    const std::vector<std::uint8_t> tooShort { 0xF0, 0xF7 };         // starts/ends right but empty
    io.handleRawSysEx (tooShort.data(), (int) tooShort.size());
    REQUIRE (io.pendingCount() == 0);

    io.handleRawSysEx (nullptr, 0);
    REQUIRE (io.pendingCount() == 0);
}

TEST_CASE ("MidiIO::syncRequest produces the act=0x00 request frame for a known param",
          "[midiio][sync]")
{
    const casioxw::SysExCodec codec (loadModel());

    // Golden vector: tssOSCsw instance=1 value=0 -> the act=0x01 "set" frame.
    const auto setFrame = parseHexFrame (
        "f0 44 16 03 7f 01 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 f7");
    REQUIRE (codec.encode ("tssOSCsw", 1, 0) == setFrame);

    // Expected request: identical header + 18-byte address, act byte (index 5) flipped from
    // 0x01 to 0x00, and the value byte(s) dropped entirely (reference/PROTOCOL.md §1, §7).
    std::vector<std::uint8_t> expectedRequest (setFrame.begin(), setFrame.begin() + 24);
    expectedRequest[5] = 0x00;
    expectedRequest.push_back (0xF7);
    REQUIRE (expectedRequest.size() == 25);

    const auto actual = casioxw::MidiIO::syncRequest (codec, "tssOSCsw", 1);
    REQUIRE (actual == expectedRequest);
}

TEST_CASE ("MidiIO::syncRequest also matches for a multi-byte-value / per-instance param (tssOSCwf, osc 3)",
          "[midiio][sync]")
{
    const casioxw::SysExCodec codec (loadModel());

    const auto setFrame = codec.encode ("tssOSCwf", 3, 0);   // wf: 3 value bytes
    std::vector<std::uint8_t> expectedRequest (setFrame.begin(), setFrame.begin() + 24);
    expectedRequest[5] = 0x00;
    expectedRequest.push_back (0xF7);

    const auto actual = casioxw::MidiIO::syncRequest (codec, "tssOSCwf", 3);
    REQUIRE (actual == expectedRequest);
    REQUIRE (actual.size() == 25);   // 6 header + 18 address + F7, regardless of the set frame's vt
}

TEST_CASE ("MidiIO: sendFrame with no output open returns false rather than crashing",
          "[midiio][send]")
{
    casioxw::MidiIO io;
    REQUIRE_FALSE (io.isOutputOpen());

    const auto frame = parseHexFrame (
        "f0 44 16 03 7f 00 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 f7");
    REQUIRE_FALSE (io.sendFrame (frame));
}

TEST_CASE ("MidiIO: channel-voice sends with no output open return false rather than crashing",
          "[midiio][send]")
{
    casioxw::MidiIO io;
    REQUIRE_FALSE (io.isOutputOpen());

    REQUIRE_FALSE (io.sendNoteOn (1, 60, 100));
    REQUIRE_FALSE (io.sendNoteOff (1, 60));
    REQUIRE_FALSE (io.sendAllNotesOff (1));
}

TEST_CASE ("MidiIO: timestamped-playback API is safe to call with no output open", "[midiio][send]")
{
    casioxw::MidiIO io;
    REQUIRE_FALSE (io.isOutputOpen());

    // sendMessageNow returns false (like the other sends); the playback-thread + scheduleBlock
    // calls are no-ops that must not crash when there's nothing to send to.
    REQUIRE_FALSE (io.sendMessageNow (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100)));

    juce::MidiBuffer buffer;
    buffer.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    io.startPlaybackThread();
    io.scheduleBlock (buffer, 1000.0, 1000.0);
    io.stopPlaybackThread();
    SUCCEED ("no crash with no output open");
}
