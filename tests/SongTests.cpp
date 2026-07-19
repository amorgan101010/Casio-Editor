#include <catch2/catch_test_macros.hpp>

#include "casioxw/Song.h"

// Song arranger MVP: pure row-advance + JSON round-trip only -- live playback (row-boundary param
// reset, MIDI output) is a hardware-verified app/ boundary, same as Sequence.h's playback pieces.

TEST_CASE ("advanceSongPosition: no rows is immediately over", "[song]")
{
    casioxw::Song song;
    CHECK_FALSE (casioxw::advanceSongPosition (song, { 0, 0 }).has_value());
}

TEST_CASE ("advanceSongPosition: single row, repeatCount 1, advances to end after one play", "[song]")
{
    casioxw::Song song;
    song.rows.push_back ({});
    song.rows[0].repeatCount = 1;

    CHECK_FALSE (casioxw::advanceSongPosition (song, { 0, 0 }).has_value());
}

TEST_CASE ("advanceSongPosition: repeatCount > 1 stays on the same row first", "[song]")
{
    casioxw::Song song;
    song.rows.push_back ({});
    song.rows[0].repeatCount = 3;

    auto pos = casioxw::advanceSongPosition (song, { 0, 0 });
    REQUIRE (pos.has_value());
    CHECK (pos->row == 0);
    CHECK (pos->repeat == 1);

    pos = casioxw::advanceSongPosition (song, *pos);
    REQUIRE (pos.has_value());
    CHECK (pos->row == 0);
    CHECK (pos->repeat == 2);

    CHECK_FALSE (casioxw::advanceSongPosition (song, *pos).has_value());   // 3rd repeat just finished
}

TEST_CASE ("advanceSongPosition: moves to the next row once repeats are exhausted", "[song]")
{
    casioxw::Song song;
    song.rows.push_back ({});
    song.rows.push_back ({});
    song.rows[0].repeatCount = 2;
    song.rows[1].repeatCount = 1;

    auto pos = casioxw::advanceSongPosition (song, { 0, 0 });
    REQUIRE (pos.has_value());
    CHECK (pos->row == 0);
    CHECK (pos->repeat == 1);

    pos = casioxw::advanceSongPosition (song, *pos);
    REQUIRE (pos.has_value());
    CHECK (pos->row == 1);
    CHECK (pos->repeat == 0);

    CHECK_FALSE (casioxw::advanceSongPosition (song, *pos).has_value());   // last row finished
}

TEST_CASE ("advanceSongPosition: repeatCount 0 is treated as 1 (never gets stuck)", "[song]")
{
    casioxw::Song song;
    song.rows.push_back ({});
    song.rows[0].repeatCount = 0;

    CHECK_FALSE (casioxw::advanceSongPosition (song, { 0, 0 }).has_value());
}

TEST_CASE ("advanceSongPosition: out-of-range current row is over", "[song]")
{
    casioxw::Song song;
    song.rows.push_back ({});
    CHECK_FALSE (casioxw::advanceSongPosition (song, { 5, 0 }).has_value());
}

TEST_CASE ("advanceSongPosition: loop line jumps back a finite number of times, then falls through", "[song]")
{
    casioxw::Song song;
    song.rows.push_back ({});   // row 0: verse
    song.rows.push_back ({});   // row 1: chorus, loops back to row 0 twice
    song.rows[1].loopBackRows = 1;
    song.rows[1].loopCount = 2;

    // Pass 1 through row 1.
    auto pos = casioxw::advanceSongPosition (song, { 1, 0 });   // row 1's only repeat just finished
    REQUIRE (pos.has_value());
    CHECK (pos->row == 0);   // jumped back to row 0 (loop 1 of 2)
    CHECK (pos->repeat == 0);

    pos = casioxw::advanceSongPosition (song, *pos);   // row 0 finishes again
    REQUIRE (pos.has_value());
    CHECK (pos->row == 1);   // forward to row 1 again

    pos = casioxw::advanceSongPosition (song, *pos);   // row 1 finishes -- loop 2 of 2
    REQUIRE (pos.has_value());
    CHECK (pos->row == 0);

    pos = casioxw::advanceSongPosition (song, *pos);   // row 0 finishes
    REQUIRE (pos.has_value());
    CHECK (pos->row == 1);

    // Loop exhausted (already taken twice) -- row 1 now falls through past the end of the song.
    CHECK_FALSE (casioxw::advanceSongPosition (song, *pos).has_value());
}

TEST_CASE ("advanceSongPosition: infinite loop line never falls through", "[song]")
{
    casioxw::Song song;
    song.rows.push_back ({});
    song.rows.push_back ({});
    song.rows[1].loopBackRows = 1;
    song.rows[1].loopCount = casioxw::kInfiniteLoopCount;

    casioxw::SongPosition pos { 1, 0, {} };
    for (int i = 0; i < 50; ++i)   // bounded loop, not a real infinite one, to prove it never ends
    {
        const auto next = casioxw::advanceSongPosition (song, pos);
        REQUIRE (next.has_value());
        pos = *next;
    }
    CHECK ((pos.row == 0 || pos.row == 1));   // still oscillating between the two rows
}

TEST_CASE ("advanceSongPosition: loop-back target clamps to row 0, never negative", "[song]")
{
    casioxw::Song song;
    song.rows.push_back ({});
    song.rows[0].loopBackRows = 99;   // absurdly large -- clamp, don't underflow
    song.rows[0].loopCount = 1;

    const auto pos = casioxw::advanceSongPosition (song, { 0, 0 });
    REQUIRE (pos.has_value());
    CHECK (pos->row == 0);
}

TEST_CASE ("advanceSongPosition: loopEnabled restarts at row 0 and resets loop counters", "[song]")
{
    casioxw::Song song;
    song.loopEnabled = true;
    song.rows.push_back ({});
    song.rows.push_back ({});
    song.rows[1].loopBackRows = 1;
    song.rows[1].loopCount = 1;

    // Exhaust row 1's loop line once (uses up its one allowed loop), then let the song "end" and loop.
    auto pos = casioxw::advanceSongPosition (song, { 1, 0 });   // loop 1 of 1 -> back to row 0
    REQUIRE (pos.has_value());
    CHECK (pos->row == 0);
    pos = casioxw::advanceSongPosition (song, *pos);   // row 0 finishes -> forward to row 1
    REQUIRE (pos.has_value());
    CHECK (pos->row == 1);
    pos = casioxw::advanceSongPosition (song, *pos);   // row 1's loop is exhausted, falls through,
    REQUIRE (pos.has_value());                          // past the end -> loopEnabled wraps to row 0
    CHECK (pos->row == 0);
    CHECK (pos->repeat == 0);

    // A fresh pass: row 1's loop line should be able to fire again (counters were reset on wrap).
    pos = casioxw::advanceSongPosition (song, { 1, 0 });
    REQUIRE (pos.has_value());
    CHECK (pos->row == 0);   // loops back again, proving loopsTaken[1] was reset by the wrap
}

TEST_CASE ("advanceSongPosition: loopEnabled false still ends the song normally", "[song]")
{
    casioxw::Song song;
    song.rows.push_back ({});
    CHECK_FALSE (casioxw::advanceSongPosition (song, { 0, 0 }).has_value());
}

TEST_CASE ("songToJson/songFromJson: round-trips loop-line fields and loopEnabled", "[song]")
{
    casioxw::Song song;
    song.loopEnabled = true;

    casioxw::SongRow row;
    row.loopBackRows = 3;
    row.loopCount = casioxw::kInfiniteLoopCount;
    song.rows.push_back (row);

    const auto parsed = casioxw::songFromJson (casioxw::songToJson (song));
    REQUIRE (parsed.has_value());
    CHECK (parsed->loopEnabled == true);
    REQUIRE (parsed->rows.size() == 1);
    CHECK (parsed->rows[0].loopBackRows == 3);
    CHECK (parsed->rows[0].loopCount == casioxw::kInfiniteLoopCount);
}

TEST_CASE ("songToJson/songFromJson: round-trips an independent-slot row", "[song]")
{
    casioxw::Song song;
    song.tempoBpm = 132;

    casioxw::SongRow row;
    row.soloFile = "verse.solo.xwseq";
    row.drumsFile = "verse.drums.xwdrm";
    row.repeatCount = 4;
    row.label = "Verse";
    row.laneMuted[casioxw::kSongPcmLaneStart] = true;   // mute Bass
    song.rows.push_back (row);

    const auto json = casioxw::songToJson (song);
    const auto parsed = casioxw::songFromJson (json);
    REQUIRE (parsed.has_value());
    CHECK (parsed->tempoBpm == 132);
    REQUIRE (parsed->rows.size() == 1);

    const auto& r = parsed->rows[0];
    CHECK (r.setFile.isEmpty());
    CHECK (r.soloFile == "verse.solo.xwseq");
    CHECK (r.drumsFile == "verse.drums.xwdrm");
    CHECK (r.pcmFile.isEmpty());
    CHECK (r.repeatCount == 4);
    CHECK (r.label == "Verse");
    CHECK (r.laneMuted[casioxw::kSongPcmLaneStart] == true);
    CHECK (r.laneMuted[casioxw::kSongSynthLane] == false);
}

TEST_CASE ("songToJson/songFromJson: round-trips a sequence-set row", "[song]")
{
    casioxw::Song song;
    casioxw::SongRow row;
    row.setFile = "chorus.xwset";
    row.repeatCount = 2;
    song.rows.push_back (row);

    const auto parsed = casioxw::songFromJson (casioxw::songToJson (song));
    REQUIRE (parsed.has_value());
    REQUIRE (parsed->rows.size() == 1);
    CHECK (parsed->rows[0].setFile == "chorus.xwset");
    CHECK (parsed->rows[0].soloFile.isEmpty());
}

TEST_CASE ("songFromJson: rejects non-object JSON", "[song]")
{
    CHECK_FALSE (casioxw::songFromJson ("[1,2,3]").has_value());
    CHECK_FALSE (casioxw::songFromJson ("not json").has_value());
}
