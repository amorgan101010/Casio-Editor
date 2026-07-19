#include <catch2/catch_test_macros.hpp>

#include "casioxw/ParamModel.h"
#include "casioxw/SysExCodec.h"

// Drawbar Organ (category 0x07, "Drawbar" in the manual, XW-P1 only) — hand-authored from
// XWP1_midi_EN.pdf section 25 (p71-72), NOT mined from franky's Lua: 022_XWOrgan.lua exists but
// drives everything live via NRPN/CC, never SysEx, so there is no tone-edit-buffer source for
// this domain, same situation pcmMelody was in. NOT yet hardware-verified (owner chose to ship
// flagged rather than probe live hardware first, 2026-07-18) — these frames are hand-computed
// straight from the manual's byte tables + the project's already-verified V2SX encoders (see
// PROTOCOL.md section 4), standing in for the Lua-derived golden vectors GoldenTests.cpp uses
// for soloSynth.

namespace
{
    casioxw::ParamModel loadModel()
    {
        return casioxw::ParamModel::fromFile (juce::File (CASIOXW_PARAMS_JSON));
    }
}

TEST_CASE ("ParamModel: drawbarOrgan section carries all 8 manual-sourced params", "[parammodel][drawbarOrgan]")
{
    const auto model = loadModel();

    for (const char* id : { "organPosition", "organPercussion", "organPercDecayTime",
                             "organKeyonClick", "organKeyoffClick", "organRotaryType",
                             "organVibratoRate", "organVibratoDepth" })
    {
        INFO ("id=" << id);
        const auto* p = model.find (id);
        REQUIRE (p != nullptr);
        CHECK (p->section == "drawbarOrgan");
        CHECK (p->ct == 0x07);
    }
}

TEST_CASE ("ParamModel: organPosition has 9 instances, one per drawbar foot length", "[parammodel][drawbarOrgan]")
{
    const auto model = loadModel();
    const auto* p = model.find ("organPosition");
    REQUIRE (p != nullptr);

    CHECK (p->instanceCount == 9);
    CHECK (p->vt == "nf");
    CHECK (p->range.min == 0);
    CHECK (p->range.max == 8);

    REQUIRE (p->instanceLabels.size() == 9);
    CHECK (p->instanceLabels[0] == "16'");
    CHECK (p->instanceLabels[2] == "8'");
    CHECK (p->instanceLabels[8] == "1'");
}

TEST_CASE ("ParamModel: organPercussion is a 4-value combo (Off/2nd/3rd/2nd+3rd)", "[parammodel][drawbarOrgan]")
{
    const auto model = loadModel();
    const auto* p = model.find ("organPercussion");
    REQUIRE (p != nullptr);

    CHECK (p->vt == "nf");
    CHECK (p->range.min == 0);
    CHECK (p->range.max == 3);
    CHECK (p->ui.control == "combo");

    using casioxw::ControlKind;
    CHECK (casioxw::decideControlKind (*p, 1) == ControlKind::ComboEnum);
    CHECK (casioxw::resolveEnumName (*p, 1) == "organPercussionMode");

    const auto* entries = model.enumValues ("organPercussionMode");
    REQUIRE (entries != nullptr);
    REQUIRE (entries->size() == 4);
    CHECK ((*entries)[0].label == "Off");
    CHECK ((*entries)[3].label == "2nd + 3rd");
}

TEST_CASE ("ParamModel: organKeyonClick/organKeyoffClick are toggles grouped with Percussion", "[parammodel][drawbarOrgan]")
{
    const auto model = loadModel();
    for (const char* id : { "organKeyonClick", "organKeyoffClick" })
    {
        const auto* p = model.find (id);
        REQUIRE (p != nullptr);
        CHECK (p->ui.control == "toggle");
        CHECK (p->range.min == 0);
        CHECK (p->range.max == 1);
        CHECK (casioxw::decideControlKind (*p, 1) == casioxw::ControlKind::Toggle);
        // Owner feedback 2026-07-18: Percussion and Click should be one group, not two.
        CHECK (p->group == "Percussion");
    }
}

TEST_CASE ("ParamModel: organRotaryType is a Sine/Vintage combo grouped with Vibrato", "[parammodel][drawbarOrgan]")
{
    // Owner-verified on hardware 2026-07-18: the manual calls this "Type" (Normal/Vintage) and
    // groups it as general, but it's actually a rotary-speaker/vibrato character switch
    // (Sine/Vintage) -- renamed organType -> organRotaryType and moved to Vibrato accordingly.
    const auto model = loadModel();
    const auto* p = model.find ("organRotaryType");
    REQUIRE (p != nullptr);

    CHECK (p->vt == "nf");
    CHECK (p->range.min == 0);
    CHECK (p->range.max == 1);
    CHECK (p->ui.control == "combo");
    CHECK (p->group == "Vibrato");

    using casioxw::ControlKind;
    CHECK (casioxw::decideControlKind (*p, 1) == ControlKind::ComboEnum);
    CHECK (casioxw::resolveEnumName (*p, 1) == "organRotaryType");

    const auto* entries = model.enumValues ("organRotaryType");
    REQUIRE (entries != nullptr);
    REQUIRE (entries->size() == 2);
    CHECK ((*entries)[0].label == "Sine");
    CHECK ((*entries)[1].label == "Vintage");
}

TEST_CASE ("ParamModel: organVibratoRate/Depth are plain 0..127 faders", "[parammodel][drawbarOrgan]")
{
    const auto model = loadModel();
    for (const char* id : { "organVibratoRate", "organVibratoDepth", "organPercDecayTime" })
    {
        INFO ("id=" << id);
        const auto* p = model.find (id);
        REQUIRE (p != nullptr);
        CHECK (p->vt == "nf");
        CHECK (p->range.min == 0);
        CHECK (p->range.max == 127);
        CHECK (p->instanceCount == 1);
    }
}

// --- Codec: hand-computed frames (no Lua SysEx source for this domain to derive golden vectors
// from — 022_XWOrgan.lua only ever sends NRPN/CC). Address layout unchanged for cat 0x07 —
// SysExCodec is category-agnostic, it only reads ParamInfo::ct/addr/ai/an (core/src/SysExCodec.cpp).

TEST_CASE ("SysExCodec: organVibratoRate (nf) encodes+decodes against a hand-computed frame", "[sysex][drawbarOrgan]")
{
    const casioxw::SysExCodec codec (loadModel());

    // Vibrato Rate id=0x06, value 64 -> wire byte = 64 (nf, no offset).
    const std::vector<std::uint8_t> expected = {
        0xF0, 0x44, 0x16, 0x03, 0x7F, 0x01,
        0x07, 0x00, 0x00, 0x00,               // ct + fixed
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // blk (8 bytes, single instance)
        0x06, 0x00,                           // id + fixed
        0x00, 0x00,                           // ai + fixed
        0x00, 0x00,                           // an + fixed
        0x40,                                 // value (nf: 64 = 0x40)
        0xF7
    };

    const auto actual = codec.encode ("organVibratoRate", 1, 64);
    REQUIRE (actual == expected);

    const auto dec = codec.decode (actual);
    REQUIRE (dec.ok);
    CHECK_FALSE (dec.ambiguous);
    CHECK (dec.paramId == "organVibratoRate");
    CHECK (dec.instance == 1);
    CHECK (dec.value == 64);
}

TEST_CASE ("SysExCodec: organPosition's 9 instances each address a distinct block byte", "[sysex][drawbarOrgan]")
{
    const casioxw::SysExCodec codec (loadModel());

    // Instance N (1-based) -> block byte (N-1) at address offset 10 (byte index 6+10=16 on the
    // wire), same absolute position solo synth's OSC block uses — see the generator's
    // ORGAN_PARAMS comment for why this is believed to hold for Drawbar too (unverified).
    for (int instance = 1; instance <= 9; ++instance)
    {
        const auto frame = codec.encode ("organPosition", instance, 5);
        REQUIRE (frame.size() == 26);
        CHECK (frame[16] == (std::uint8_t) (instance - 1));   // block byte
        CHECK (frame[18] == 0x00);                            // id byte (Position = id 0x00)
        CHECK (frame[24] == 0x05);                            // value byte (nf: 5, no offset)

        const auto dec = codec.decode (frame);
        REQUIRE (dec.ok);
        CHECK (dec.paramId == "organPosition");
        CHECK (dec.instance == instance);
        CHECK (dec.value == 5);
    }
}

TEST_CASE ("SysExCodec: organPosition round-trips its full 0..8 range", "[sysex][drawbarOrgan]")
{
    const casioxw::SysExCodec codec (loadModel());
    for (int v = 0; v <= 8; ++v)
    {
        const auto frame = codec.encode ("organPosition", 1, v);
        const auto dec = codec.decode (frame);
        REQUIRE (dec.ok);
        CHECK (dec.value == v);
    }
}
