#pragma once

#include <juce_core/juce_core.h>

#include <optional>

namespace casioxw
{
    /** MIDI-note-number <-> note-name conversions for params whose manual range is stated as
        "C-1 to G9" rather than a raw integer (Chunk 7c item 3, e.g. the four Key Follow Base
        params: tssOSCPkeyfB/tssOSCFkeyfB/tssOSCAkeyfB/tssFLTFkeyfB). Pure functions, no JUCE GUI
        type involved, so they're Catch2-testable from casioxw_core same as decideControlKind().

        Convention: MIDI note 0 = "C-1", 60 = "C4", 127 = "G9" (octave = note/12 - 1). Sharps only
        (no flats) — matches how the rest of the codebase already spells accidentals. */

    /** note must be in [0,127]; asserts (debug) / clamps (release) outside that range.
        Examples: midiNoteName(0) == "C-1", midiNoteName(60) == "C4", midiNoteName(127) == "G9",
        midiNoteName(61) == "C#4". */
    juce::String midiNoteName (int note);

    /** Parses a note name in the same style midiNoteName() produces (e.g. "C-1", "G#5", "C4") —
        case-insensitive, tolerates surrounding whitespace. Returns nullopt for anything that
        doesn't parse as <letter A-G>[#]<signed octave>, or that resolves outside [0,127]. */
    std::optional<int> noteNameToMidi (const juce::String& name);
}
