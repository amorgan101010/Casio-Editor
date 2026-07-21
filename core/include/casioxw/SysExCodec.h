#pragma once

#include "casioxw/ParamModel.h"
#include "casioxw/Version.h"

#include <cstdint>
#include <string>
#include <vector>

namespace casioxw
{
    /** XW-P1 parameter SysEx codec.

        Encodes a (paramId, instance, value) triple into the full wire frame

            F0 44 16 03 7F 01 <18-byte address> <value LSB-first> F7

        and decodes such a frame back, mirroring franky's V2SX / SX2v encoders
        and createSXtssArray address layout exactly (verified against golden
        vectors generated from the real Lua). Value math is raw — no range
        clamping (that is a UI concern). GUI-less. */
    class SysExCodec
    {
    public:
        explicit SysExCodec (ParamModel model);

        /** The XW-P1 solo-synth tone SysEx header: F0 44 16 03 7F 01 09. */
        static std::vector<std::uint8_t> soloSynthToneHeader();

        /** Encode a parameter edit (act=0x01, set) into the full wire frame.
            @param paramId   logical id, e.g. "tssOSCwf"
            @param instance  1-based instance (oscillator/LFO number, 1..count)
            @param value     UI value (may be negative for cf/cF/pk/tn) */
        std::vector<std::uint8_t> encode (const juce::String& paramId,
                                          int instance,
                                          int value) const;

        struct Decoded
        {
            bool ok = false;                          // address was recognised
            bool ambiguous = false;                   // >1 param shares this address (Lua typo)
            juce::String paramId;                     // first candidate
            std::vector<juce::String> candidates;     // all ids when ambiguous
            int instance = 1;                         // 1-based
            int value = 0;
            juce::String vt;
        };

        /** Decode a full wire frame back to {paramId, instance, value}. When the
            address is one of the flagged lfo1D/2D collisions the result carries
            ambiguous=true and every colliding id in `candidates` rather than
            guessing which parameter was meant. */
        Decoded decode (const std::vector<std::uint8_t>& frame) const;

        const ParamModel& model() const noexcept { return paramModel; }

    private:
        ParamModel paramModel;
    };
}
