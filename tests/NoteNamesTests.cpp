#include <catch2/catch_test_macros.hpp>

#include "casioxw/NoteNames.h"

// Chunk 7c item 3 — casioxw::midiNoteName()/noteNameToMidi(), the pure int<->name converter
// backing the Key Follow Base params' Slider::textFromValueFunction/valueFromTextFunction in
// app/ParamControl.cpp. Convention (matches the manual's "C-1 to G9"): note 0 == "C-1",
// note 60 == "C4", note 127 == "G9", sharps only.

TEST_CASE ("midiNoteName: manual reference points", "[notenames]")
{
    CHECK (casioxw::midiNoteName (0) == "C-1");
    CHECK (casioxw::midiNoteName (60) == "C4");
    CHECK (casioxw::midiNoteName (127) == "G9");
}

TEST_CASE ("midiNoteName: sharps spot-check", "[notenames]")
{
    CHECK (casioxw::midiNoteName (61) == "C#4");    // one above C4
    CHECK (casioxw::midiNoteName (63) == "D#4");
    CHECK (casioxw::midiNoteName (70) == "A#4");
    CHECK (casioxw::midiNoteName (12) == "C0");      // octave boundary
    CHECK (casioxw::midiNoteName (11) == "B-1");
}

TEST_CASE ("midiNoteName: out-of-range input clamps rather than UB", "[notenames]")
{
    CHECK (casioxw::midiNoteName (-5) == casioxw::midiNoteName (0));
    CHECK (casioxw::midiNoteName (999) == casioxw::midiNoteName (127));
}

TEST_CASE ("noteNameToMidi: round-trips midiNoteName for every note 0..127", "[notenames]")
{
    for (int n = 0; n <= 127; ++n)
    {
        const auto name = casioxw::midiNoteName (n);
        const auto parsed = casioxw::noteNameToMidi (name);
        REQUIRE (parsed.has_value());
        CHECK (*parsed == n);
    }
}

TEST_CASE ("noteNameToMidi: case-insensitive and whitespace-tolerant", "[notenames]")
{
    CHECK (casioxw::noteNameToMidi ("c4") == 60);
    CHECK (casioxw::noteNameToMidi (" C4 ") == 60);
    CHECK (casioxw::noteNameToMidi ("g#5") == casioxw::noteNameToMidi ("G#5"));
}

TEST_CASE ("noteNameToMidi: rejects garbage instead of guessing", "[notenames]")
{
    CHECK_FALSE (casioxw::noteNameToMidi ("").has_value());
    CHECK_FALSE (casioxw::noteNameToMidi ("H4").has_value());       // no H in music notation
    CHECK_FALSE (casioxw::noteNameToMidi ("Cxyz").has_value());     // non-numeric octave
    CHECK_FALSE (casioxw::noteNameToMidi ("C").has_value());        // missing octave
    CHECK_FALSE (casioxw::noteNameToMidi ("C10").has_value());      // resolves to note 132, out of range
}
