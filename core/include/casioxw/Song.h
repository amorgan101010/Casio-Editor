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
    /** loopCount sentinel meaning "loop forever" rather than a specific number of times. */
    constexpr int kInfiniteLoopCount = -1;

    struct SongRow
    {
        juce::String setFile;
        juce::String soloFile;
        juce::String drumsFile;
        juce::String pcmFile;

        std::array<bool, kSongLaneCount> laneMuted {};   // false = audible
        int repeatCount = 1;   // 1..99 -- how many times this row plays before the song advances
        juce::String label;    // optional user-facing name, e.g. "Intro"

        /** Elektron-style loop line: once this row's own repeatCount is exhausted, jump BACK
            `loopBackRows` rows (clamped to row 0) instead of falling through to the next row,
            `loopCount` times (or forever if `loopCount == kInfiniteLoopCount`) before finally
            falling through. `loopBackRows == 0` means "no loop line on this row" -- `loopCount` is
            then unused. Lets an intro play once while a verse/chorus range loops behind it. */
        int loopBackRows = 0;
        int loopCount = 1;
    };

    /** A full arrangement: an ordered list of rows plus one arrangement-wide tempo. The song's
        tempo overrides whatever tempoBpm is embedded in each row's referenced sequence file(s) --
        deliberate, so mixing files saved at different tempos doesn't drift the song mid-playback.
        `loopEnabled`: when the last row's last repeat (and any loop lines on it) finish, restart
        the whole arrangement at row 0 instead of ending -- the "Loop Arrangement" baseline ask,
        independent of any per-row loop lines. */
    struct Song
    {
        std::vector<SongRow> rows;
        int tempoBpm = 120;
        bool loopEnabled = false;
    };

    /** Where arranger playback currently is: which row, and which repeat-of-that-row (0-based,
        always < rows[row].repeatCount). `loopsTaken[i]` counts how many times row i's loop line
        has jumped back so far in the CURRENT pass through the song; it resets to 0 for a row the
        moment that row's loop line falls through (exhausted), so a later pass (via an outer loop
        line, or the whole-arrangement loop) can take the full count again. Sized/grown to
        `song.rows.size()` by advanceSongPosition() as needed -- callers can start it empty. */
    struct SongPosition
    {
        int row = 0;
        int repeat = 0;
        std::vector<int> loopsTaken;
    };

    /** Pure row-advance step, called once a row's 16 steps have played through one full time.
        Returns the position to play next: same row/repeat+1 if the row has repeats left; else, if
        the row has an unexhausted loop line, the jump-back target at repeat 0; else the next row at
        repeat 0; else -- past the last row -- row 0 with every loopsTaken reset if `song.loopEnabled`,
        or std::nullopt if not (the song has ended). No I/O, no real time, so the playback engine's
        row-boundary logic is Catch2-testable headless, same as Sequence.h's pure functions. */
    std::optional<SongPosition> advanceSongPosition (const Song& song, SongPosition current);

    /** Serialize a Song to a JSON string: every row's file references, lane mutes, repeat count,
        and label, plus the song's tempo. Round-trips with songFromJson(). */
    juce::String songToJson (const Song& song);

    /** Parse a Song from a songToJson() string. std::nullopt if the text isn't a valid Song
        object; a missing/malformed row field falls back to its default rather than rejecting the
        whole file. */
    std::optional<Song> songFromJson (const juce::String& text);
}
