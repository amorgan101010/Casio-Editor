// Manual diagnostic tool for talking to a real XW-P1 over MIDI: request/read a
// parameter's current value, or set one. Not part of the automated test suite —
// this is the human-in-the-loop hardware gate, run by hand against real hardware.
//
// Usage:
//   midi-probe get <paramId> <instance>
//   midi-probe set <paramId> <instance> <value>
//   midi-probe organ-drawbar-test <instance 1-9>   (GET/SET/GET round-trip diagnostic)

#include <casioxw/MidiIO.h>
#include <casioxw/ParamModel.h>
#include <casioxw/SysExCodec.h>

#include <juce_core/juce_core.h>

#include <chrono>
#include <cstdio>
#include <optional>
#include <thread>

namespace
{
    juce::String hex (const std::vector<std::uint8_t>& b)
    {
        juce::String s;
        for (auto byte : b)
            s << juce::String::toHexString ((int) byte).paddedLeft ('0', 2) << " ";
        return s.trim();
    }

    // Sends a get-request for (paramId, instance) and polls the receive queue briefly for the
    // matching reply. Returns nullopt if nothing matching arrived within the timeout. Shared by
    // "get" (which prints every reply frame, matching or not) and the drawbar round-trip test
    // (which only cares about this one param/instance's value).
    std::optional<int> getValue (casioxw::MidiIO& io, const casioxw::SysExCodec& codec,
                                  const juce::String& paramId, int instance,
                                  int timeoutMs = 500)
    {
        io.sendFrame (casioxw::MidiIO::syncRequest (codec, paramId, instance));
        for (int waited = 0; waited < timeoutMs; waited += 25)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (25));
            for (auto& frame : io.drainReceived())
            {
                auto d = codec.decode (frame);
                if (d.ok && ! d.ambiguous && d.paramId == paramId && d.instance == instance)
                    return d.value;
            }
        }
        return std::nullopt;
    }
}

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::fprintf (stderr, "usage: midi-probe get <paramId> <instance>\n"
                               "       midi-probe set <paramId> <instance> <value>\n"
                               "       midi-probe organ-drawbar-test <instance 1-9>\n");
        return 1;
    }
    const juce::String mode = argv[1];

    // Every mode except organ-drawbar-test (which only takes an instance number in argv[2])
    // expects at least argv[2]/argv[3] the same way it always has -- scan/setraw below dereference
    // them unchecked, relying on this gate.
    if (mode == "organ-drawbar-test")
    {
        if (argc < 3)
        {
            std::fprintf (stderr, "organ-drawbar-test requires: <instance 1-9>\n");
            return 1;
        }
    }
    else if (argc < 4)
    {
        std::fprintf (stderr, "usage: midi-probe get <paramId> <instance>\n"
                               "       midi-probe set <paramId> <instance> <value>\n"
                               "       midi-probe organ-drawbar-test <instance 1-9>\n");
        return 1;
    }

    const juce::String paramId = argc > 2 ? juce::String (argv[2]) : juce::String();
    const int instance = argc > 3 ? juce::String (argv[3]).getIntValue() : 0;

    const auto jsonPath = juce::File::getCurrentWorkingDirectory().getChildFile ("params/xwp1.json");
    if (! jsonPath.existsAsFile())
    {
        std::fprintf (stderr, "params/xwp1.json not found relative to cwd (%s) — run from repo root\n",
                      jsonPath.getFullPathName().toRawUTF8());
        return 1;
    }

    casioxw::SysExCodec codec (casioxw::ParamModel::fromFile (jsonPath));

    auto outDev = casioxw::MidiDevices::findOutputContaining ("CASIO");
    auto inDev  = casioxw::MidiDevices::findInputContaining ("CASIO");
    if (! outDev || ! inDev)
    {
        std::fprintf (stderr, "CASIO MIDI device not found.\nOutputs seen:\n");
        for (auto& d : casioxw::MidiDevices::availableOutputs())
            std::fprintf (stderr, "  - %s\n", d.name.toRawUTF8());
        std::fprintf (stderr, "Inputs seen:\n");
        for (auto& d : casioxw::MidiDevices::availableInputs())
            std::fprintf (stderr, "  - %s\n", d.name.toRawUTF8());
        return 1;
    }

    casioxw::MidiIO io;
    if (! io.openOutput (outDev->identifier) || ! io.openInput (inDev->identifier))
    {
        std::fprintf (stderr, "Failed to open CASIO MIDI ports\n");
        return 1;
    }

    std::printf ("Opened output: %s\nOpened input:  %s\n",
                 outDev->name.toRawUTF8(), inDev->name.toRawUTF8());

    if (mode == "set")
    {
        if (argc < 5)
        {
            std::fprintf (stderr, "set requires a value: midi-probe set <paramId> <instance> <value>\n");
            return 1;
        }
        const int value = juce::String (argv[4]).getIntValue();
        const auto frame = codec.encode (paramId, instance, value);
        std::printf ("SET  %s[%d] = %d\nFrame: %s\n", paramId.toRawUTF8(), instance, value,
                     hex (frame).toRawUTF8());
        io.sendFrame (frame);
        std::printf ("Sent. Check the XW-P1's screen/sound.\n");
        return 0;
    }

    if (mode == "get")
    {
        const auto req = casioxw::MidiIO::syncRequest (codec, paramId, instance);
        std::printf ("REQUEST %s[%d]\nFrame: %s\n", paramId.toRawUTF8(), instance, hex (req).toRawUTF8());
        io.sendFrame (req);

        // Reply arrives asynchronously on JUCE's MIDI thread; poll briefly.
        for (int i = 0; i < 20; ++i)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (50));
            for (auto& frame : io.drainReceived())
            {
                std::printf ("Reply frame: %s\n", hex (frame).toRawUTF8());
                auto d = codec.decode (frame);
                if (! d.ok)
                {
                    std::printf ("  (did not decode as a known XW-P1 address)\n");
                    continue;
                }
                std::printf ("  paramId=%s instance=%d value=%d%s\n",
                             d.paramId.toRawUTF8(), d.instance, d.value,
                             d.ambiguous ? " (AMBIGUOUS — collision, see candidates)" : "");
            }
        }
        std::printf ("Done waiting (1s). If nothing printed above, no reply arrived.\n");
        return 0;
    }

    if (mode == "setraw")
    {
        // setraw <ct> <id> <value>  (single unsigned 7-bit value byte, block/instance/ai/an = 0)
        const int ct    = juce::String (argv[2]).getIntValue();
        const int id    = juce::String (argv[3]).getIntValue();
        const int value = argc > 4 ? juce::String (argv[4]).getIntValue() : 0;
        std::vector<std::uint8_t> frame = {
            0xF0, 0x44, 0x16, 0x03, 0x7F, 0x01, (std::uint8_t) ct,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            (std::uint8_t) id, 0, 0, 0, 0, 0,
            (std::uint8_t) (value & 0x7F), 0xF7
        };
        std::printf ("SETRAW ct=%d id=0x%02X value=%d\nFrame: %s\n", ct, id, value, hex (frame).toRawUTF8());
        io.sendFrame (frame);
        std::printf ("Sent.\n");
        return 0;
    }

    if (mode == "scan")
    {
        // scan <ct> <idFrom> <idTo> [wantValue]
        // Sweeps raw get requests over category <ct>, param-id byte idFrom..idTo (instance 1,
        // ai=0, an=0), printing every reply's id + value byte. Diagnostic for finding a
        // mistranscribed address: set the target on the synth to a known value, then look for
        // the id whose reply carries it. Bypasses the codec's paramId map entirely (raw bytes).
        const int ct     = juce::String (argv[2]).getIntValue();
        const int idFrom = juce::String (argv[3]).getIntValue();
        const int idTo   = juce::String (argc > 4 ? argv[4] : argv[3]).getIntValue();
        const int wantValue = argc > 5 ? juce::String (argv[5]).getIntValue() : -1;

        std::printf ("SCAN ct=%d id=%d..%d%s\n", ct, idFrom, idTo,
                     wantValue >= 0 ? (" looking for value " + juce::String (wantValue)).toRawUTF8() : "");

        for (int id = idFrom; id <= idTo; ++id)
        {
            std::vector<std::uint8_t> req = {
                0xF0, 0x44, 0x16, 0x03, 0x7F, 0x00, (std::uint8_t) ct,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                (std::uint8_t) id, 0, 0, 0, 0, 0, 0xF7
            };
            io.sendFrame (req);

            std::this_thread::sleep_for (std::chrono::milliseconds (35));
            for (auto& frame : io.drainReceived())
            {
                if (frame.size() < 26) continue;
                const int replyId    = frame[18];
                const int replyValue = frame[24];
                const bool hit = (wantValue < 0) || (replyValue == wantValue);
                if (hit)
                    std::printf ("  id 0x%02X (%3d) -> value 0x%02X (%3d)%s\n",
                                 replyId, replyId, replyValue, replyValue,
                                 (wantValue >= 0 && replyValue == wantValue) ? "   <== MATCH" : "");
            }
        }
        std::printf ("Scan done.\n");
        return 0;
    }

    if (mode == "organ-drawbar-test")
    {
        // organ-drawbar-test <instance 1-9>
        //
        // Diagnoses the "drawbar SET does nothing" report: the owner confirmed sync/GET already
        // reflects the real patch's per-bar values correctly (organPosition's address/decode is
        // trusted), but moving the panel's fader has no audible/visible effect on the synth. This
        // does a GET/SET/GET round-trip on the target bar (does our own SET actually change what
        // GET reads back?) plus the SAME check on a second bar (did the write land somewhere
        // else / alias onto a different bar instead of doing nothing at all?).
        //
        // This only tells you whether OUR frame changes what OUR reads see -- it can't confirm
        // the drawbar audibly/visibly changed on the real hardware. Watch the synth's screen
        // and/or listen while this runs, and read both together.
        //
        // NOTE: this mode's one argument (the drawbar number) lands in argv[2], not argv[3] --
        // it takes only ONE arg after the mode name, unlike get/set's <paramId> <instance> shape
        // that the shared `paramId`/`instance` locals above are parsed for. Use `paramId` (the
        // raw argv[2] string) here, not `instance` (which reads argv[3] and would silently be 0).
        const int drawbar = paramId.getIntValue();
        if (drawbar < 1 || drawbar > 9)
        {
            std::fprintf (stderr, "instance must be 1-9 (drawbar: 1=16' .. 9=1')\n");
            return 1;
        }
        const int otherInstance = (drawbar == 1) ? 2 : 1;

        const auto before = getValue (io, codec, "organPosition", drawbar);
        if (! before.has_value())
        {
            std::printf ("No reply reading organPosition[%d] baseline -- is a Drawbar Organ tone "
                          "loaded on the synth? Aborting.\n", drawbar);
            return 1;
        }
        const auto otherBefore = getValue (io, codec, "organPosition", otherInstance);

        const int testValue = (*before == 0) ? 8 : 0;
        std::printf ("Baseline: organPosition[%d] = %d (other bar %d = %s)\n",
                     drawbar, *before, otherInstance,
                     otherBefore.has_value() ? juce::String (*otherBefore).toRawUTF8() : "no reply");
        std::printf ("Setting organPosition[%d] = %d ...\n", drawbar, testValue);

        const auto frame = codec.encode ("organPosition", drawbar, testValue);
        std::printf ("Frame: %s\n", hex (frame).toRawUTF8());
        io.sendFrame (frame);
        std::this_thread::sleep_for (std::chrono::milliseconds (150));

        const auto after = getValue (io, codec, "organPosition", drawbar);
        const auto otherAfter = getValue (io, codec, "organPosition", otherInstance);

        std::printf ("Read back: organPosition[%d] = %s (other bar %d = %s)\n",
                     drawbar, after.has_value() ? juce::String (*after).toRawUTF8() : "no reply",
                     otherInstance,
                     otherAfter.has_value() ? juce::String (*otherAfter).toRawUTF8() : "no reply");

        if (after.has_value() && *after == testValue)
            std::printf ("PASS: the SET took effect on bar %d's own readback.\n", drawbar);
        else
            std::printf ("FAIL: bar %d's readback did not change to %d -- the SET had no effect "
                         "on this address (matches the reported symptom).\n", drawbar, testValue);

        if (otherBefore.has_value() && otherAfter.has_value() && *otherBefore != *otherAfter)
            std::printf ("NOTE: bar %d's value ALSO changed (%d -> %d) -- the write may be "
                         "aliasing onto the wrong bar rather than doing nothing.\n",
                         otherInstance, *otherBefore, *otherAfter);

        std::printf ("\nNow check the synth itself: did bar %d actually move/sound different? "
                     "Report both this printout and what you saw/heard.\n", drawbar);
        return 0;
    }

    std::fprintf (stderr, "unknown mode '%s' (expected get|set|setraw|scan|organ-drawbar-test)\n", mode.toRawUTF8());
    return 1;
}
