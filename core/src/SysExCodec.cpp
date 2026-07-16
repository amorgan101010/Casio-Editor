#include "casioxw/SysExCodec.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace casioxw
{
    std::string coreVersion()
    {
        return "0.1.0";
    }

    std::vector<std::uint8_t> SysExCodec::soloSynthToneHeader()
    {
        // F0 44 16 03 7F 01 09 — Casio XW-P1 solo synth tone SysEx header.
        return { 0xF0, 0x44, 0x16, 0x03, 0x7F, 0x01, 0x09 };
    }

    std::vector<std::uint8_t> SysExCodec::encode(const std::vector<std::uint8_t>& payload) const
    {
        // Genuinely exercise juce_audio_basics at compile/link time by wrapping
        // the payload in a real MIDI SysEx message. Content is a pass-through
        // for now (real encoding lands in Chunk 5).
        if (! payload.empty())
        {
            const juce::MidiMessage msg = juce::MidiMessage::createSysExMessage(
                payload.data(), static_cast<int>(payload.size()));
            juce::ignoreUnused(msg);
        }

        return payload;
    }
}
