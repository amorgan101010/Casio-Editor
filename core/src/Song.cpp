#include "casioxw/Song.h"

#include <algorithm>
#include <utility>

namespace casioxw
{
    std::optional<SongPosition> advanceSongPosition (const Song& song, SongPosition current)
    {
        if (song.rows.empty())
            return std::nullopt;
        if (current.row < 0 || current.row >= (int) song.rows.size())
            return std::nullopt;

        std::vector<int> loopsTaken = std::move (current.loopsTaken);
        loopsTaken.resize (song.rows.size(), 0);   // grow defensively; never shrinks a caller's vector

        const auto& row = song.rows[(size_t) current.row];
        const int repeatsWanted = juce::jmax (1, row.repeatCount);
        if (current.repeat + 1 < repeatsWanted)
            return SongPosition { current.row, current.repeat + 1, std::move (loopsTaken) };

        // This row's own repeats just finished -- an unexhausted loop line jumps back instead of
        // falling through to the next row.
        if (row.loopBackRows > 0)
        {
            const bool infinite = row.loopCount == kInfiniteLoopCount;
            if (infinite || loopsTaken[(size_t) current.row] < row.loopCount)
            {
                if (! infinite)
                    ++loopsTaken[(size_t) current.row];
                const int target = juce::jmax (0, current.row - row.loopBackRows);
                return SongPosition { target, 0, std::move (loopsTaken) };
            }
            // Loop exhausted: reset so a LATER pass (an outer loop line, or the whole-arrangement
            // loop) can take this row's loop line the full count again, then fall through below.
            loopsTaken[(size_t) current.row] = 0;
        }

        const int nextRow = current.row + 1;
        if (nextRow >= (int) song.rows.size())
        {
            if (! song.loopEnabled)
                return std::nullopt;   // last row's last repeat/loop just finished -- song over
            std::fill (loopsTaken.begin(), loopsTaken.end(), 0);   // fresh pass, every loop line resets
            return SongPosition { 0, 0, std::move (loopsTaken) };
        }

        return SongPosition { nextRow, 0, std::move (loopsTaken) };
    }

    juce::String songToJson (const Song& song)
    {
        juce::DynamicObject::Ptr root = new juce::DynamicObject();
        root->setProperty ("format", "casioxw-song");
        root->setProperty ("version", 1);
        root->setProperty ("tempoBpm", song.tempoBpm);
        root->setProperty ("loopEnabled", song.loopEnabled);

        juce::Array<juce::var> rowsArr;
        for (const auto& row : song.rows)
        {
            juce::DynamicObject::Ptr o = new juce::DynamicObject();
            o->setProperty ("setFile", row.setFile);
            o->setProperty ("soloFile", row.soloFile);
            o->setProperty ("drumsFile", row.drumsFile);
            o->setProperty ("pcmFile", row.pcmFile);
            o->setProperty ("repeatCount", row.repeatCount);
            o->setProperty ("label", row.label);
            o->setProperty ("loopBackRows", row.loopBackRows);
            o->setProperty ("loopCount", row.loopCount);

            juce::Array<juce::var> muteArr;
            for (const bool m : row.laneMuted)
                muteArr.add (juce::var (m));
            o->setProperty ("laneMuted", muteArr);

            rowsArr.add (juce::var (o.get()));
        }
        root->setProperty ("rows", rowsArr);

        return juce::JSON::toString (juce::var (root.get()));
    }

    std::optional<Song> songFromJson (const juce::String& text)
    {
        const auto parsed = juce::JSON::parse (text);
        if (parsed.getDynamicObject() == nullptr)
            return std::nullopt;

        Song song;
        song.tempoBpm = (int) parsed.getProperty ("tempoBpm", 120);
        song.loopEnabled = (bool) parsed.getProperty ("loopEnabled", false);

        if (const auto* rarr = parsed.getProperty ("rows", {}).getArray())
        {
            for (const auto& v : *rarr)
            {
                SongRow row;
                row.setFile     = v.getProperty ("setFile", "").toString();
                row.soloFile    = v.getProperty ("soloFile", "").toString();
                row.drumsFile   = v.getProperty ("drumsFile", "").toString();
                row.pcmFile     = v.getProperty ("pcmFile", "").toString();
                row.repeatCount = juce::jmax (1, (int) v.getProperty ("repeatCount", 1));
                row.label       = v.getProperty ("label", "").toString();
                row.loopBackRows = juce::jmax (0, (int) v.getProperty ("loopBackRows", 0));
                row.loopCount    = (int) v.getProperty ("loopCount", 1);
                if (row.loopCount != kInfiniteLoopCount)
                    row.loopCount = juce::jmax (1, row.loopCount);

                if (const auto* marr = v.getProperty ("laneMuted", {}).getArray())
                {
                    const int n = juce::jmin (kSongLaneCount, marr->size());
                    for (int i = 0; i < n; ++i)
                        row.laneMuted[(size_t) i] = (bool) marr->getReference (i);
                }

                song.rows.push_back (std::move (row));
            }
        }

        return song;
    }
}
