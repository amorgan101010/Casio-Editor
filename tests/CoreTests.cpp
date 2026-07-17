#include <catch2/catch_test_macros.hpp>

#include "casioxw/SysExCodec.h"

TEST_CASE ("casioxw_core version is reported", "[core]")
{
    // Asserts against an out-of-line function so the test genuinely links
    // against the casioxw_core static library.
    REQUIRE (casioxw::coreVersion() == "0.1.0");
}

TEST_CASE ("SysEx solo synth tone header matches the XW-P1 map", "[core][sysex]")
{
    const auto header = casioxw::SysExCodec::soloSynthToneHeader();
    const std::vector<std::uint8_t> expected { 0xF0, 0x44, 0x16, 0x03, 0x7F, 0x01, 0x09 };
    REQUIRE (header == expected);
}
