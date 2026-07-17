#include <catch2/catch_test_macros.hpp>

#include "casioxw/ParamModel.h"

#include <algorithm>

// Chunk 7a — the additive UI-metadata fields on ParamInfo (name/block/range/default/unit/ui/
// instances.labels/perOsc.enumPerOsc+maxPerOsc) and the top-level "enums" table, plus the pure
// decideControlKind()/resolveEnumName() decision logic ParamControl (app/, GUI, untestable
// headless) is built on. None of this touches the codec-facing fields GoldenTests.cpp/
// MidiIOTests.cpp already exercise.

namespace
{
    casioxw::ParamModel loadModel()
    {
        return casioxw::ParamModel::fromFile (juce::File (CASIOXW_PARAMS_JSON));
    }
}

TEST_CASE ("ParamModel: tssOSCwf carries full per-osc UI metadata", "[parammodel][ui]")
{
    const auto model = loadModel();
    const auto* p = model.find ("tssOSCwf");
    REQUIRE (p != nullptr);

    CHECK (p->name == "OSC Waveform");
    CHECK (p->block == "OSC");
    CHECK (p->instanceLabels.size() == 6);
    CHECK (p->instanceLabels == juce::StringArray { "Synth1", "Synth2", "PCM1", "PCM2", "EXT", "Noise" });

    CHECK (p->ui.control == "combo");
    CHECK (p->ui.enumPerOsc);
    CHECK (p->ui.enumName.isEmpty());   // this param has no single ui.enum — it's per-osc

    // OSC1/2 -> soloSynthWaves, OSC3/4 -> soloPcmWaves, OSC5/6 -> disabled (no entry).
    CHECK (p->enumPerOscByInstance.at (1) == "soloSynthWaves");
    CHECK (p->enumPerOscByInstance.at (2) == "soloSynthWaves");
    CHECK (p->enumPerOscByInstance.at (3) == "soloPcmWaves");
    CHECK (p->enumPerOscByInstance.at (4) == "soloPcmWaves");
    CHECK (p->enumPerOscByInstance.find (5) == p->enumPerOscByInstance.end());
    CHECK (p->enumPerOscByInstance.find (6) == p->enumPerOscByInstance.end());

    CHECK (p->maxPerOsc.at (1) == 310);
    CHECK (p->maxPerOsc.at (3) == 2157);

    using casioxw::ControlKind;
    CHECK (casioxw::decideControlKind (*p, 1) == ControlKind::ComboEnumPerOsc);
    CHECK (casioxw::decideControlKind (*p, 3) == ControlKind::ComboEnumPerOsc);
    CHECK (casioxw::decideControlKind (*p, 5) == ControlKind::Disabled);
    CHECK (casioxw::decideControlKind (*p, 6) == ControlKind::Disabled);

    CHECK (casioxw::resolveEnumName (*p, 1) == "soloSynthWaves");
    CHECK (casioxw::resolveEnumName (*p, 5).isEmpty());
}

TEST_CASE ("ParamModel: tssOSCsw is an on/off toggle (range 0..1)", "[parammodel][ui]")
{
    const auto model = loadModel();
    const auto* p = model.find ("tssOSCsw");
    REQUIRE (p != nullptr);

    CHECK (p->name == "OSC On/Off");
    CHECK (p->block == "OSC");
    CHECK (p->range.min == 0);
    CHECK (p->range.max == 1);
    CHECK (p->ui.control == "toggle");
    CHECK (casioxw::decideControlKind (*p, 1) == casioxw::ControlKind::Toggle);
}

TEST_CASE ("ParamModel: tssFLTFtype is a plain single-enum combo, and the enum resolves", "[parammodel][ui]")
{
    const auto model = loadModel();
    const auto* p = model.find ("tssFLTFtype");
    REQUIRE (p != nullptr);

    CHECK (p->block == "TotalFilter");
    CHECK (p->ui.control == "combo");
    CHECK (p->ui.enumName == "filterType");
    CHECK_FALSE (p->ui.enumPerOsc);

    CHECK (casioxw::decideControlKind (*p, 1) == casioxw::ControlKind::ComboEnum);
    CHECK (casioxw::resolveEnumName (*p, 1) == "filterType");

    const auto* entries = model.enumValues ("filterType");
    REQUIRE (entries != nullptr);
    REQUIRE (entries->size() == 3);
    CHECK ((*entries)[0].value == 0);
    CHECK ((*entries)[0].label == "LPF");
    CHECK ((*entries)[1].label == "BPF");
    CHECK ((*entries)[2].label == "HPF");
}

TEST_CASE ("ParamModel: tssLFOsync is a combo with no backing enum at all (ComboRange)", "[parammodel][ui]")
{
    const auto model = loadModel();
    const auto* p = model.find ("tssLFOsync");
    REQUIRE (p != nullptr);

    CHECK (p->ui.control == "combo");
    CHECK (p->ui.enumName.isEmpty());
    CHECK_FALSE (p->ui.enumPerOsc);
    CHECK (p->range.min == 0);
    CHECK (p->range.max == 2);

    CHECK (casioxw::decideControlKind (*p, 1) == casioxw::ControlKind::ComboRange);
    CHECK (casioxw::resolveEnumName (*p, 1).isEmpty());
}

TEST_CASE ("ParamModel: a slider param decodes as Slider, and a null JSON default is nullopt",
          "[parammodel][ui]")
{
    const auto model = loadModel();
    const auto* p = model.find ("tssOSCPWMpw");   // ui.control=="slider", default: null in JSON
    REQUIRE (p != nullptr);

    CHECK (p->block == "PWM");
    CHECK (p->ui.control == "slider");
    CHECK_FALSE (p->defaultValue.has_value());
    CHECK (casioxw::decideControlKind (*p, 1) == casioxw::ControlKind::Slider);

    const auto* other = model.find ("tssOSCsw");   // default: 0 in JSON
    REQUIRE (other != nullptr);
    REQUIRE (other->defaultValue.has_value());
    CHECK (*other->defaultValue == 0);
}

TEST_CASE ("ParamModel::enumValues returns nullptr for an unknown enum name", "[parammodel][ui]")
{
    const auto model = loadModel();
    CHECK (model.enumValues ("noSuchEnum") == nullptr);
    CHECK (model.enumValues ("") == nullptr);

    const auto* lfoWave = model.enumValues ("lfoWave");
    REQUIRE (lfoWave != nullptr);
    CHECK (lfoWave->size() == 8);
}

// ---- Chunk 7c ------------------------------------------------------------------------------

TEST_CASE ("ParamModel: Clock Trigger params are combos backed by the right enum", "[parammodel][ui][7c]")
{
    const auto model = loadModel();

    for (const auto* id : { "tssOSCPEclk", "tssOSCFEclk", "tssOSCAEclk", "tssFLTFEclk" })
    {
        const auto* p = model.find (id);
        REQUIRE (p != nullptr);
        CHECK (p->ui.control == "combo");
        CHECK (p->ui.enumName == "clockTrigger");
        CHECK (p->range.min == 0);
        CHECK (p->range.max == 18);
    }

    const auto* entries = model.enumValues ("clockTrigger");
    REQUIRE (entries != nullptr);
    REQUIRE (entries->size() == 19);
    CHECK ((*entries)[0].label == "Off");
    CHECK ((*entries)[9].label == "4");
    CHECK ((*entries)[18].label == "4U");

    const auto* lfo = model.find ("tssLFOclk");
    REQUIRE (lfo != nullptr);
    CHECK (lfo->ui.control == "combo");
    CHECK (lfo->ui.enumName == "lfoClockTrigger");
    CHECK (lfo->range.min == 0);
    CHECK (lfo->range.max == 17);

    const auto* lfoEntries = model.enumValues ("lfoClockTrigger");
    REQUIRE (lfoEntries != nullptr);
    REQUIRE (lfoEntries->size() == 18);
    CHECK ((*lfoEntries)[0].label == "1/4");     // no leading "Off" — see gen_xwp1.py note
    CHECK ((*lfoEntries)[17].label == "4U");
}

TEST_CASE ("ParamModel: Key Follow Base params carry unit=\"note\"", "[parammodel][ui][7c]")
{
    const auto model = loadModel();
    for (const auto* id : { "tssOSCPkeyfB", "tssOSCFkeyfB", "tssOSCAkeyfB", "tssFLTFkeyfB" })
    {
        const auto* p = model.find (id);
        REQUIRE (p != nullptr);
        CHECK (p->unit == "note");
        CHECK (p->range.min == 0);
        CHECK (p->range.max == 127);
    }
}

TEST_CASE ("ParamModel: params carry a group, and orderedGroupsForBlock covers every OSC param",
          "[parammodel][ui][7c]")
{
    const auto model = loadModel();

    // Chunk 7e item 1: "Pitch Envelope" was merged into "Pitch" -- an envelope-stage param's
    // group is now indistinguishable, at the group-name level, from any other Pitch param;
    // envelopeStageIds(id).isValid() is the per-param way to tell them apart (see below).
    const auto* pitchEnv = model.find ("tssOSCPENViL");
    REQUIRE (pitchEnv != nullptr);
    CHECK (pitchEnv->group == "Pitch");
    CHECK (casioxw::envelopeStageIds (pitchEnv->id).isValid());

    const auto* pitch = model.find ("tssOSCPdtne");
    REQUIRE (pitch != nullptr);
    CHECK (pitch->group == "Pitch");
    CHECK_FALSE (casioxw::envelopeStageIds (pitch->id).isValid());

    CHECK (model.groupOrder().size() > 0);

    const auto groups = casioxw::orderedGroupsForBlock (model, "soloSynth", "OSC");
    CHECK_FALSE (groups.empty());

    // Every soloSynth/OSC param's group must appear somewhere in the ordered list — none
    // silently dropped, and no duplicate groups.
    juce::StringArray seenGroups;
    for (const auto& g : groups)
    {
        CHECK_FALSE (seenGroups.contains (g));
        seenGroups.add (g);
    }
    for (const auto& p : model.all())
    {
        if (p.section != "soloSynth" || p.block != "OSC")
            continue;
        REQUIRE (p.group.isNotEmpty());
        CHECK (seenGroups.contains (p.group));
    }

    // The OSC block's group merge (Chunk 7e item 1) halved 8 groups down to exactly these 4, in
    // groupOrder's canonical order.
    const std::vector<juce::String> expected { "General", "Pitch", "Filter", "Amp" };
    CHECK (groups == expected);
}

TEST_CASE ("ParamModel: envelopeStageIds derives all 9 siblings from any one stage id",
          "[parammodel][ui][7c]")
{
    using casioxw::envelopeStageIds;

    const auto ids = envelopeStageIds ("tssOSCPENVr1L");
    CHECK (ids.isValid());
    CHECK (ids.initLevel == "tssOSCPENViL");
    CHECK (ids.attackTime == "tssOSCPENVaT");
    CHECK (ids.attackLevel == "tssOSCPENVaL");
    CHECK (ids.decayTime == "tssOSCPENVdT");
    CHECK (ids.sustainLevel == "tssOSCPENVsL");
    CHECK (ids.release1Time == "tssOSCPENVr1T");
    CHECK (ids.release1Level == "tssOSCPENVr1L");
    CHECK (ids.release2Time == "tssOSCPENVr2T");
    CHECK (ids.release2Level == "tssOSCPENVr2L");

    // Every derived id must actually exist in the model, for every one of the 4 envelope groups.
    const auto model = loadModel();
    for (const auto* anyId : { "tssOSCPENViL", "tssOSCFENVaL", "tssOSCAENVdT", "tssFLTFENVr2L" })
    {
        const auto stage = envelopeStageIds (anyId);
        REQUIRE (stage.isValid());
        for (const auto& sid : { stage.initLevel, stage.attackTime, stage.attackLevel,
                                  stage.decayTime, stage.sustainLevel, stage.release1Time,
                                  stage.release1Level, stage.release2Time, stage.release2Level })
            CHECK (model.find (sid) != nullptr);
    }

    CHECK_FALSE (envelopeStageIds ("tssOSCsw").isValid());     // not an envelope param at all
    CHECK_FALSE (envelopeStageIds ("").isValid());
}
