#include <catch2/catch_test_macros.hpp>

#include "casioxw/ParamModel.h"

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
