// Manual diagnostic tool for talking to a real XW-P1 over MIDI: request/read a
// parameter's current value, or set one. Not part of the automated test suite —
// this is the human-in-the-loop hardware gate, run by hand against real hardware.
//
// Usage:
//   midi-probe get <paramId> <instance>
//   midi-probe set <paramId> <instance> <value>

#include <casioxw/MidiIO.h>
#include <casioxw/ParamModel.h>
#include <casioxw/SysExCodec.h>

#include <juce_core/juce_core.h>

#include <chrono>
#include <cstdio>
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
}

int main (int argc, char* argv[])
{
    if (argc < 4)
    {
        std::fprintf (stderr, "usage: midi-probe get <paramId> <instance>\n"
                               "       midi-probe set <paramId> <instance> <value>\n");
        return 1;
    }

    const juce::String mode = argv[1];
    const juce::String paramId = argv[2];
    const int instance = juce::String (argv[3]).getIntValue();

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

    std::fprintf (stderr, "unknown mode '%s' (expected get|set)\n", mode.toRawUTF8());
    return 1;
}
