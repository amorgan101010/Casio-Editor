#include "casioxw/NoteNames.h"

namespace casioxw
{
    namespace
    {
        // Index 0..11 = C, C#, D, D#, E, F, F#, G, G#, A, A#, B — sharps only, matches the rest
        // of the codebase's accidental spelling (see reference/midi-spec.md wave/enum labels).
        constexpr const char* kPitchClasses[12] =
        {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };

        int pitchClassIndex (juce::String letter, bool sharp)
        {
            letter = letter.toUpperCase();
            static const juce::String kLetters = "CDEFGAB";
            static const int kNatural[7] = { 0, 2, 4, 5, 7, 9, 11 };   // C D E F G A B -> semitone
            const int li = kLetters.indexOf (letter);
            if (li < 0)
                return -1;
            int pc = kNatural[li];
            if (sharp)
                pc = (pc + 1) % 12;
            return pc;
        }
    }

    juce::String midiNoteName (int note)
    {
        note = juce::jlimit (0, 127, note);
        const int pc = note % 12;
        const int octave = note / 12 - 1;   // note>=0 so integer division truncates toward -1 correctly
        return juce::String (kPitchClasses[pc]) + juce::String (octave);
    }

    std::optional<int> noteNameToMidi (const juce::String& nameIn)
    {
        const juce::String name = nameIn.trim();
        if (name.isEmpty())
            return std::nullopt;

        int i = 0;
        const juce::String letter = name.substring (i, i + 1);
        ++i;
        bool sharp = false;
        if (i < name.length() && name[i] == '#')
        {
            sharp = true;
            ++i;
        }

        const int pc = pitchClassIndex (letter, sharp);
        if (pc < 0)
            return std::nullopt;

        const juce::String octaveStr = name.substring (i);
        if (octaveStr.isEmpty())
            return std::nullopt;
        // Validate the remainder is a signed integer (juce::String::getIntValue() silently
        // returns 0 for garbage, which would wrongly accept e.g. "Cxyz" as octave 0).
        const juce::String digits = octaveStr.startsWith ("-") ? octaveStr.substring (1) : octaveStr;
        if (digits.isEmpty() || ! digits.containsOnly ("0123456789"))
            return std::nullopt;

        const int octave = octaveStr.getIntValue();
        const int note = (octave + 1) * 12 + pc;
        if (note < 0 || note > 127)
            return std::nullopt;
        return note;
    }
}
