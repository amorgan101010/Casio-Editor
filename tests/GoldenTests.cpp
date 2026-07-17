#include <catch2/catch_test_macros.hpp>

#include "casioxw/SysExCodec.h"

#include <juce_core/juce_core.h>

#include <set>
#include <vector>

namespace
{
    std::vector<std::uint8_t> parseHexFrame (const juce::String& hex)
    {
        std::vector<std::uint8_t> bytes;
        for (const auto& tok : juce::StringArray::fromTokens (hex, " ", ""))
            if (tok.isNotEmpty())
                bytes.push_back ((std::uint8_t) tok.getHexValue32());
        return bytes;
    }

    juce::String toHexFrame (const std::vector<std::uint8_t>& bytes)
    {
        juce::StringArray toks;
        for (auto b : bytes)
            toks.add (juce::String::toHexString (&b, 1, 0).paddedLeft ('0', 2));
        return toks.joinIntoString (" ");
    }

    casioxw::ParamModel loadModel()
    {
        return casioxw::ParamModel::fromFile (juce::File (CASIOXW_PARAMS_JSON));
    }

    juce::var loadGolden()
    {
        const juce::File f (CASIOXW_GOLDEN_JSON);
        REQUIRE (f.existsAsFile());
        juce::var root;
        const auto r = juce::JSON::parse (f.loadFileAsString(), root);
        REQUIRE (r.wasOk());
        return root;
    }
}

TEST_CASE ("golden vectors: encode() reproduces every real-Lua frame exactly", "[sysex][golden]")
{
    const casioxw::SysExCodec codec (loadModel());
    const juce::var golden = loadGolden();
    auto* rows = golden.getProperty ("rows", juce::var()).getArray();
    REQUIRE (rows != nullptr);
    REQUIRE (rows->size() > 0);

    std::set<juce::String> vtsSeen;
    std::set<int> wfOscSeen;
    int checked = 0;

    for (const auto& row : *rows)
    {
        const juce::String paramId = row.getProperty ("paramId", juce::var()).toString();
        const int instance         = (int) row.getProperty ("instance", juce::var());
        const int value            = (int) row.getProperty ("value", juce::var());
        const juce::String expHex  = row.getProperty ("expectedFrameHex", juce::var()).toString();

        const auto expected = parseHexFrame (expHex);
        const auto actual   = codec.encode (paramId, instance, value);

        INFO ("paramId=" << paramId << " instance=" << instance << " value=" << value);
        INFO ("expected: " << expHex);
        INFO ("actual:   " << toHexFrame (actual));
        REQUIRE (actual == expected);
        ++checked;

        if (const auto* p = codec.model().find (paramId))
        {
            vtsSeen.insert (p->vt);
            if (p->vt == "wf")
                wfOscSeen.insert (instance);
        }
    }

    // Coverage gates: every solo-synth value-type and all 6 wf oscillators.
    for (const auto* vt : { "nf", "cf", "cF", "pk", "tn", "wf" })
    {
        INFO ("value-type must be covered: " << vt);
        REQUIRE (vtsSeen.count (vt) == 1);
    }
    for (int osc = 1; osc <= 6; ++osc)
    {
        INFO ("wf oscillator must be covered: " << osc);
        REQUIRE (wfOscSeen.count (osc) == 1);
    }

    WARN ("golden encode vectors checked: " << checked
          << " | value-types covered: " << (int) vtsSeen.size()
          << " | wf oscillators covered: " << (int) wfOscSeen.size());
}

TEST_CASE ("round-trip: decode(encode(x)) == x (collisions flagged, not guessed)", "[sysex][golden][roundtrip]")
{
    const casioxw::SysExCodec codec (loadModel());
    const juce::var golden = loadGolden();
    auto* rows = golden.getProperty ("rows", juce::var()).getArray();
    REQUIRE (rows != nullptr);

    int roundTripped = 0;
    int ambiguousSkipped = 0;

    for (const auto& row : *rows)
    {
        const juce::String paramId = row.getProperty ("paramId", juce::var()).toString();
        const int instance         = (int) row.getProperty ("instance", juce::var());
        const int value            = (int) row.getProperty ("value", juce::var());

        const auto frame = codec.encode (paramId, instance, value);
        const auto dec   = codec.decode (frame);

        INFO ("paramId=" << paramId << " instance=" << instance << " value=" << value);
        REQUIRE (dec.ok);

        // Value is always recoverable (the vt is shared even on a collision).
        REQUIRE (dec.value == value);
        REQUIRE (dec.instance == instance);

        if (dec.ambiguous)
        {
            // Address collision (Lua ai typo): the codec must NOT guess which
            // param it was — it returns all candidates. Verify our param is one.
            REQUIRE (dec.candidates.size() > 1);
            bool found = false;
            for (const auto& c : dec.candidates)
                if (c == paramId)
                    found = true;
            REQUIRE (found);
            ++ambiguousSkipped;
        }
        else
        {
            REQUIRE (dec.paramId == paramId);
            ++roundTripped;
        }
    }

    WARN ("clean round-trips: " << roundTripped
          << " | ambiguous (collision) rows flagged: " << ambiguousSkipped);
}
