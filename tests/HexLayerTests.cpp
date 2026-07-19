#include <catch2/catch_test_macros.hpp>

#include "casioxw/ParamModel.h"
#include "casioxw/SysExCodec.h"

// Hex Layer (category 0x08, "Hex Layer Parameter" in the manual, XW-P1 only) -- hand-authored
// from XWP1_midi_EN.pdf section 26 (p72-73), NOT mined from franky's Lua: 020_ToneHexLayer.lua
// exists but drives everything live via NRPN (sendHEXParam), never SysEx, and only for a small
// per-layer-level + all-layer mixer subset -- not this section's full per-layer offset/LFO set.
// So there is no tone-edit-buffer source for this domain, same situation pcmMelody/drawbarOrgan
// were in. NOT yet hardware-verified for address/protocol correctness -- these frames are
// hand-computed straight from the manual's byte tables + the project's already-verified
// V2SX-style encoders (nf/cf/cF, no codec changes needed -- see core/src/SysExCodec.cpp),
// standing in for the Lua-derived golden vectors GoldenTests.cpp uses for soloSynth. [bug-198,
// 2026-07-19] The 8-bit offset params were briefly (and incorrectly) given a new single-byte
// 'cf256' vt -- owner hardware testing caught it (every affected param synced back as -128
// regardless of the real value) -- fixed by reusing the existing 2-byte 'cF' vt instead.
// [2026-07-19] hexPitchLock (id 0x14) was hand-authored, shipped as a Global/1-instance param,
// REMOVED after owner hardware testing found no effect and no corresponding synth-menu setting,
// then RE-ADDED same day as a per-layer param (Layer block, ai=2) once the owner found the real
// manual scope: "Pitch Lock (Layers 2, 4, and 6 only)". See gen_xwp1.py's HEXLAYER_PITCH_LOCK_NOTE.

namespace
{
    casioxw::ParamModel loadModel()
    {
        return casioxw::ParamModel::fromFile (juce::File (CASIOXW_PARAMS_JSON));
    }
}

TEST_CASE ("ParamModel: hexLayer section carries all 33 manual-sourced params", "[parammodel][hexLayer]")
{
    const auto model = loadModel();

    for (const char* id : {
             "hexOnoff", "hexPanOffset", "hexPitchKey",
             "hexAmpAttackOfs", "hexAmpDecayOfs", "hexAmpSustainOfs", "hexAmpReleaseOfs",
             "hexVolumeOfs", "hexCutoffOfs", "hexTouchSenseOfs",
             "hexReverbSendOfs", "hexChorusSendOfs",
             "hexKeyRangeLow", "hexKeyRangeHigh", "hexVelRangeLow", "hexVelRangeHigh",
             "hexDetuneNumber", "hexPitchLock",
             "hexPitchLfoWave", "hexPitchLfoRate", "hexPitchAutoDelay", "hexPitchAutoRise",
             "hexPitchAutoDepth", "hexPitchModDepth", "hexPitchAfterDepth",
             "hexAmpLfoWave", "hexAmpLfoRate", "hexAmpAutoDelay", "hexAmpAutoRise",
             "hexAmpAutoDepth", "hexAmpModDepth", "hexAmpAfterDepth" })
    {
        INFO ("id=" << id);
        const auto* p = model.find (id);
        REQUIRE (p != nullptr);
        CHECK (p->section == "hexLayer");
        CHECK (p->ct == 0x08);
    }
}

TEST_CASE ("ParamModel: per-layer params have 6 instances labelled Layer 1..Layer 6", "[parammodel][hexLayer]")
{
    const auto model = loadModel();
    for (const char* id : { "hexOnoff", "hexPanOffset", "hexAmpAttackOfs", "hexKeyRangeHigh" })
    {
        INFO ("id=" << id);
        const auto* p = model.find (id);
        REQUIRE (p != nullptr);
        CHECK (p->block == "Layer");
        REQUIRE (p->instanceCount == 6);
        REQUIRE (p->instanceLabels.size() == 6);
        CHECK (p->instanceLabels[0] == "Layer 1");
        CHECK (p->instanceLabels[5] == "Layer 6");
    }
}

TEST_CASE ("ParamModel: hexDetuneNumber is Hex-Layer-wide (1 instance, Global block)", "[parammodel][hexLayer]")
{
    const auto model = loadModel();

    const auto* detune = model.find ("hexDetuneNumber");
    REQUIRE (detune != nullptr);
    CHECK (detune->block == "Global");
    CHECK (detune->instanceCount == 1);
    CHECK (detune->vt == "nf");
    CHECK (detune->range.min == 0);
    CHECK (detune->range.max == 31);
    CHECK (detune->addr == 0x13);
}

TEST_CASE ("ParamModel: hexPitchLock is a per-layer param (Layer block, 6 instances, ai=0)", "[parammodel][hexLayer]")
{
    // RE-ADDED 2026-07-19 (see gen_xwp1.py's HEXLAYER_PITCH_LOCK_NOTE): unlike its first, removed
    // Global/1-instance incarnation, the manual scopes this to layers 2/4/6 -- the address layout
    // is still per-layer/6-instance like its siblings (app/HexLayerPanel.cpp is what hides the
    // control for layers 1/3/5, not the address model). ai=0 is HARDWARE-CONFIRMED (2026-07-19,
    // midi-probe against a real XW-P1) -- the manual's "Array 03" annotation does NOT map to ai=2
    // via this section's usual Array-1 rule for this one param; ai=2 was tried first and read the
    // wrong value on real hardware.
    const auto model = loadModel();

    const auto* lock = model.find ("hexPitchLock");
    REQUIRE (lock != nullptr);
    CHECK (lock->block == "Layer");
    CHECK (lock->instanceCount == 6);
    CHECK (lock->vt == "nf");
    CHECK (lock->range.min == 0);
    CHECK (lock->range.max == 1);
    CHECK (lock->addr == 0x14);
    CHECK (lock->ai == 0);
}

TEST_CASE ("ParamModel: LFO section is 14 params, all Global block, group LFO", "[parammodel][hexLayer]")
{
    const auto model = loadModel();
    for (const char* id : { "hexPitchLfoWave", "hexPitchLfoRate", "hexPitchAutoDelay",
                             "hexPitchAutoRise", "hexPitchAutoDepth", "hexPitchModDepth",
                             "hexPitchAfterDepth", "hexAmpLfoWave", "hexAmpLfoRate",
                             "hexAmpAutoDelay", "hexAmpAutoRise", "hexAmpAutoDepth",
                             "hexAmpModDepth", "hexAmpAfterDepth" })
    {
        INFO ("id=" << id);
        const auto* p = model.find (id);
        REQUIRE (p != nullptr);
        CHECK (p->block == "Global");
        CHECK (p->instanceCount == 1);
        CHECK (p->group == "LFO");
    }
}

TEST_CASE ("ParamModel: hexPitchLfoWave/hexAmpLfoWave are 7-value combos (no Random)", "[parammodel][hexLayer]")
{
    const auto model = loadModel();
    for (const char* id : { "hexPitchLfoWave", "hexAmpLfoWave" })
    {
        const auto* p = model.find (id);
        REQUIRE (p != nullptr);
        CHECK (p->vt == "nf");
        CHECK (p->range.min == 0);
        CHECK (p->range.max == 6);
        CHECK (p->ui.control == "combo");

        using casioxw::ControlKind;
        CHECK (casioxw::decideControlKind (*p, 1) == ControlKind::ComboEnum);
        CHECK (casioxw::resolveEnumName (*p, 1) == "hexLayerLfoWave");
    }

    const auto* entries = model.enumValues ("hexLayerLfoWave");
    REQUIRE (entries != nullptr);
    REQUIRE (entries->size() == 7);
    CHECK ((*entries)[0].label == "Sine");
    CHECK ((*entries)[6].label == "Pulse 3:1");
}

TEST_CASE ("ParamModel: cF offset params carry -128..127 range", "[parammodel][hexLayer]")
{
    // [bug-198, 2026-07-19] These were 'cf256' (a broken single-byte encoding -- half its range
    // produced a MIDI-illegal byte with the high bit set, since SysEx data bytes must be 0-127).
    // Fixed to reuse the existing 2-byte 'cF' vt, which already encodes this exact -128..+128
    // shape correctly (lo7/hi7 split) and is proven via soloSynth's own golden tests.
    const auto model = loadModel();
    for (const char* id : { "hexAmpAttackOfs", "hexAmpDecayOfs", "hexAmpSustainOfs",
                             "hexAmpReleaseOfs", "hexVolumeOfs", "hexCutoffOfs",
                             "hexTouchSenseOfs", "hexReverbSendOfs", "hexChorusSendOfs",
                             "hexPitchAutoDepth", "hexAmpAutoDepth" })
    {
        INFO ("id=" << id);
        const auto* p = model.find (id);
        REQUIRE (p != nullptr);
        CHECK (p->vt == "cF");
        CHECK (p->range.min == -128);
        CHECK (p->range.max == 127);
        CHECK (p->defaultValue.value_or (-999) == 0);
    }
}

// --- Codec: hand-computed frames (no Lua SysEx source for this domain -- 020_ToneHexLayer.lua
// only ever sends NRPN). Address layout unchanged for cat 0x08 -- SysExCodec is category-agnostic,
// it only reads ParamInfo::ct/addr/ai/an (core/src/SysExCodec.cpp).

TEST_CASE ("SysExCodec: cF offset params encode/decode as 2 MIDI-safe bytes (hexVolumeOfs)", "[sysex][hexLayer]")
{
    const casioxw::SysExCodec codec (loadModel());

    // Volume Offset id=0x0A, value 0 -> w=value+128=128 -> lo7=0x00, hi7=0x01 (2 bytes, both
    // < 0x80, MIDI-safe -- the whole point of using cF instead of the broken single-byte cf256).
    const std::vector<std::uint8_t> expectedZero = {
        0xF0, 0x44, 0x16, 0x03, 0x7F, 0x01,
        0x08, 0x00, 0x00, 0x00,               // ct + fixed
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // blk (instance 1 -> block byte 0)
        0x0A, 0x00,                           // id + fixed
        0x00, 0x00,                           // ai + fixed
        0x00, 0x00,                           // an + fixed
        0x00, 0x01,                           // value (cF: w=128 -> lo7=0x00, hi7=0x01)
        0xF7
    };
    const auto actualZero = codec.encode ("hexVolumeOfs", 1, 0);
    REQUIRE (actualZero == expectedZero);

    // Round-trip the extremes -- every value byte must stay < 0x80 (MIDI-safe).
    for (int v : { -128, -1, 0, 1, 127 })
    {
        const auto frame = codec.encode ("hexVolumeOfs", 1, v);
        REQUIRE (frame.size() == 27);   // 6 header + 18 addr + 2 value bytes + F7
        REQUIRE (frame.back() == 0xF7);
        CHECK (frame[24] < 0x80);
        CHECK (frame[25] < 0x80);

        const int w = v + 128;
        CHECK (frame[24] == (std::uint8_t) (w & 0x7f));
        CHECK (frame[25] == (std::uint8_t) ((w >> 7) & 0xff));

        const auto dec = codec.decode (frame);
        REQUIRE (dec.ok);
        CHECK_FALSE (dec.ambiguous);
        CHECK (dec.paramId == "hexVolumeOfs");
        CHECK (dec.value == v);
    }
}

TEST_CASE ("SysExCodec: hexOnoff's 6 layer instances each address a distinct block byte", "[sysex][hexLayer]")
{
    const casioxw::SysExCodec codec (loadModel());

    for (int instance = 1; instance <= 6; ++instance)
    {
        const auto frame = codec.encode ("hexOnoff", instance, 1);
        REQUIRE (frame.size() == 26);
        CHECK (frame[16] == (std::uint8_t) (instance - 1));   // block byte (addressByteIndex 10 -> wire index 6+10)
        CHECK (frame[18] == 0x00);                            // id byte (Onoff = id 0x00)
        CHECK (frame[24] == 0x01);                            // value byte (nf: 1, no offset)

        const auto dec = codec.decode (frame);
        REQUIRE (dec.ok);
        CHECK (dec.paramId == "hexOnoff");
        CHECK (dec.instance == instance);
        CHECK (dec.value == 1);
    }
}

TEST_CASE ("SysExCodec: hexPanOffset/hexPitchKey (cf) round-trip -64..63", "[sysex][hexLayer]")
{
    const casioxw::SysExCodec codec (loadModel());
    for (const char* id : { "hexPanOffset", "hexPitchKey" })
        for (int v : { -64, 0, 63 })
        {
            const auto frame = codec.encode (id, 1, v);
            CHECK (frame[24] == (std::uint8_t) (v + 64));
            const auto dec = codec.decode (frame);
            REQUIRE (dec.ok);
            CHECK (dec.value == v);
        }
}

TEST_CASE ("SysExCodec: hexPitchLock's 6 layer instances each address a distinct block byte with ai=0", "[sysex][hexLayer]")
{
    // Address layout only, not the layers-2/4/6-only UI restriction (that's app/HexLayerPanel.cpp,
    // not the codec/model). ai=0 is HARDWARE-CONFIRMED (2026-07-19, midi-probe against a real
    // XW-P1 with Pitch Lock audibly on) -- see gen_xwp1.py's HEXLAYER_PITCH_LOCK_NOTE for the
    // probe transcript; the manual's "Array 03" does NOT map to ai=2 for this one param.
    const casioxw::SysExCodec codec (loadModel());

    for (int instance = 1; instance <= 6; ++instance)
    {
        const auto frame = codec.encode ("hexPitchLock", instance, 1);
        REQUIRE (frame.size() == 26);
        CHECK (frame[16] == (std::uint8_t) (instance - 1));   // block byte (addressByteIndex 10 -> wire index 6+10)
        CHECK (frame[18] == 0x14);                            // id byte (Pitch Lock = id 0x14)
        CHECK (frame[20] == 0x00);                            // ai byte (hardware-confirmed 0, not the manual's naive Array-1=2)
        CHECK (frame[24] == 0x01);                            // value byte (nf: 1, no offset)

        const auto dec = codec.decode (frame);
        REQUIRE (dec.ok);
        CHECK (dec.paramId == "hexPitchLock");
        CHECK (dec.instance == instance);
        CHECK (dec.value == 1);
    }
}

TEST_CASE ("SysExCodec: hexDetuneNumber addresses the Global block (byte 0)", "[sysex][hexLayer]")
{
    const casioxw::SysExCodec codec (loadModel());

    const auto detuneFrame = codec.encode ("hexDetuneNumber", 1, 17);
    CHECK (detuneFrame[16] == 0x00);   // block byte: Global is always instance 1 -> 0
    CHECK (detuneFrame[18] == 0x13);   // id
    CHECK (detuneFrame[20] == 0x00);   // ai
    CHECK (detuneFrame[24] == 17);     // nf, no offset

    const auto detuneDec = codec.decode (detuneFrame);
    REQUIRE (detuneDec.ok);
    CHECK (detuneDec.paramId == "hexDetuneNumber");
    CHECK (detuneDec.value == 17);
}
