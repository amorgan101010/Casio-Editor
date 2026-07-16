#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace casioxw
{
    /** Returns the casioxw_core version string.

        Declared out-of-line (defined in SysExCodec.cpp) on purpose: linking
        against this symbol proves the static library actually links, which is
        what the tests assert.
    */
    std::string coreVersion();

    /** Placeholder SysEx codec for the Casio XW-P1.

        Real SysEx encode/decode (the reverse-engineered g_XWSysEx map) lands in
        Chunk 5; this stub exists only so the module compiles, links, and has a
        home for the golden-file tests.
    */
    class SysExCodec
    {
    public:
        SysExCodec() = default;

        /** The XW-P1 manufacturer/model SysEx header:
            F0 44 16 03 7F 01 09 ... (Casio, XW-P1, solo synth tone).
            Exposed now so Chunk 5 tests can pin it. */
        static std::vector<std::uint8_t> soloSynthToneHeader();

        /** Trivial round-trip placeholder: returns bytes unchanged for now. */
        std::vector<std::uint8_t> encode(const std::vector<std::uint8_t>& payload) const;
    };
}
