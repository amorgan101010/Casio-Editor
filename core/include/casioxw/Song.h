#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <optional>
#include <vector>

namespace casioxw
{
    /** Fixed track-lane roster the arranger mutes independently, mirroring SequencerPanel's fixed
        lanes (kDrumTracks/kPcmTracks + the one Solo Synth lane in SequencerPanel.cpp) rather than
        anything defined per saved file -- every drums/pcm file always carries all 5/4 of its
        lanes, so a row's mute state is meaningful regardless of which specific file is loaded into
        that slot. Index order: 0 = the Solo Synth lane, 1..5 = Drum 1..5, 6..9 = Bass/Solo 1/Solo
        2/Chords. */
    constexpr int kSongLaneCount = 10;
    constexpr int kSongSynthLane = 0;
    constexpr int kSongDrumLaneStart = 1;   // + 0..4
    constexpr int kSongPcmLaneStart = 6;    // + 0..3

    /** One row ("line") of a Song arrangement, Digitakt-style: which saved sequence(s) play on
        this row, which of the 10 fixed lanes are muted, and how many times the row's 16 steps
        repeat before the song advances to the next row.

        A row is in one of two mutually-exclusive modes:
          - `setFile` non-empty: the row loads a sequence SET (.xwset, SequencerPanel::SaveKind::
            sequenceSet) -- solo+drums+pcm all come from that one bundle and `soloFile`/`drumsFile`/
            `pcmFile` below are ignored (a set "can't load anything else", per the arranger brief).
          - `setFile` empty: `soloFile`/`drumsFile`/`pcmFile` are each independently optional --
            any combination of a solo (.xwseq), drums (.xwdrm), and pcm (.xwpcm) file, or none.

        Every file reference is a relative filename (matching the existing .xwset convention, see
        SequencerPanel::saveByKind), resolved against the Song's own directory so a Song stays
        portable alongside the sequences it references. */
    struct SongRow
    {
        juce::String setFile;
        juce::String soloFile;
        juce::String drumsFile;
        juce::String pcmFile;

        std::array<bool, kSongLaneCount> laneMuted {};   // false = audible
        int repeatCount = 1;   // 1..99 -- how many times this row plays before the song advances
        juce::String label;    // optional user-facing name, e.g. "Intro"
    };

    /** A full arrangement: an ordered list of rows plus one arrangement-wide tempo. The song's
        tempo overrides whatever tempoBpm is embedded in each row's referenced sequence file(s) --
        deliberate, so mixing files saved at different tempos doesn't drift the song mid-playback. */
    struct Song
    {
        std::vector<SongRow> rows;
        int tempoBpm = 120;
    };

    /** Where arranger playback currently is: which row, and which repeat-of-that-row (0-based,
        always < rows[row].repeatCount). */
    struct SongPosition
    {
        int row = 0;
        int repeat = 0;
    };

    /** Pure row-advance step, called once a row's 16 steps have played through one full time.
        Returns the position to play next (same row/repeat+1 if the row has repeats left, else row
        +1/repeat 0), or std::nullopt once the last row's last repeat has just finished -- the song
        has ended. No I/O, no real time, so the playback engine's row-boundary logic is
        Catch2-testable headless, same as Sequence.h's pure functions. */
    std::optional<SongPosition> advanceSongPosition (const Song& song, SongPosition current);

    /** Serialize a Song to a JSON string: every row's file references, lane mutes, repeat count,
        and label, plus the song's tempo. Round-trips with songFromJson(). */
    juce::String songToJson (const Song& song);

    /** Parse a Song from a songToJson() string. std::nullopt if the text isn't a valid Song
        object; a missing/malformed row field falls back to its default rather than rejecting the
        whole file. */
    std::optional<Song> songFromJson (const juce::String& text);
}
