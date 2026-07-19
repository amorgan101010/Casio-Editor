#include "casioxw/Song.h"

namespace casioxw
{
    std::optional<SongPosition> advanceSongPosition (const Song& song, SongPosition current)
    {
        if (song.rows.empty())
            return std::nullopt;
        if (current.row < 0 || current.row >= (int) song.rows.size())
            return std::nullopt;

        const int repeatsWanted = juce::jmax (1, song.rows[(size_t) current.row].repeatCount);
        if (current.repeat + 1 < repeatsWanted)
            return SongPosition { current.row, current.repeat + 1 };

        const int nextRow = current.row + 1;
        if (nextRow >= (int) song.rows.size())
            return std::nullopt;   // last row's last repeat just finished -- song over

        return SongPosition { nextRow, 0 };
    }

    juce::String songToJson (const Song& song)
    {
        juce::DynamicObject::Ptr root = new juce::DynamicObject();
        root->setProperty ("format", "casioxw-song");
        root->setProperty ("version", 1);
        root->setProperty ("tempoBpm", song.tempoBpm);

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
