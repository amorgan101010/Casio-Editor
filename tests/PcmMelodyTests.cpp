#include <catch2/catch_test_macros.hpp>

#include "casioxw/ParamModel.h"
#include "casioxw/SysExCodec.h"

// PCM engine (category 0x05, "Melody" in the manual) — hand-authored from
// XWP1_midi_EN.pdf section 23 (p70), NOT mined from franky's Lua (no Lua
// handler for this domain exists) and NOT yet hardware-verified, unlike the
// soloSynth section GoldenTests.cpp covers. These frames are hand-computed
// straight from the manual's byte tables + the project's already-verified
// V2SX encoders (see PROTOCOL.md section 4), standing in for the Lua-derived
// golden vectors GoldenTests.cpp uses for soloSynth.

namespace
{
    casioxw::ParamModel loadModel()
    {
        return casioxw::ParamModel::fromFile (juce::File (CASIOXW_PARAMS_JSON));
    }
}

TEST_CASE ("ParamModel: pcmMelody section carries all 10 manual-sourced params", "[parammodel][pcmMelody]")
{
    const auto model = loadModel();

    for (const char* id : { "pcmAttackTime", "pcmReleaseTime", "pcmCutoffFreq", "pcmVibratoType",
                             "pcmVibratoDepth", "pcmVibratoSpeed", "pcmVibratoDelay",
                             "pcmOctaveShift", "pcmVolume", "pcmTouchSense" })
    {
        INFO ("id=" << id);
        const auto* p = model.find (id);
        REQUIRE (p != nullptr);
        CHECK (p->section == "pcmMelody");
        // Every Melody sound param is category 0x05 EXCEPT Volume, which is hardware-verified to
        // be the Tone-category (0x03) "Level" -- see the pcmVolume test below and its JSON note.
        CHECK (p->ct == (juce::String (id) == "pcmVolume" ? 0x03 : 0x05));
        CHECK (p->instanceCount == 1);
    }
}

TEST_CASE ("ParamModel: pcmVibratoType is a 4-value combo (Sine/Triangle/Saw/Square)", "[parammodel][pcmMelody]")
{
    const auto model = loadModel();
    const auto* p = model.find ("pcmVibratoType");
    REQUIRE (p != nullptr);

    CHECK (p->vt == "nf");
    CHECK (p->range.min == 0);
    CHECK (p->range.max == 3);
    CHECK (p->ui.control == "combo");

    using casioxw::ControlKind;
    CHECK (casioxw::decideControlKind (*p, 1) == ControlKind::ComboEnum);
    CHECK (casioxw::resolveEnumName (*p, 1) == "melodyVibratoType");

    const auto* entries = model.enumValues ("melodyVibratoType");
    REQUIRE (entries != nullptr);
    REQUIRE (entries->size() == 4);
    CHECK ((*entries)[0].label == "Sine");
    CHECK ((*entries)[1].label == "Triangle");
    CHECK ((*entries)[2].label == "Saw");
    CHECK ((*entries)[3].label == "Square");
}

TEST_CASE ("ParamModel: the six cf-encoded Melody params carry -64..+63", "[parammodel][pcmMelody]")
{
    const auto model = loadModel();
    for (const char* id : { "pcmAttackTime", "pcmReleaseTime", "pcmCutoffFreq",
                             "pcmVibratoDepth", "pcmVibratoSpeed", "pcmVibratoDelay", "pcmTouchSense" })
    {
        INFO ("id=" << id);
        const auto* p = model.find (id);
        REQUIRE (p != nullptr);
        CHECK (p->vt == "cf");
        CHECK (p->range.min == -64);
        CHECK (p->range.max == 63);
    }
}

TEST_CASE ("ParamModel: pcmOctaveShift is cf-encoded but UI-clamped to -2..+2", "[parammodel][pcmMelody]")
{
    const auto model = loadModel();
    const auto* p = model.find ("pcmOctaveShift");
    REQUIRE (p != nullptr);
    CHECK (p->vt == "cf");
    CHECK (p->range.min == -2);
    CHECK (p->range.max == 2);
}

TEST_CASE ("ParamModel: pcmVolume is a plain 0..127 nf fader", "[parammodel][pcmMelody]")
{
    const auto model = loadModel();
    const auto* p = model.find ("pcmVolume");
    REQUIRE (p != nullptr);
    CHECK (p->vt == "nf");
    CHECK (p->range.min == 0);
    CHECK (p->range.max == 127);
    // Hardware-verified 2026-07-18: the PCM tone Volume is the Tone-category "Level"
    // (ct 0x03 / id 0x08), not the Melody-category 0x1F the manual's sec 23 lists.
    CHECK (p->ct == 0x03);
    CHECK (p->addr == 0x08);
}

// --- Codec: hand-computed frames (no Lua source for this domain to derive golden vectors from) ---
//
// Address layout (params/xwp1.json addressLayout, unchanged for cat 0x05 -- SysExCodec is
// category-agnostic, it only reads ParamInfo::ct/addr/ai/an, see core/src/SysExCodec.cpp):
//   ct(1)=05  fixed(3)=00 00 00  blk(8, all 00 -- instanceCount==1)  id(1)  fixed(1)=00
//   ai(1)=00  fixed(1)=00  an(1)=00  fixed(1)=00
// followed by act=0x01 (set) prefix and LSB-first value bytes + F7, same frame shape
// PROTOCOL.md documents for soloSynth.

TEST_CASE ("SysExCodec: pcmAttackTime (cf) encodes+decodes against a hand-computed frame", "[sysex][pcmMelody]")
{
    const casioxw::SysExCodec codec (loadModel());

    // Attack Time id=0x17, value +10 -> wire byte = 10+64 = 0x4A.
    const std::vector<std::uint8_t> expected = {
        0xF0, 0x44, 0x16, 0x03, 0x7F, 0x01,
        0x05, 0x00, 0x00, 0x00,               // ct + fixed
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // blk (8 bytes, single instance)
        0x17, 0x00,                           // id + fixed
        0x00, 0x00,                           // ai + fixed
        0x00, 0x00,                           // an + fixed
        0x4A,                                 // value (cf: 10+64)
        0xF7
    };

    const auto actual = codec.encode ("pcmAttackTime", 1, 10);
    REQUIRE (actual == expected);

    const auto dec = codec.decode (actual);
    REQUIRE (dec.ok);
    CHECK_FALSE (dec.ambiguous);
    CHECK (dec.paramId == "pcmAttackTime");
    CHECK (dec.instance == 1);
    CHECK (dec.value == 10);
}

TEST_CASE ("SysExCodec: pcmVibratoType (nf/combo) encodes the raw enum value", "[sysex][pcmMelody]")
{
    const casioxw::SysExCodec codec (loadModel());

    // Vibrato Type id=0x1A, value 2 (Saw) -> wire byte = 2 (nf, no offset).
    const auto frame = codec.encode ("pcmVibratoType", 1, 2);
    REQUIRE (frame.size() == 26);
    CHECK (frame[18] == 0x1A);   // id byte, offset 12 within the 18-byte address (index 6+12=18)
    CHECK (frame[24] == 0x02);   // value byte

    const auto dec = codec.decode (frame);
    REQUIRE (dec.ok);
    CHECK (dec.paramId == "pcmVibratoType");
    CHECK (dec.value == 2);
}

TEST_CASE ("SysExCodec: pcmVolume (nf) and pcmTouchSense (cf) round-trip at their range extremes", "[sysex][pcmMelody]")
{
    const casioxw::SysExCodec codec (loadModel());

    for (int v : { 0, 127 })
    {
        const auto frame = codec.encode ("pcmVolume", 1, v);
        const auto dec = codec.decode (frame);
        REQUIRE (dec.ok);
        CHECK (dec.value == v);
    }

    for (int v : { -64, 0, 63 })
    {
        const auto frame = codec.encode ("pcmTouchSense", 1, v);
        const auto dec = codec.decode (frame);
        REQUIRE (dec.ok);
        CHECK (dec.value == v);
    }
}

TEST_CASE ("SysExCodec: pcmOctaveShift round-trips its full -2..+2 range", "[sysex][pcmMelody]")
{
    const casioxw::SysExCodec codec (loadModel());
    for (int v = -2; v <= 2; ++v)
    {
        const auto frame = codec.encode ("pcmOctaveShift", 1, v);
        const auto dec = codec.decode (frame);
        REQUIRE (dec.ok);
        CHECK (dec.value == v);
    }
}
