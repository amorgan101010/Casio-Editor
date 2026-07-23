#include "SequencerPanel.h"

#include "ArrangerPanel.h"
#include "EditorLookAndFeel.h"
#include "casioxw/NoteNames.h"
#include "casioxw/Scheduler.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace
{
    // ---- Look-ahead transport tuning -------------------------------------------------------
    // The feeder timer runs on the (jittery) message thread; precise dispatch is JUCE's output
    // thread. The window must exceed the worst feeder stall to avoid an audible gap, but a bigger
    // window also delays how soon a live edit / tempo change takes effect. These are conservative
    // starting points — the owner can tune them on the actual game-loaded machine.
    constexpr int    kSchedulerTickMs = 12;      // feeder fires ~every 12 ms
    constexpr double kLookaheadMs     = 60.0;    // steady-state: schedule this far ahead of now
                                                 // (kept small so live p-lock/base edits apply within ~1 step)
    constexpr double kStartLeadMs     = 50.0;    // put step 0 comfortably in the future so output-thread
                                                 // spin-up can't leave it already past on the first dispatch
    constexpr double kStartPrimeFloorMs = 1500.0;  // ONE-TIME deep prime at play(): floor for the startup horizon.
                                                   // play() primes max(this, one full loop) so the ENTIRE first
                                                   // loop is queued up front with correct timestamps -- the
                                                   // message-thread feeder then does nothing until loop 2, by
                                                   // which point the startup layout/paint spike (which was
                                                   // delaying feeds into a rushed catch-up burst) is long over.
    constexpr double kScheduleSampleRate = 1000.0;   // 1 "sample" == 1 ms, so sample pos == timeMs

    constexpr int kStepWidth = 58;
    constexpr int kStepGridWidth = kStepWidth * 16;       // 16-step columns are the anchor, always leftmost
    constexpr int kLaneLabelWidth = 64;                   // shared label gutter: drum names + Pitch/Gate/Vel
    constexpr int kCardWidth = 470;                       // right-side control cards (drum / synth)
    constexpr int kDrumTrackRowHeight = 52;
    constexpr int kPcmTrackRowHeight = 52;                // same shape as a drum row: label/mute/channel + 16 steps
    constexpr int kPolyVoiceRowHeight = 40;               // a poly sub-track's compact step-strip row (no
                                                           // mute/channel/knobs -- note/gate/vel live in the screen)
    constexpr int kDrumKeyHeight = 34;                    // drum trig keys, tall enough to read as keys
    constexpr int kSelectKeyHeight = kDrumKeyHeight;      // synth select/trig row (match drum/PCM key size)
    // Just the select/trig row now -- note/gate/vel for the solo lane's primary voice no longer
    // live as always-visible per-step knobs here; selecting a step in P-LOCK mode swaps the screen
    // to a NOTE page instead (same mechanism PCM/poly tracks already use).
    constexpr int kStepColumnHeight = kSelectKeyHeight;
    constexpr int kSynthSectionHeight = 306;              // fits the card (header + LCD display + page keys)
    constexpr int kSynthStepTopInset = 9;                 // visually match drum/PCM header-to-keys spacing

    // Every step button (parent row or poly sub-row) renders at the same fixed height
    // (kDrumKeyHeight, 34px), vertically centred in whatever cell it's given -- but parent rows
    // and sub-rows use DIFFERENT cell heights (52px vs 40px), so a plain, flat "2px gap after
    // every row" leaves visibly UNEQUAL whitespace between the button BOXES themselves: a PCM/
    // drum row's own 9px built-in padding ((52-34)/2) plus a 2px gap plus a sub-row's 3px padding
    // ((40-34)/2) reads as ~14px of whitespace, but two sub-rows back to back only had ~8px
    // (3+2+3) -- a visible, uneven pitch the owner flagged live. These two constants instead
    // target a consistent ~14px of visible whitespace between step-button boxes anywhere poly
    // sub-track rows appear (tighter than the ~20px between two ordinary rows -- "closer together
    // is fine, just be consistent", per the owner) by compensating for each row type's own
    // padding: kPolySubRowGap (8 = 14 - 3 - 3) is the gap BETWEEN two sub-rows; a PCM/drum row's
    // OWN padding (9px) already reaches ~14px with the standard 2px inter-row gap, so that
    // transition needs no special constant. The solo lane's trig row has NO padding of its own
    // (its cell height now exactly equals its button height, since the always-visible note/gate/
    // vel knob rows were removed) so kSynthTrigToPolyGap (11 = 14 - 0 - 3) makes up the difference
    // there specifically.
    constexpr int kPolySubRowGap = 8;
    constexpr int kSynthTrigToPolyGap = 11;

    constexpr int kToolbarRowHeight = 30;                 // transport toolbar rows (wrapping flow)
    constexpr int kFooterHeight = 18;                     // file save/load message line
    constexpr int kSectionGap = 6;

    // Solarized Dark tokens (EditorColours). Step-key state colours now live in
    // StepKeyButton::paintButton; these two remain for the Base button + lock markers.
    const juce::Colour kSelectedColour = EditorColours::selected;
    const juce::Colour kIdleColour     = EditorColours::idleStep;

    // Sentinel lockableIndex values for the PCM step editor's raw (non-ParamInfo) cells --
    // negative, so they can never collide with a real index into sequence.lockable (always >= 0).
    // onParamEdited() branches on these instead of indexing sequence.lockable.
    constexpr int kPcmNoteCell = -1;
    constexpr int kPcmGateCell = -2;
    constexpr int kPcmVelCell  = -3;
    // A focused drum lane's base NOTE/VEL raw cells (no GATE -- drums have no gate concept).
    constexpr int kDrumNoteCell = -4;
    constexpr int kDrumVelCell  = -5;

    // displayedMelodicTarget encodings for focus-driven "Base" pages (no step selected anywhere,
    // but a drum/PCM lane is the focused track -- see SequencerPanel::refreshParamDisplayPages()).
    // 200+drumIndex (0-4) and 300+pcmIndex (0-3): both comfortably clear of the highest step-
    // selection encoding in use (100+voice, max 103).
    constexpr int kDrumBaseTargetBase = 200;
    constexpr int kPcmBaseTargetBase  = 300;

    // The p-lockable parameter set, organised into ParamPageDisplay pages (Digitakt-style: one
    // page of 8 cells on screen at a time). Base defaults are musical/neutral — the sequencer
    // sends every base value when playback starts, so anything that would audibly change the
    // patch defaults to "no effect" (open filter, zero depths) rather than 0-means-silence.
    // Adding a lockable param = one row in the active engine's table below; pages, playback, and
    // lock UI all derive from it.
    struct Lockable { juce::String paramId; int instance; int base; juce::String shortName; int page; };

    // One lockable table + page-name set per TrackEngine (see SequencerPanel.h's TrackEngine doc
    // comment for why only one engine is ever active). Drawbar Organ's table is fixed (empty);
    // Solo Synth's depends on which (block, instance) SequencerPanel::currentSoloSynthBlock/
    // currentSoloSynthInstance currently select (buildSoloSynthLockableSet), and Hex Layer's on
    // which of its 6 layers is selected (buildHexLayerLockableSet) -- resolveEngineLockableSet()
    // is the one seam that picks the right source per engine, used by both
    // seedLockableFromEngine() and rebuildSynthParamPages() so they can never disagree about the
    // table's shape.
    struct EngineLockableSet
    {
        juce::String displayName;             // synthLabel text while this engine is selected
        std::vector<juce::String> pageNames;
        std::vector<Lockable> lockables;
    };

    // Solo Synth FULL coverage: every editable param in params/xwp1.json's soloSynth section, at
    // whichever (block, instance) the synth card's block/instance combos currently select
    // (OSC has 6 instances -- Synth1/Synth2/PCM1/PCM2/EXT/Noise; PWM and LFO have 2; Etc and
    // TotalFilter have 1). ONE exception: tssOSCsw (the oscillator's own On/Off switch) is
    // deliberately EXCLUDED -- unlike every other param here, it has no context-free neutral
    // value at all. Every other raw param at worst changes the SOUND until Sync'd (same
    // "recommend a Sync before first play" caveat the pre-existing FLTR/ENV/LFO table already
    // implicitly carried for Cutoff/Rate/envelope shape); tssOSCsw changing "on" vs "off" changes
    // whether an oscillator that a real patch may deliberately have silenced (or relies on)
    // produces sound at ALL -- forcing it either way on every play-start would corrupt a working
    // patch, not just recolour it. See soloSynthNeutralBase()/kSoloSynthNeutralBase for how the
    // ~20 params with no JSON "default" (xwp1.json's default=null) get a hand-judged neutral,
    // same kind of call the original curated table already made for TotalFilter/LFO (kept
    // byte-identical here via kSoloSynthKnownShortNames/kSoloSynthNeutralBase's overrides).
    const std::map<juce::String, int> kSoloSynthNeutralBase = {
        // Time-type envelope stages (Pitch/Filter/Amp): 0 = instant, matches the pre-existing
        // FLTFENV convention exactly.
        { "tssOSCPortaTm", 0 }, { "tssOSCPENVaT", 0 }, { "tssOSCPENVdT", 0 }, { "tssOSCPENVr1T", 0 },
        { "tssOSCPENVr2T", 0 }, { "tssOSCFENVaT", 0 }, { "tssOSCFENVdT", 0 }, { "tssOSCFENVr1T", 0 },
        { "tssOSCFENVr2T", 0 }, { "tssOSCAENVaT", 0 }, { "tssOSCAENVdT", 0 }, { "tssOSCAENVr1T", 0 },
        { "tssOSCAENVr2T", 0 },
        // Level-type envelope stages: 127 = flat-at-max (audible, uncoloured), except r2L which
        // matches the pre-existing FLTFENVr2L=0 "settle low" convention.
        { "tssOSCFENViL", 127 }, { "tssOSCFENVaL", 127 }, { "tssOSCFENVsL", 127 }, { "tssOSCFENVr1L", 127 },
        { "tssOSCFENVr2L", 0 },
        { "tssOSCAENViL", 127 }, { "tssOSCAENVaL", 127 }, { "tssOSCAENVsL", 127 }, { "tssOSCAENVr1L", 127 },
        { "tssOSCAENVr2L", 0 },
        // Clock triggers: 0 = "Off", matching the pre-existing FLTFEclk=0 convention.
        { "tssOSCPEclk", 0 }, { "tssOSCFEclk", 0 }, { "tssOSCAEclk", 0 },
        // Key Follow BASE (a reference note, not an amount -- the KEYF amount param already
        // defaults 0/moot): C4, matches the pre-existing FLTFkeyfB=60 convention.
        { "tssOSCPkeyfB", 60 }, { "tssOSCFkeyfB", 60 }, { "tssOSCAkeyfB", 60 },
        // Audibility-relevant levels/cutoffs: max/open so the oscillator/filter stays audible
        // rather than silently attenuated -- same reasoning as the pre-existing FLTFcoff=127.
        { "tssOSCFcoff", 15 },     // OSC's own per-osc filter, range is 0-15 (not TotalFilter's 0-127)
        { "tssOSCFgain", 0 },      // combo, 0 = Flat (no gain change)
        { "tssOSCAlvl", 127 },     // OSC's overall amp level -- changes the mix balance until Sync'd,
                                   // same risk class as Cutoff, not the tssOSCsw silence/add-sound class
        { "tssOSCPWMpw", 64 },     // 50% duty cycle -- conventional "no pulse coloration" neutral
        // External Input block: params only meaningful with the EXT oscillator/ext triggers in
        // use, which every relevant toggle already defaults off -- inert 0s except a sane pitch
        // reference (Original Key) and a mid input level.
        { "tssOSCXokey", 60 }, { "tssOSCXinlvl", 100 }, { "tssOSCXngth", 0 }, { "tssOSCXngrel", 0 },
        { "tssOSCXPshmode", 0 }, { "tssOSCXPshmix", 0 },
    };

    int soloSynthNeutralBase (const casioxw::ParamInfo& info)
    {
        const auto it = kSoloSynthNeutralBase.find (info.id);
        if (it != kSoloSynthNeutralBase.end())
            return it->second;
        // Every other vt (cf/cF/pk/tn, plus toggle-style nf switches) already has a real JSON
        // default that IS neutral (0 = centered/off, never silences or adds anything).
        return info.defaultValue.value_or (0);
    }

    // Envelope-stage abbreviation, reused verbatim from the pre-existing FLTFENV table's own
    // naming -- keyed by the 9 canonical stage suffixes envelopeStageIds() recognises, so Pitch/
    // Filter/Amp envelopes (and TotalFilter's) all read the same way.
    const std::map<juce::String, juce::String> kEnvelopeStageAbbrev = {
        { "iL", "I.LV" }, { "aT", "ATK" }, { "aL", "A.LV" }, { "dT", "DEC" }, { "sL", "SUS" },
        { "r1T", "R1.T" }, { "r1L", "R1.L" }, { "r2T", "R2.T" }, { "r2L", "R2.L" },
    };

    // Continuity for the params the pre-existing curated table already named well (TotalFilter/
    // LFO) -- everything else derives an abbreviation automatically (soloSynthShortName()) rather
    // than hand-picking ~70 more; not correctness-critical the way base values are.
    const std::map<juce::String, juce::String> kSoloSynthKnownShortNames = {
        { "tssFLTFcoff", "CUT" }, { "tssFLTFreso", "RES" }, { "tssFLTFtype", "TYP" },
        { "tssFLTFEdep", "EDEP" }, { "tssFLTFtch", "TCH" }, { "tssFLTFkeyf", "KEYF" },
        { "tssFLTFlfo1D", "LFO1" }, { "tssFLTFlfo2D", "LFO2" }, { "tssFLTFkeyfB", "KF.B" },
        { "tssFLTFEclk", "ECLK" }, { "tssFLTFErtrg", "RTRG" },
        { "tssLFOwf", "WAVE" }, { "tssLFOrate", "RATE" }, { "tssLFOdep", "DEP" },
        { "tssLFOdelay", "DLY" }, { "tssLFOrise", "RISE" }, { "tssLFOmdep", "MDEP" },
        { "tssLFOsync", "SYNC" }, { "tssLFOclk", "CLK" },
    };

    juce::String soloSynthShortName (const casioxw::ParamInfo& info)
    {
        const auto known = kSoloSynthKnownShortNames.find (info.id);
        if (known != kSoloSynthKnownShortNames.end())
            return known->second;

        const auto stages = casioxw::envelopeStageIds (info.id);
        if (stages.isValid())
        {
            const std::pair<juce::String, juce::String> fields[] = {
                { stages.initLevel, "iL" }, { stages.attackTime, "aT" }, { stages.attackLevel, "aL" },
                { stages.decayTime, "dT" }, { stages.sustainLevel, "sL" },
                { stages.release1Time, "r1T" }, { stages.release1Level, "r1L" },
                { stages.release2Time, "r2T" }, { stages.release2Level, "r2L" },
            };
            for (const auto& f : fields)
                if (f.first == info.id)
                    return kEnvelopeStageAbbrev.at (f.second);
        }

        // Generic fallback: initials of each word in the human name ("Amp Level" -> "AL",
        // "Filter Env Attack Time" -> "FEAT") -- recognisable enough within a page whose header
        // already names the block/group.
        juce::StringArray words;
        words.addTokens (info.name, " ", "");
        juce::String abbrev;
        for (const auto& w : words)
            if (w.isNotEmpty())
                abbrev += w.substring (0, 1).toUpperCase();
        if (abbrev.length() < 2)
            abbrev = info.name.substring (0, 4).toUpperCase();
        return abbrev;
    }

    EngineLockableSet buildSoloSynthLockableSet (const casioxw::ParamModel& model, const juce::String& block, int instance)
    {
        const auto groups = casioxw::orderedGroupsForBlock (model, "soloSynth", block);

        std::vector<juce::String> pageNames;
        std::vector<Lockable> lockables;
        for (const auto& group : groups)
        {
            int cellsOnPage = 8;      // forces a new page on the first param
            int pageInGroup = 0;
            for (const auto& info : model.all())
            {
                if (info.section != "soloSynth" || info.block != block || info.group != group)
                    continue;
                if (info.id == "tssOSCsw")   // no context-free neutral -- see the table doc comment
                    continue;

                if (cellsOnPage == 8)
                {
                    ++pageInGroup;
                    const juce::String base4 = group.substring (0, 4).toUpperCase();
                    pageNames.push_back (pageInGroup == 1 ? base4 : base4.substring (0, 3) + juce::String (pageInGroup));
                    cellsOnPage = 0;
                }

                lockables.push_back ({ info.id, instance, soloSynthNeutralBase (info), soloSynthShortName (info),
                                        (int) pageNames.size() - 1 });
                ++cellsOnPage;
            }
        }
        return { "SOLO SYNTH", pageNames, lockables };
    }

    // Hex Layer (category 0x08) — FULL 6-layer coverage (owner's explicit call, superseding the
    // earlier Layer-1-only v1): every one of the 33 logical hexLayer params from params/xwp1.json,
    // at whichever layer SequencerPanel::currentHexLayer currently selects (per-layer params only
    // -- the Global/LFO params are hex-layer-wide already and appear regardless of layer). Built
    // from codec.model() rather than hand-transcribed (33 logical params reused across 6 layers,
    // not 33*6 -- hand-curation stays tractable; see buildHexLayerLockableSet() below), unlike
    // Solo Synth's still-hand-authored table above.
    //
    // shortName is the one thing the model can't give us (no "3-4 char LCD label" field in the
    // JSON) -- kHexLayerShortNames hand-picks one per logical param (not per layer, so still only
    // 33 judgment calls). kHexLayerPageOf assigns each param to one of 5 pages; the actual
    // in-page CELL ORDER falls out of model.all()'s own JSON iteration order for free (verified:
    // it already matches the pre-existing AMP/FILT/PITCH/LFO ordering below, plus a new LFO2 page
    // for the wave-type/delay/rise params the old curated v1 excluded).
    //
    // Every param here is an *offset* applied to the layer's own tone-editor setting (JSON
    // default=0 for all but the two LFO rates), so base=0 is already the correct "no effect"
    // neutral for the whole table -- no hand-derived musical default needed the way Solo Synth's
    // raw params require (see resolveSoloSynthBase() note once that expansion lands).
    //
    // TRUST NOTE: none of hexLayer's params are hardware-verified (see HexLayerPanel.h's own
    // provenance comment) and its Lua live path is NRPN-only, never SysEx -- same shape as the
    // Drawbar Organ precedent where a SysEx write persisted to the edit buffer but did not reach
    // the running voice. A p-lock write lands just before the step's note-on though, so params
    // only consulted at note-on/envelope-stage time (Amp env offsets, Pitch Key, Detune) are the
    // most likely to work even if continuously-modulated ones (Cutoff/Pan/Volume offset, LFO
    // depths) turn out to need a live path like the drawbars did. Hardware-gated; the owner chose
    // to expand to full 6-layer coverage knowing this -- see the sequencer handoff notes for the
    // test procedure before trusting any page the way Solo Synth's are trusted.
    const std::map<juce::String, juce::String> kHexLayerShortNames = {
        { "hexOnoff",         "ON"   }, { "hexVolumeOfs",      "VOL"  }, { "hexPanOffset",     "PAN"  },
        { "hexAmpAttackOfs",  "ATK"  }, { "hexAmpDecayOfs",    "DEC"  }, { "hexAmpSustainOfs", "SUS"  },
        { "hexAmpReleaseOfs", "REL"  },
        { "hexCutoffOfs",     "CUT"  }, { "hexTouchSenseOfs",  "TCH"  }, { "hexReverbSendOfs", "REV"  },
        { "hexChorusSendOfs", "CHO"  }, { "hexKeyRangeLow",    "KR.L" }, { "hexKeyRangeHigh",  "KR.H" },
        { "hexVelRangeLow",   "VR.L" }, { "hexVelRangeHigh",   "VR.H" },
        { "hexPitchKey",      "PTCH" }, { "hexPitchLock",      "PLCK" }, { "hexWaveNumber",    "WAVE" },
        { "hexDetuneNumber",  "DET"  },
        { "hexPitchLfoRate",  "P.RT" }, { "hexPitchLfoWave",   "P.WV" }, { "hexPitchAutoDelay","P.DL" },
        { "hexPitchAutoRise", "P.RS" }, { "hexPitchAutoDepth", "P.AD" }, { "hexPitchModDepth", "P.MD" },
        { "hexPitchAfterDepth","P.AF" },
        { "hexAmpLfoRate",    "A.RT" }, { "hexAmpLfoWave",     "A.WV" }, { "hexAmpAutoDelay",  "A.DL" },
        { "hexAmpAutoRise",   "A.RS" }, { "hexAmpAutoDepth",   "A.AD" }, { "hexAmpModDepth",   "A.MD" },
        { "hexAmpAfterDepth", "A.AF" },
    };
    const std::vector<juce::String> kHexLayerPageNames = { "AMP", "FILT", "PITCH", "LFO", "LFO2" };
    const std::map<juce::String, int> kHexLayerPageOf = {
        { "hexOnoff", 0 }, { "hexVolumeOfs", 0 }, { "hexPanOffset", 0 }, { "hexAmpAttackOfs", 0 },
        { "hexAmpDecayOfs", 0 }, { "hexAmpSustainOfs", 0 }, { "hexAmpReleaseOfs", 0 },
        { "hexCutoffOfs", 1 }, { "hexTouchSenseOfs", 1 }, { "hexReverbSendOfs", 1 }, { "hexChorusSendOfs", 1 },
        { "hexKeyRangeLow", 1 }, { "hexKeyRangeHigh", 1 }, { "hexVelRangeLow", 1 }, { "hexVelRangeHigh", 1 },
        { "hexPitchKey", 2 }, { "hexPitchLock", 2 }, { "hexWaveNumber", 2 }, { "hexDetuneNumber", 2 },
        { "hexPitchLfoRate", 3 }, { "hexPitchAutoDepth", 3 }, { "hexPitchModDepth", 3 }, { "hexPitchAfterDepth", 3 },
        { "hexAmpLfoRate", 3 }, { "hexAmpAutoDepth", 3 }, { "hexAmpModDepth", 3 }, { "hexAmpAfterDepth", 3 },
        { "hexPitchLfoWave", 4 }, { "hexPitchAutoDelay", 4 }, { "hexPitchAutoRise", 4 },
        { "hexAmpLfoWave", 4 }, { "hexAmpAutoDelay", 4 }, { "hexAmpAutoRise", 4 },
    };

    EngineLockableSet buildHexLayerLockableSet (const casioxw::ParamModel& model, int layer)
    {
        std::vector<Lockable> lockables;
        for (const auto& info : model.all())
        {
            if (info.section != "hexLayer")
                continue;

            // Pitch Lock only exists on the even layer of each pair (manual: turning it on for
            // Layer 2/4/6 copies Layer 1/3/5's pitch onto it) -- skip entirely on odd layers,
            // same gate HexLayerPanel::buildLayerGrid() already applies to its own controls.
            if (info.id == "hexPitchLock" && (layer % 2) != 0)
                continue;

            const auto pageIt = kHexLayerPageOf.find (info.id);
            jassert (pageIt != kHexLayerPageOf.end());   // every hexLayer param needs a page slot
            if (pageIt == kHexLayerPageOf.end())
                continue;

            const auto nameIt = kHexLayerShortNames.find (info.id);
            jassert (nameIt != kHexLayerShortNames.end());

            const bool perLayer = (info.block == "Layer");   // else "Global" (Detune Number/LFO)
            lockables.push_back ({ info.id, perLayer ? layer : 1, 0,
                                    nameIt != kHexLayerShortNames.end() ? nameIt->second
                                                                         : info.id.substring (0, 4).toUpperCase(),
                                    pageIt->second });
        }
        return { "HEX LAYER", kHexLayerPageNames, lockables };
    }

    // Drawbar Organ (category 0x07) — deliberately EMPTY for now. Its one obviously-interesting
    // lockable (organPosition, the 9 drawbar faders) does NOT reach the running voice via the
    // codec's SysEx path at all -- OrganPanel.cpp's sendDrawbarNrpn exists precisely because that
    // param needs a fixed-channel NRPN fader write instead (hardware-verified 2026-07-18: SysEx
    // writes persist to the edit buffer but never sound). paramMessages() below only knows the
    // SysEx path, so routing a drawbar through it here would silently "work" (no error, no
    // effect) rather than fail loudly. Needs its own transport branch in paramMessages() before
    // any Organ param is added to this table -- owner's call this pass was Solo Synth + Hex Layer
    // only (Organ/PCM Melody p-locks explicitly deferred, see project memory).
    EngineLockableSet resolveEngineLockableSet (TrackEngine engine, const casioxw::ParamModel& model,
                                                 const juce::String& soloBlock, int soloInstance, int hexLayer)
    {
        switch (engine)
        {
            case TrackEngine::hexLayer:
                return buildHexLayerLockableSet (model, hexLayer);
            case TrackEngine::drawbarOrgan:
                return { "DRAWBAR ORGAN", {}, {} };
            case TrackEngine::soloSynth:
                break;
        }
        return buildSoloSynthLockableSet (model, soloBlock, soloInstance);
    }

    // Representative param for a soloSynth block (any instance -- block structure is
    // instance-invariant), used to read instanceCount/instanceLabels once per block switch.
    // Mirrors SoloSynthPanel.cpp's own firstParamInBlock() (a different .cpp's anonymous
    // namespace, not reusable directly).
    const casioxw::ParamInfo* firstSoloSynthParamInBlock (const casioxw::ParamModel& model, const juce::String& block)
    {
        for (const auto& info : model.all())
            if (info.section == "soloSynth" && info.block == block)
                return &info;
        return nullptr;
    }

    // engineTag()'s strings are the on-disk vocabulary sequenceToJson()/sequenceFromJson() carry
    // in Sequence::engineTag (core stores it opaquely, doesn't interpret it) -- keep in sync with
    // TrackEngine and kEngineSets' order.
    juce::String engineTag (TrackEngine e)
    {
        switch (e)
        {
            case TrackEngine::hexLayer:     return "hexLayer";
            case TrackEngine::drawbarOrgan: return "drawbarOrgan";
            case TrackEngine::soloSynth:    break;
        }
        return "soloSynth";
    }

    // Missing/unrecognised tag (old files, or a hand-edited one) -> soloSynth, the only engine
    // that existed before multi-engine support.
    TrackEngine engineFromTag (const juce::String& tag)
    {
        if (tag == "hexLayer")     return TrackEngine::hexLayer;
        if (tag == "drawbarOrgan") return TrackEngine::drawbarOrgan;
        return TrackEngine::soloSynth;
    }

    struct DrumTrackDef
    {
        const char* label;
        int defaultChannel;
        int defaultNote;
    };

    constexpr DrumTrackDef kDrumTracks[] = {
        { "Drum 1", 8, 36 },
        { "Drum 2", 9, 38 },
        { "Drum 3", 10, 42 },
        { "Drum 4", 11, 46 },
        { "Drum 5", 12, 49 },
    };

    struct PcmTrackDef
    {
        const char* label;
        int defaultChannel;
    };

    // The Step Sequencer's remaining note parts (XWP1_1B_EN.pdf p.E-49): Drum 1-5 are parts 8-12
    // above; Bass/Solo 1/Solo 2/Chords are parts 13-16, mixer channels 13ch-16ch 1:1. Any of these
    // can hold a PCM Melody tone (or any other tone type) in a configured Performance.
    constexpr PcmTrackDef kPcmTracks[] = {
        { "Bass",   13 },
        { "Solo 1", 14 },
        { "Solo 2", 15 },
        { "Chords", 16 },
    };

    // The shared 3-cell NOTE/GATE/VEL page (raw kPcmNoteCell/GateCell/VelCell cells, see above) --
    // used both for PCM/poly voices (their only page) and, prepended to lockablePages, for the
    // solo lane's own primary voice (see refreshParamDisplayPages()).
    ParamPageDisplay::Page buildNoteGateVelPage (const juce::String& name)
    {
        ParamPageDisplay::CellSpec note, gate, vel;
        note.rawMin = 0;   note.rawMax = 127; note.rawFormat = ParamPageDisplay::ValueFormat::Note;
        note.shortName = "NOTE"; note.lockableIndex = kPcmNoteCell;
        gate.rawMin = 1;   gate.rawMax = 100; gate.rawFormat = ParamPageDisplay::ValueFormat::Percent;
        gate.shortName = "GATE"; gate.lockableIndex = kPcmGateCell;
        vel.rawMin  = 1;   vel.rawMax  = 127; vel.rawFormat  = ParamPageDisplay::ValueFormat::Plain;
        vel.shortName  = "VEL";  vel.lockableIndex  = kPcmVelCell;
        return { name, { note, gate, vel } };
    }

    // A focused drum lane's base page: NOTE + VEL only (raw cells, kDrumNoteCell/kDrumVelCell) --
    // no GATE (drums have no gate concept) and no NOTE/GATE/VEL-per-step editor (a drum step is
    // just a trigger on/off; NOTE and VEL here are the lane's whole-pattern base values, already
    // visible in the row's own note/velocity sliders -- this page just mirrors them for the
    // focused-track LCD display, per the owner's "one consistent rule for every track type" call).
    ParamPageDisplay::Page buildDrumBasePage (const juce::String& name)
    {
        ParamPageDisplay::CellSpec note, vel;
        note.rawMin = 0; note.rawMax = 127; note.rawFormat = ParamPageDisplay::ValueFormat::Note;
        note.shortName = "NOTE"; note.lockableIndex = kDrumNoteCell;
        vel.rawMin  = 1; vel.rawMax  = 127; vel.rawFormat  = ParamPageDisplay::ValueFormat::Plain;
        vel.shortName  = "VEL";  vel.lockableIndex  = kDrumVelCell;
        return { name, { note, vel } };
    }

    // The solo lane's gutter label (synthLabel) needs a name as terse as "Drum 1"/"Chords" to
    // render at the SAME un-shrunk font size those labels do -- juce::Label's default minimum-
    // horizontal-scale silently shrinks text that doesn't fit its (narrow, kLaneLabelWidth) box,
    // and "Solo Synth"/"Hex Layer"/"Drawbar Organ" (the resolveEngineLockableSet() displayName
    // strings, all-caps + much longer) don't fit -- which made this ONE label look like it was in
    // a visibly smaller/different font (owner-flagged live). Deliberately a separate short form
    // from EngineLockableSet::displayName (which stays the full name for its other use), not a
    // truncation of it.
    juce::String shortEngineLabel (TrackEngine engine)
    {
        switch (engine)
        {
            case TrackEngine::hexLayer:     return "Hex Layer";
            case TrackEngine::drawbarOrgan: return "Organ";
            case TrackEngine::soloSynth:    break;
        }
        return "Synth";
    }
}

//==============================================================================
/** The Rnd options call-out: edits the panel's RandomizeOptions (and the combo-params flag)
    in place — no apply step, the next Rnd click uses whatever this shows. */
class RandomizeOptionsComponent : public juce::Component
{
public:
    RandomizeOptionsComponent (casioxw::RandomizeOptions& optionsIn, bool& randomizeCombosIn)
        : options (optionsIn), randomizeCombos (randomizeCombosIn)
    {
        using Scale = casioxw::RandomizeOptions::Scale;

        auto initLabel = [this] (juce::Label& l, const char* text)
        {
            l.setText (text, juce::dontSendNotification);
            l.setFont (EditorFonts::header (11.0f));
            l.setColour (juce::Label::textColourId, EditorColours::textMuted);
            addAndMakeVisible (l);
        };
        initLabel (scaleLabel, "SCALE");
        initLabel (rootLabel, "ROOT");
        initLabel (rangeLabel, "NOTES");
        initLabel (trigLabel, "TRIGS");
        initLabel (lockLabel, "LOCKS");

        scaleCombo.addItem ("Minor Pentatonic", 1 + (int) Scale::minorPentatonic);
        scaleCombo.addItem ("Major Pentatonic", 1 + (int) Scale::majorPentatonic);
        scaleCombo.addItem ("Natural Minor",    1 + (int) Scale::naturalMinor);
        scaleCombo.addItem ("Major",            1 + (int) Scale::major);
        scaleCombo.addItem ("Chromatic",        1 + (int) Scale::chromatic);
        scaleCombo.setSelectedId (1 + (int) options.scale, juce::dontSendNotification);
        scaleCombo.onChange = [this] { options.scale = (Scale) (scaleCombo.getSelectedId() - 1); };
        addAndMakeVisible (scaleCombo);

        static const char* const noteNames[] = { "C", "C#", "D", "D#", "E", "F",
                                                 "F#", "G", "G#", "A", "A#", "B" };
        for (int i = 0; i < 12; ++i)
            rootCombo.addItem (noteNames[i], i + 1);
        rootCombo.setSelectedId (options.rootNote + 1, juce::dontSendNotification);
        rootCombo.onChange = [this] { options.rootNote = rootCombo.getSelectedId() - 1; };
        addAndMakeVisible (rootCombo);

        noteRange.setSliderStyle (juce::Slider::TwoValueHorizontal);
        noteRange.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        noteRange.setRange (0.0, 127.0, 1.0);
        noteRange.setMinAndMaxValues ((double) options.noteMin, (double) options.noteMax,
                                      juce::dontSendNotification);
        noteRange.onValueChange = [this]
        {
            options.noteMin = (int) noteRange.getMinValue();
            options.noteMax = (int) noteRange.getMaxValue();
            updateRangeReadout();
        };
        addAndMakeVisible (noteRange);
        rangeReadout.setFont (EditorFonts::mono (11.0f));
        rangeReadout.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (rangeReadout);
        updateRangeReadout();

        auto initPercent = [this] (juce::Slider& s, float& target)
        {
            s.setSliderStyle (juce::Slider::LinearHorizontal);
            s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 46, 18);
            s.setRange (0.0, 100.0, 1.0);
            s.textFromValueFunction = [] (double v) { return juce::String ((int) v) + "%"; };
            s.setValue ((double) (target * 100.0f), juce::dontSendNotification);
            s.updateText();
            float* boundTarget = &target;   // capture the object, not the (soon-dead) reference
            s.onValueChange = [&s, boundTarget] { *boundTarget = (float) (s.getValue() / 100.0); };
            addAndMakeVisible (s);
        };
        initPercent (trigDensity, options.trigDensity);
        initPercent (lockDensity, options.lockDensity);

        combosToggle.setButtonText ("Also lock combo/switch params");
        combosToggle.setToggleState (randomizeCombos, juce::dontSendNotification);
        combosToggle.onClick = [this] { randomizeCombos = combosToggle.getToggleState(); };
        addAndMakeVisible (combosToggle);

        setSize (300, 6 * kRow + 10);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (8, 5);
        auto layoutRow = [&b] (juce::Label& l, juce::Component& c)
        {
            auto r = b.removeFromTop (kRow).reduced (0, 3);
            l.setBounds (r.removeFromLeft (52));
            c.setBounds (r);
        };
        layoutRow (scaleLabel, scaleCombo);
        layoutRow (rootLabel, rootCombo);
        {
            auto r = b.removeFromTop (kRow).reduced (0, 3);
            rangeLabel.setBounds (r.removeFromLeft (52));
            rangeReadout.setBounds (r.removeFromRight (72));
            noteRange.setBounds (r);
        }
        layoutRow (trigLabel, trigDensity);
        layoutRow (lockLabel, lockDensity);
        combosToggle.setBounds (b.removeFromTop (kRow).reduced (0, 3));
    }

private:
    static constexpr int kRow = 30;

    void updateRangeReadout()
    {
        rangeReadout.setText (casioxw::midiNoteName (options.noteMin) + " - "
                                  + casioxw::midiNoteName (options.noteMax),
                              juce::dontSendNotification);
    }

    casioxw::RandomizeOptions& options;
    bool& randomizeCombos;

    juce::Label scaleLabel, rootLabel, rangeLabel, trigLabel, lockLabel, rangeReadout;
    juce::ComboBox scaleCombo, rootCombo;
    juce::Slider noteRange, trigDensity, lockDensity;
    juce::ToggleButton combosToggle;
};

//==============================================================================
void StepKeyButton::setLockState (bool hasLockIn, bool selectedIn)
{
    if (hasLock == hasLockIn && selected == selectedIn)
        return;
    hasLock = hasLockIn;
    selected = selectedIn;
    repaint();
}

void StepKeyButton::setChordState (bool hasChordIn)
{
    if (hasChord == hasChordIn)
        return;
    hasChord = hasChordIn;
    repaint();
}

void StepKeyButton::paintButton (juce::Graphics& g, bool isMouseOver, bool isMouseDown)
{
    auto b = getLocalBounds().toFloat().reduced (1.5f);
    const bool on = getToggleState();

    auto fill = selected ? EditorColours::selected
              : on       ? EditorColours::filledStep
                         : EditorColours::idleStep;
    if (isMouseOver) fill = fill.brighter (0.08f);
    if (isMouseDown) fill = fill.brighter (0.16f);
    g.setColour (fill);
    g.fillRoundedRectangle (b, 4.0f);

    // Quarter-note steps carry a structurally thicker/brighter outline — bar orientation must
    // never depend on fill colour alone (it also encodes trig/lock/selection state).
    const bool quarter = (stepIndex % 4) == 0;
    g.setColour (quarter ? EditorColours::textHeader.withAlpha (0.6f)
                         : EditorColours::border.withAlpha (0.45f));
    g.drawRoundedRectangle (b, 4.0f, quarter ? 2.2f : 1.0f);

    g.setColour ((selected || on) ? EditorColours::base03 : EditorColours::base00);
    g.setFont (EditorFonts::mono (12.0f, true));
    g.drawText (juce::String (stepIndex + 1), getLocalBounds().translated (0, -2),
                juce::Justification::centred);
    if (quarter)
        g.fillRect (juce::Rectangle<float> (b.getCentreX() - 5.0f, b.getCentreY() + 6.0f, 10.0f, 1.8f));

    if (hasLock)
    {
        g.setColour (EditorColours::hasLocks);
        g.fillEllipse (b.getRight() - 8.5f, b.getY() + 3.0f, 5.0f, 5.0f);
    }
    if (hasChord)
    {
        g.setColour (EditorColours::screenAccent);
        g.fillEllipse (b.getRight() - 8.5f, b.getBottom() - 8.0f, 5.0f, 5.0f);
    }
}

//==============================================================================
SequencerPanel::SequencerPanel (casioxw::SysExCodec& codecIn, casioxw::MidiIO& midiIOIn)
    : codec (codecIn), midiIO (midiIOIn)
{
    loadSequenceSettings();

    // ---- seed the source-of-truth sequence -------------------------------------------------
    for (auto& step : sequence.steps)
        step.velocity = 100;

    // Solo Synth block list, built from the model (block/instance selector) BEFORE the initial
    // seedLockableFromEngine() below, since resolveEngineLockableSet() needs currentSoloSynthBlock
    // populated for the default engine (soloSynth).
    for (const auto& info : codec.model().all())
        if (info.section == "soloSynth" && ! soloSynthBlockOrder.contains (info.block))
            soloSynthBlockOrder.add (info.block);
    // Default to TotalFilter (cutoff sweeps are the most common p-lock target, and this was the
    // sole block the pre-expansion table covered) rather than block order's first entry (OSC),
    // so opening the sequencer on Solo Synth doesn't change the landing page from prior behaviour.
    if (soloSynthBlockOrder.contains ("TotalFilter"))
        currentSoloSynthBlock = "TotalFilter";
    else if (! soloSynthBlockOrder.isEmpty())
        currentSoloSynthBlock = soloSynthBlockOrder[0];

    seedLockableFromEngine (currentEngine);   // default TrackEngine::soloSynth

    engineCombo.addItem ("Solo Synth", (int) TrackEngine::soloSynth + 1);
    engineCombo.addItem ("Hex Layer", (int) TrackEngine::hexLayer + 1);
    engineCombo.addItem ("Drawbar Organ", (int) TrackEngine::drawbarOrgan + 1);
    engineCombo.setSelectedId ((int) currentEngine + 1, juce::dontSendNotification);
    engineCombo.onChange = [this]
    {
        switchEngine (static_cast<TrackEngine> (engineCombo.getSelectedId() - 1));
    };
    addAndMakeVisible (engineCombo);

    for (int layer = 1; layer <= 6; ++layer)
        hexLayerCombo.addItem ("Layer " + juce::String (layer), layer);
    hexLayerCombo.setSelectedId (currentHexLayer, juce::dontSendNotification);
    hexLayerCombo.onChange = [this] { setHexLayer (hexLayerCombo.getSelectedId()); };
    addAndMakeVisible (hexLayerCombo);

    for (int i = 0; i < soloSynthBlockOrder.size(); ++i)
        soloSynthBlockCombo.addItem (soloSynthBlockOrder[i], i + 1);
    if (! soloSynthBlockOrder.isEmpty())
        soloSynthBlockCombo.setSelectedId (soloSynthBlockOrder.indexOf (currentSoloSynthBlock) + 1,
                                            juce::dontSendNotification);
    soloSynthBlockCombo.onChange = [this] { setSoloSynthBlock (soloSynthBlockCombo.getText()); };
    addAndMakeVisible (soloSynthBlockCombo);

    // Instance combo's items depend on the current block (rebuilt in setSoloSynthBlock() too);
    // seed it here for the ctor's default block so it isn't empty before the first user action.
    if (const auto* rep = firstSoloSynthParamInBlock (codec.model(), currentSoloSynthBlock))
        for (int i = 0; i < rep->instanceCount; ++i)
            soloSynthInstanceCombo.addItem (i < rep->instanceLabels.size() ? rep->instanceLabels[i]
                                                                            : juce::String (i + 1),
                                             i + 1);
    soloSynthInstanceCombo.setSelectedId (1, juce::dontSendNotification);
    soloSynthInstanceCombo.onChange = [this] { setSoloSynthInstance (soloSynthInstanceCombo.getSelectedId()); };
    addAndMakeVisible (soloSynthInstanceCombo);

    // ---- solo lane poly mode (Hex Layer/Drawbar Organ only -- see switchEngine()) ----------
    for (auto& voice : synthExtraVoices)
        for (auto& step : voice.track.steps)
            step.velocity = 100;   // channel is mirrored from `sequence.channel` on every play/stop tick

    synthPolyToggle.setClickingTogglesState (true);
    synthPolyToggle.onClick = [this]
    {
        synthPolyMode = synthPolyToggle.getToggleState();
        if (! synthPolyMode)
        {
            synthSubTracksExpanded = false;
            clearSynthPolySelection();
        }
        refreshStepButtons();
        updateStatusLabel();
        resized();
        repaint();   // resized() alone leaves the synth card's painted background (synthCardBounds,
                     // drawn in paint()) stale at its old Y when this shifts it -- see the Arranger-mode
                     // switch's identical resized()+repaint() pair (further down) for the same fix.
    };
    addAndMakeVisible (synthPolyToggle);

    synthSubTrackArrow.setClickingTogglesState (true);
    synthSubTrackArrow.setToggleState (false, juce::dontSendNotification);
    synthSubTrackArrow.setButtonText (juce::String::fromUTF8 ("▶"));
    synthSubTrackArrow.onClick = [this]
    {
        synthSubTracksExpanded = synthSubTrackArrow.getToggleState();
        synthSubTrackArrow.setButtonText (synthSubTracksExpanded ? juce::String::fromUTF8 ("▼")
                                                                   : juce::String::fromUTF8 ("▶"));
        resized();
        repaint();   // see synthPolyToggle.onClick above
    };
    addAndMakeVisible (synthSubTrackArrow);

    for (size_t v = 0; v < synthExtraVoices.size(); ++v)
    {
        auto& voice = synthExtraVoices[v];
        for (size_t step = 0; step < voice.steps.size(); ++step)
        {
            auto& b = voice.steps[step];
            b.setStepIndex ((int) step);
            b.setClickingTogglesState (false);
            b.setToggleState (false, juce::dontSendNotification);
            b.onClick = [this, v, step]
            {
                setFocusedTrack (FocusedTrackKind::soloSynth, -1);
                if (editButton.getToggleState())
                {
                    const bool wasThisCell = (synthPolyStep == (int) step && synthSelectedVoice == (int) v + 1);
                    clearPcmSelections();
                    clearDrumSelections();
                    if (! wasThisCell)
                    {
                        synthPolyStep = (int) step;
                        synthSelectedVoice = (int) v + 1;
                        selectedStep = -1;
                    }
                    else
                    {
                        clearSynthPolySelection();
                    }
                    refreshStepButtons();
                    updateStatusLabel();
                    updateClearLocksEnabled();
                }
                else
                {
                    auto& s = synthExtraVoices[v].track.steps[step];
                    s.enabled = ! s.enabled;
                    if (s.enabled)   // sensible starting pitch: unison with the primary voice
                        s.note = sequence.steps[step].note;
                    refreshStepButtons();
                    updateStatusLabel();
                }
            };
            addAndMakeVisible (b);
        }
    }

    // ---- step grid -------------------------------------------------------------------------
    for (int i = 0; i < 16; ++i)
    {
        auto sc = std::make_unique<StepControl>();

        sc->select.setStepIndex (i);
        sc->select.onClick = [this, i]
        {
            setFocusedTrack (FocusedTrackKind::soloSynth, -1);
            if (editButton.getToggleState())
                selectStep (selectedStep == i ? -1 : i);
            else
            {
                auto& step = sequence.steps[(size_t) i];
                step.enabled = ! step.enabled;
                refreshStepButtons();
                updateStatusLabel();
            }
        };
        addAndMakeVisible (sc->select);

        stepControls[(size_t) i] = std::move (sc);
    }

    for (auto* l : { &tempoLabel, &rateLabel, &channelLabel })
    {
        l->setJustificationType (juce::Justification::centredRight);
        l->setColour (juce::Label::textColourId, EditorColours::textMuted);
        l->setFont (EditorFonts::header (11.0f));
        addAndMakeVisible (*l);
    }

    // ---- transport -------------------------------------------------------------------------
    playStopButton.onClick = [this] { playing ? stop() : play(); };
    addAndMakeVisible (playStopButton);

    randomizeButton.onClick = [this] { randomizeSequence(); };
    addAndMakeVisible (randomizeButton);

    rndOptionsButton.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xbe"));   // small down triangle
    rndOptionsButton.setTooltip ("Randomize options (scale, note range, densities)");
    rndOptionsButton.onClick = [this] { showRandomizeOptions(); };
    addAndMakeVisible (rndOptionsButton);

    saveButton.onClick = [this] { saveSequenceToFile(); };
    loadButton.onClick = [this] { loadSequenceFromFile(); };
    sequenceDirButton.onClick = [this] { chooseSequenceDirectory(); };
    addAndMakeVisible (saveButton);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (sequenceDirButton);

    // ---- Arranger sub-view (owner's brief: Digitakt-style song table as a MODE of this tab) ----
    arrangerPanel = std::make_unique<ArrangerPanel> (codec, midiIO);
    arrangerPanel->setSequenceDirectory (sequenceDefaultDirectory);
    // The two transports share one MidiIO output -- only one may schedule at a time. Pressing the
    // arranger's own Play stops the step editor's transport first; the reverse (this panel's own
    // play(), below) stops the arranger.
    arrangerPanel->beforePlay = [this] { if (playing) stop(); };
    addChildComponent (*arrangerPanel);   // hidden until setShowingArranger(true)

    arrangerModeButton.setClickingTogglesState (true);
    arrangerModeButton.setTooltip ("Switch between the step editor and the song arranger");
    arrangerModeButton.onClick = [this] { setShowingArranger (arrangerModeButton.getToggleState()); };
    addAndMakeVisible (arrangerModeButton);

    // Rate / time-scale. Item id == steps-per-beat, so the combo maps straight onto the model.
    rateCombo.addItem ("1/4", 1);
    rateCombo.addItem ("1/8", 2);
    rateCombo.addItem ("1/8T", 3);
    rateCombo.addItem ("1/16", 4);
    rateCombo.addItem ("1/16T", 6);
    rateCombo.addItem ("1/32", 8);
    rateCombo.setSelectedId (4, juce::dontSendNotification);   // 1/16 = current default
    rateCombo.onChange = [this]
    {
        const int spb = rateCombo.getSelectedId();
        if (spb > 0)
            sequence.stepsPerBeat = spb;
    };
    addAndMakeVisible (rateCombo);
    addAndMakeVisible (rateLabel);

    tempoSlider.setRange (40.0, 240.0, 1.0);
    tempoSlider.setValue (120.0, juce::dontSendNotification);
    tempoSlider.onValueChange = [this] { sequence.tempoBpm = (int) tempoSlider.getValue(); };
    addAndMakeVisible (tempoSlider);
    addAndMakeVisible (tempoLabel);

    channelSlider.setRange (1.0, 16.0, 1.0);
    channelSlider.setValue (1.0, juce::dontSendNotification);
    channelSlider.onValueChange = [this] { sequence.channel = (int) channelSlider.getValue(); };
    addAndMakeVisible (channelSlider);
    addAndMakeVisible (channelLabel);

    // ---- mode row --------------------------------------------------------------------------
    baseButton.onClick = [this] { selectStep (-1); };
    addAndMakeVisible (baseButton);

    syncBaseButton.setTooltip ("Adopt the synth's current values as the sequence's base sound");
    syncBaseButton.onClick = [this] { syncBaseValuesFromSynth(); };
    addAndMakeVisible (syncBaseButton);

    // STEP / P-LOCK: a two-key segmented mode switch (radio pair) instead of one button whose
    // caption flips — the active mode is always the lit key, never a caption to parse.
    stepModeButton.setClickingTogglesState (true);
    stepModeButton.setRadioGroupId (0x5EC7);
    stepModeButton.setToggleState (true, juce::dontSendNotification);
    stepModeButton.onClick = [this] { setPLockMode (false); };
    addAndMakeVisible (stepModeButton);

    editButton.setClickingTogglesState (true);
    editButton.setRadioGroupId (0x5EC7);
    editButton.onClick = [this] { setPLockMode (true); };
    addAndMakeVisible (editButton);

    muteSynthButton.setClickingTogglesState (true);
    addAndMakeVisible (muteSynthButton);

    clearLocksButton.onClick = [this]
    {
        if (selectedStep >= 0 || hasAnyDrumStepSelected())
        {
            if (selectedStep >= 0)
                casioxw::clearStepLocks (sequence, selectedStep);
            for (auto& row : drumTrackControls)
                if (row != nullptr)
                    if (row->selectedStep >= 0)
                        row->velocityLocks[(size_t) row->selectedStep].reset();
            refreshParamControls();
            refreshStepButtons();
            updateClearLocksEnabled();
        }
    };
    addAndMakeVisible (clearLocksButton);

    clearAllButton.onClick = [this] { clearAllSteps(); };
    addAndMakeVisible (clearAllButton);

    shiftLeftButton.onClick  = [this] { shiftFocusedTrack (-1); };
    shiftRightButton.onClick = [this] { shiftFocusedTrack (1); };
    addAndMakeVisible (shiftLeftButton);
    addAndMakeVisible (shiftRightButton);

    // Clicking the solo lane's own label focuses it -- the drum/PCM equivalent of each row's
    // trackLabel. synthLabel already tracks the current engine's display name ("SOLO SYNTH"/
    // "HEX LAYER"/"DRAWBAR ORGAN", set in applyEngine()) and is repositioned in resized() to sit
    // in the same lane-label-gutter column drum/PCM rows use, instead of the top header. mouseDown()
    // below routes on eventComponent identity.
    synthLabel.addMouseListener (this, false);

    statusLabel.setColour (juce::Label::textColourId, EditorColours::textMuted);
    statusLabel.setFont (EditorFonts::mono (11.0f));
    addAndMakeVisible (statusLabel);

    // ---- drum-track controls (5 lanes, each with channel + note + 16 step on/off + velocity) ---
    for (auto* l : { &drumTracksLabel, &pcmTracksLabel })
    {
        l->setColour (juce::Label::textColourId, EditorColours::textHeader);
        l->setJustificationType (juce::Justification::centredLeft);
        l->setFont (EditorFonts::header (12.0f));
        addAndMakeVisible (*l);
    }

    // synthLabel is styled like a DrumTrackControl/PcmTrackControl row's trackLabel (textMuted,
    // smaller) now that it lives in that same lane-label-gutter column, not like the "DRUM TRACKS"/
    // "PCM TRACKS" section-title styling above -- see its header doc comment.
    synthLabel.setColour (juce::Label::textColourId, EditorColours::textMuted);
    synthLabel.setJustificationType (juce::Justification::centredLeft);
    synthLabel.setFont (EditorFonts::header (11.0f));
    addAndMakeVisible (synthLabel);

    const int noteMin = 0;
    const int noteMax = 127;

    for (size_t i = 0; i < std::size (kDrumTracks); ++i)
    {
        const auto& def = kDrumTracks[i];
        auto row = std::make_unique<DrumTrackControl>();
        auto* rowPtr = row.get();

        row->trackLabel.setText (def.label, juce::dontSendNotification);
        row->trackLabel.setJustificationType (juce::Justification::centredLeft);
        row->trackLabel.setColour (juce::Label::textColourId, EditorColours::textMuted);
        row->trackLabel.setFont (EditorFonts::header (11.0f));
        row->trackLabel.addMouseListener (this, false);   // click focuses this lane -- mouseDown() below
        addAndMakeVisible (row->trackLabel);

        row->mute.setClickingTogglesState (true);
        addAndMakeVisible (row->mute);

        for (int ch = 1; ch <= 16; ++ch)
            row->channel.addItem (juce::String (ch), ch);
        row->channel.setSelectedId (def.defaultChannel, juce::dontSendNotification);
        addAndMakeVisible (row->channel);

        row->note.setRange ((double) noteMin, (double) noteMax, 1.0);
        row->note.setValue ((double) juce::jlimit (noteMin, noteMax, def.defaultNote), juce::dontSendNotification);
        row->note.textFromValueFunction = [] (double v) { return casioxw::midiNoteName ((int) v); };
        row->note.valueFromTextFunction = [] (const juce::String& t) -> double
        {
            const auto n = casioxw::noteNameToMidi (t);
            return n.has_value() ? (double) *n : 0.0;
        };
        row->note.updateText();
        addAndMakeVisible (row->note);

        row->velocity.setRange (1.0, 127.0, 1.0);
        row->velocity.setValue (100.0, juce::dontSendNotification);
        row->velocity.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 40, 20);
        row->velocity.textFromValueFunction = [] (double v) { return juce::String ((int) v); };
        row->velocity.onValueChange = [this, rowPtr]
        {
            const int v = juce::jlimit (1, 127, (int) rowPtr->velocity.getValue());
            if (editButton.getToggleState() && rowPtr->selectedStep >= 0)
                rowPtr->velocityLocks[(size_t) rowPtr->selectedStep] = v;
            else
                rowPtr->baseVelocity = v;

            refreshStepButtons();
        };
        row->velocityMarker.setJustificationType (juce::Justification::centredLeft);
        row->velocityMarker.setColour (juce::Label::textColourId, EditorColours::textMuted);
        addAndMakeVisible (row->velocity);
        addAndMakeVisible (row->velocityMarker);

        for (size_t step = 0; step < row->steps.size(); ++step)
        {
            auto& b = row->steps[step];
            b.setStepIndex ((int) step);
            b.setClickingTogglesState (false);
            b.setToggleState (false, juce::dontSendNotification);
            b.onClick = [this, rowPtr, i, step]
            {
                setFocusedTrack (FocusedTrackKind::drum, (int) i);
                if (editButton.getToggleState())
                {
                    rowPtr->selectedStep = (rowPtr->selectedStep == (int) step ? -1 : (int) step);
                    if (rowPtr->selectedStep >= 0)
                        selectedStep = -1; // synth and drum step edit targets are mutually exclusive
                    refreshStepButtons();
                    updateStatusLabel();
                    updateClearLocksEnabled();
                }
                else
                {
                    const bool next = ! rowPtr->steps[step].getToggleState();
                    rowPtr->steps[step].setToggleState (next, juce::dontSendNotification);
                    refreshStepButtons();
                    updateStatusLabel();
                }
            };
            addAndMakeVisible (b);
        }

        drumTrackControls[i] = std::move (row);
    }

    // ---- PCM-track controls (4 melodic lanes: Bass/Solo 1/Solo 2/Chords, XWP1_1B_EN.pdf p.E-49).
    // Each is its own lane (label/mute/channel/16 step keys), not a shared column with the Solo
    // Synth. In P-LOCK mode, selecting a step swaps the screen to a NOTE/GATE/VEL editor for it
    // (refreshParamDisplayPages()); in STEP mode, clicking a step just toggles it on/off.
    for (size_t i = 0; i < std::size (kPcmTracks); ++i)
    {
        const auto& def = kPcmTracks[i];
        auto row = std::make_unique<PcmTrackControl>();
        auto* rowPtr = row.get();

        for (auto& step : row->track.steps)
            step.velocity = 100;
        row->track.channel = def.defaultChannel;
        for (auto& voice : row->extraVoices)
        {
            voice.track.channel = def.defaultChannel;   // extra voices are simultaneous notes on
            for (auto& step : voice.track.steps)         // the SAME part/channel, not independent lanes
                step.velocity = 100;
        }

        row->trackLabel.setText (def.label, juce::dontSendNotification);
        row->trackLabel.setJustificationType (juce::Justification::centredLeft);
        row->trackLabel.setColour (juce::Label::textColourId, EditorColours::textMuted);
        row->trackLabel.setFont (EditorFonts::header (11.0f));
        row->trackLabel.addMouseListener (this, false);   // click focuses this lane -- mouseDown() below
        addAndMakeVisible (row->trackLabel);

        row->mute.setClickingTogglesState (true);
        addAndMakeVisible (row->mute);

        for (int ch = 1; ch <= 16; ++ch)
            row->channel.addItem (juce::String (ch), ch);
        row->channel.setSelectedId (def.defaultChannel, juce::dontSendNotification);
        row->channel.onChange = [rowPtr]
        {
            const int ch = juce::jlimit (1, 16, rowPtr->channel.getSelectedId());
            rowPtr->track.channel = ch;
            for (auto& voice : rowPtr->extraVoices)
                voice.track.channel = ch;
        };
        addAndMakeVisible (row->channel);

        // Poly mode: owner's scope is Chords only (index 3) -- Bass/Solo 1/Solo 2 never show or
        // wire these, polyMode stays permanently false for them.
        const bool polyCapable = (i == 3);
        row->polyCapable = polyCapable;
        if (polyCapable)
        {
            row->polyToggle.setClickingTogglesState (true);
            row->polyToggle.onClick = [this, rowPtr]
            {
                rowPtr->polyMode = rowPtr->polyToggle.getToggleState();
                if (! rowPtr->polyMode)
                {
                    rowPtr->subTracksExpanded = false;
                    if (rowPtr->selectedVoice != 0)   // deselect an in-progress sub-voice edit
                    {
                        rowPtr->selectedStep = -1;
                        rowPtr->selectedVoice = 0;
                    }
                }
                refreshStepButtons();
                updateStatusLabel();
                resized();
                repaint();   // resized() alone leaves pcmCardBounds/synthCardBounds's painted background
                             // stale at the old Y once this row's height change shifts everything below it
                             // -- see the Arranger-mode switch's identical resized()+repaint() pair for precedent.
            };
            addAndMakeVisible (row->polyToggle);

            row->subTrackArrow.setClickingTogglesState (true);
            row->subTrackArrow.setToggleState (false, juce::dontSendNotification);
            row->subTrackArrow.setButtonText (juce::String::fromUTF8 ("▶"));
            row->subTrackArrow.onClick = [this, rowPtr]
            {
                rowPtr->subTracksExpanded = rowPtr->subTrackArrow.getToggleState();
                rowPtr->subTrackArrow.setButtonText (rowPtr->subTracksExpanded ? juce::String::fromUTF8 ("▼")
                                                                                : juce::String::fromUTF8 ("▶"));
                resized();
                repaint();   // see row->polyToggle.onClick above
            };
            addAndMakeVisible (row->subTrackArrow);

            for (size_t v = 0; v < rowPtr->extraVoices.size(); ++v)
            {
                auto& voice = rowPtr->extraVoices[v];
                for (size_t step = 0; step < voice.steps.size(); ++step)
                {
                    auto& b = voice.steps[step];
                    b.setStepIndex ((int) step);
                    b.setClickingTogglesState (false);
                    b.setToggleState (false, juce::dontSendNotification);
                    b.onClick = [this, rowPtr, i, v, step]
                    {
                        setFocusedTrack (FocusedTrackKind::pcm, (int) i);
                        if (editButton.getToggleState())
                        {
                            const bool wasThisCell = (rowPtr->selectedStep == (int) step
                                                       && rowPtr->selectedVoice == (int) v + 1);
                            clearPcmSelections();
                            clearDrumSelections();
                            clearSynthPolySelection();
                            if (! wasThisCell)
                            {
                                rowPtr->selectedStep = (int) step;
                                rowPtr->selectedVoice = (int) v + 1;
                                selectedStep = -1;
                            }
                            refreshStepButtons();
                            updateStatusLabel();
                            updateClearLocksEnabled();
                        }
                        else
                        {
                            auto& s = rowPtr->extraVoices[v].track.steps[step];
                            s.enabled = ! s.enabled;
                            if (s.enabled)   // sensible starting pitch: unison with the primary voice
                                s.note = rowPtr->track.steps[step].note;
                            refreshStepButtons();
                            updateStatusLabel();
                        }
                    };
                    addAndMakeVisible (b);
                }
            }
        }

        for (size_t step = 0; step < row->steps.size(); ++step)
        {
            auto& b = row->steps[step];
            b.setStepIndex ((int) step);
            b.setClickingTogglesState (false);
            b.setToggleState (false, juce::dontSendNotification);
            b.onClick = [this, rowPtr, i, step]
            {
                setFocusedTrack (FocusedTrackKind::pcm, (int) i);
                if (editButton.getToggleState())
                {
                    const bool wasThisCell = (rowPtr->selectedStep == (int) step && rowPtr->selectedVoice == 0);
                    const int newSelected = (wasThisCell ? -1 : (int) step);
                    clearPcmSelections();   // mutual exclusion across all 4 PCM lanes (+ their sub-voices)
                    rowPtr->selectedStep = newSelected;
                    rowPtr->selectedVoice = 0;
                    if (newSelected >= 0)
                    {
                        selectedStep = -1;
                        clearDrumSelections();
                        clearSynthPolySelection();
                    }
                    refreshStepButtons();   // also swaps the screen via refreshParamDisplayPages()
                    updateStatusLabel();
                    updateClearLocksEnabled();
                }
                else
                {
                    auto& s = rowPtr->track.steps[step];
                    s.enabled = ! s.enabled;
                    refreshStepButtons();
                    updateStatusLabel();
                }
            };
            addAndMakeVisible (b);
        }

        pcmTrackControls[i] = std::move (row);
    }

    // ---- the pageable p-lock parameter display (the "screen") ------------------------------
    paramDisplay = std::make_unique<ParamPageDisplay> (codec.model());
    rebuildSynthParamPages();
    paramDisplay->onValueEdited = [this] (int index, int value) { onParamEdited (index, value); };
    paramDisplay->onValueReset  = [this] (int index) { onParamReset (index); };
    addAndMakeVisible (*paramDisplay);

    // The window is a fixed-size chassis (Main.cpp sizes itself from this panel's getWidth/Height
    // ONCE at construction, no viewport) -- so poly mode's expanded sub-track rows can't grow the
    // window on demand. Reserve the WORST CASE (every poly-capable lane fully expanded) here
    // unconditionally; resized() only actually lays out sub-rows while a lane is poly+expanded,
    // so collapsed/mono states just leave unused space above the bottom-anchored statusLabel
    // rather than needing a live resize.
    const int kPolyReserve = (kMaxPolyVoices - 1) * (kPolyVoiceRowHeight + kPolySubRowGap);
    setSize (8 + kStepGridWidth + kSectionGap + kLaneLabelWidth + kSectionGap + kCardWidth + 8,
             8 + kToolbarRowHeight + 8 + 20 + 4
                 + (int) std::size (kDrumTracks) * (kDrumTrackRowHeight + 2)
                 + 10 + 20 + 4
                 + (int) std::size (kPcmTracks) * (kPcmTrackRowHeight + 2) + kPolyReserve
                 + 10 + 20 + 4 + juce::jmax (kStepColumnHeight, kSynthSectionHeight) + kPolyReserve
                 + 6 + kFooterHeight + 8);

    selectStep (-1);   // start in Base mode
    clearDrumSelections();
    clearPcmSelections();
    clearSynthPolySelection();
    resized();
}

SequencerPanel::~SequencerPanel()
{
    // The call-out lives on the desktop, not in this component tree — it must not outlive the
    // options/flag members its content component references.
    if (activeCallout != nullptr)
        activeCallout->dismiss();
    stop();
}

void SequencerPanel::applyPreviewDemoState()
{
    for (int i : { 0, 4, 6, 8, 12, 14 })
        sequence.steps[(size_t) i].enabled = true;
    casioxw::setStepLock (sequence, 4, "tssFLTFcoff", 1, 40);
    casioxw::setStepLock (sequence, 4, "tssFLTFreso", 1, 90);
    casioxw::setStepLock (sequence, 12, "tssLFOdep", 1, 64);

    editButton.setToggleState (true, juce::dontSendNotification);
    stepModeButton.setToggleState (false, juce::dontSendNotification);
    selectedStep = 4;
    playheadStep = 8;

    if (auto& row = drumTrackControls[0])
        for (int i : { 0, 4, 8, 12 })
            row->steps[(size_t) i].setToggleState (true, juce::dontSendNotification);
    if (auto& row = drumTrackControls[1])
    {
        row->steps[2].setToggleState (true, juce::dontSendNotification);
        row->velocityLocks[10] = 45;
    }

    refreshParamControls();
    refreshStepButtons();
    updateStatusLabel();
    updateClearLocksEnabled();
}

void SequencerPanel::applyHexLayerPreviewState()
{
    switchEngine (TrackEngine::hexLayer);
    casioxw::setStepLock (sequence, 4, "hexPitchKey", 1, 24);
    casioxw::setStepLock (sequence, 12, "hexAmpAttackOfs", 1, -60);

    editButton.setToggleState (true, juce::dontSendNotification);
    stepModeButton.setToggleState (false, juce::dontSendNotification);
    selectedStep = 4;

    refreshParamControls();
    refreshStepButtons();
    updateStatusLabel();
    updateClearLocksEnabled();
}

void SequencerPanel::applyPcmStepEditPreviewState()
{
    editButton.setToggleState (true, juce::dontSendNotification);
    stepModeButton.setToggleState (false, juce::dontSendNotification);

    if (auto& row = pcmTrackControls[0])   // Bass
    {
        for (int i : { 0, 3, 6, 10 })
            row->track.steps[(size_t) i].enabled = true;
        row->track.steps[3] = { 43, 110, true, 70, {} };   // note/velocity/enabled/gatePercent/locks
        row->selectedStep = 3;                             // select it for editing
        row->mute.setToggleState (false, juce::dontSendNotification);
        focusedTrackKind = FocusedTrackKind::pcm;           // a real click would set this too --
        focusedTrackIndex = 0;                              // seeded directly since this demo pokes
                                                             // selectedStep the same way, bypassing onClick
    }
    if (auto& row = pcmTrackControls[2])   // Solo 2 -- muted, to check the mute button renders distinctly
        row->mute.setToggleState (true, juce::dontSendNotification);

    refreshStepButtons();   // also swaps the screen to Bass step 4's NOTE/GATE/VEL editor
    updateStatusLabel();
}

void SequencerPanel::applyPolyPreviewState()
{
    editButton.setToggleState (true, juce::dontSendNotification);
    stepModeButton.setToggleState (false, juce::dontSendNotification);

    // Chords row: poly on, expanded, a triad on step 1 (primary + 2 extra voices), selecting the
    // first extra voice's step so the screen swaps to its NOTE/GATE/VEL editor.
    if (auto& row = pcmTrackControls[3])
    {
        row->polyMode = true;
        row->polyToggle.setToggleState (true, juce::dontSendNotification);
        row->subTracksExpanded = true;
        row->subTrackArrow.setToggleState (true, juce::dontSendNotification);
        row->subTrackArrow.setButtonText (juce::String::fromUTF8 ("\xE2\x96\xBC"));   // "▼"

        row->track.steps[0] = { 60, 100, true, 90, {} };            // root
        row->extraVoices[0].track.steps[0] = { 64, 95, true, 90, {} };   // third
        row->extraVoices[1].track.steps[0] = { 67, 90, true, 90, {} };   // fifth

        row->selectedStep = 0;
        row->selectedVoice = 1;   // extraVoices[0]
    }

    // Solo lane: switch to Hex Layer (Solo Synth can't go poly), poly on, expanded, a dyad on
    // step 2 (primary + 1 extra voice).
    switchEngine (TrackEngine::hexLayer);
    synthPolyMode = true;
    synthPolyToggle.setToggleState (true, juce::dontSendNotification);
    synthSubTracksExpanded = true;
    synthSubTrackArrow.setToggleState (true, juce::dontSendNotification);
    synthSubTrackArrow.setButtonText (juce::String::fromUTF8 ("\xE2\x96\xBC"));   // "▼"
    sequence.steps[1] = { 55, 100, true, 90, {} };
    synthExtraVoices[0].track.steps[1] = { 58, 90, true, 90, {} };

    refreshStepButtons();   // also swaps the screen + computes the amber/cyan step-key dots
    updateStatusLabel();
    resized();
}

void SequencerPanel::applyFocusPreviewState()
{
    editButton.setToggleState (true, juce::dontSendNotification);
    stepModeButton.setToggleState (false, juce::dontSendNotification);

    if (auto& row = drumTrackControls[1])   // Drum 2: distinctive base note/velocity to spot in the page
    {
        row->note.setValue (38.0, juce::dontSendNotification);
        row->baseVelocity = 77;
    }

    setFocusedTrack (FocusedTrackKind::drum, 1);   // exercises the exact click path, not a shortcut
}

void SequencerPanel::applyArrangerPreviewState()
{
    setShowingArranger (true);
    arrangerPanel->applyPreviewDemoState();
}

bool SequencerPanel::verifyPcmRoundTripForPreview()
{
    if (pcmTrackControls[0] == nullptr || pcmTrackControls[1] == nullptr || pcmTrackControls[3] == nullptr)
        return false;

    auto& bass = pcmTrackControls[0]->track;
    auto& solo1 = pcmTrackControls[1]->track;
    auto& chords = *pcmTrackControls[3];   // the one poly-capable row -- also exercises polyMode/extraVoices

    bass.channel = 13;
    for (int i = 0; i < 16; ++i)
    {
        bass.steps[(size_t) i] = { 30 + i, 90 + i, i % 3 == 0, 40 + i, {} };
        solo1.steps[(size_t) i] = { 60 - i, 20 + i, i % 2 == 0, 10 + i, {} };
    }
    solo1.channel = 14;

    chords.polyMode = true;
    for (int i = 0; i < 16; ++i)
        chords.track.steps[(size_t) i] = { 48 + i, 100, i % 4 == 0, 90, {} };
    for (size_t v = 0; v < chords.extraVoices.size(); ++v)
        for (int i = 0; i < 16; ++i)
            chords.extraVoices[v].track.steps[(size_t) i] =
                { 52 + (int) v + i, 80, i % 5 == 0, 70, {} };

    const auto expectedBass  = bass;
    const auto expectedSolo1 = solo1;
    const bool expectedChordsPoly = chords.polyMode;
    const auto expectedChordsPrimary = chords.track;
    std::array<casioxw::Sequence, kMaxPolyVoices - 1> expectedChordsExtra;
    for (size_t v = 0; v < chords.extraVoices.size(); ++v)
        expectedChordsExtra[v] = chords.extraVoices[v].track;   // PolyVoice itself isn't copyable (owns widgets)

    const auto json = serializePcmTracksToJson();

    // Clobber the live tracks so a false pass (comparing against unchanged data) is impossible.
    bass  = casioxw::Sequence {};
    solo1 = casioxw::Sequence {};
    chords.polyMode = false;
    chords.track = casioxw::Sequence {};
    for (auto& voice : chords.extraVoices)
        voice.track = casioxw::Sequence {};

    if (! applyPcmTracksText (json))
        return false;

    auto stepsMatch = [] (const casioxw::Step& a, const casioxw::Step& b)
    {
        return a.note == b.note && a.velocity == b.velocity
            && a.enabled == b.enabled && a.gatePercent == b.gatePercent;
    };

    if (bass.channel != expectedBass.channel || solo1.channel != expectedSolo1.channel)
        return false;
    if (chords.polyMode != expectedChordsPoly)
        return false;

    for (int i = 0; i < 16; ++i)
    {
        if (! stepsMatch (bass.steps[(size_t) i], expectedBass.steps[(size_t) i])
            || ! stepsMatch (solo1.steps[(size_t) i], expectedSolo1.steps[(size_t) i])
            || ! stepsMatch (chords.track.steps[(size_t) i], expectedChordsPrimary.steps[(size_t) i]))
            return false;

        for (size_t v = 0; v < chords.extraVoices.size(); ++v)
            if (! stepsMatch (chords.extraVoices[v].track.steps[(size_t) i],
                               expectedChordsExtra[v].steps[(size_t) i]))
                return false;
    }

    return true;
}

bool SequencerPanel::verifySoloPolyRoundTripForPreview()
{
    // Mirrors verifyPcmRoundTripForPreview() but for the solo lane's poly state, which has its
    // own (de)serialization path (serializeSoloSequenceToJson()/applySoloSequenceText()) that the
    // PCM check never exercises -- catch drift between the two independently rather than assuming
    // "same pattern as something tested" means "tested."
    if (currentEngine == TrackEngine::soloSynth)
        applyEngine (TrackEngine::hexLayer);   // poly is owner-scoped off for Solo Synth

    synthPolyMode = true;
    for (int i = 0; i < 16; ++i)
        sequence.steps[(size_t) i] = { 36 + i, 110, i % 3 == 1, 55 + i, {} };
    for (size_t v = 0; v < synthExtraVoices.size(); ++v)
        for (int i = 0; i < 16; ++i)
            synthExtraVoices[v].track.steps[(size_t) i] = { 40 + (int) v + i, 90, i % 4 == 1, 65, {} };

    const bool expectedPoly = synthPolyMode;
    const auto expectedPrimary = sequence.steps;
    std::array<casioxw::Sequence, kMaxPolyVoices - 1> expectedExtra;
    for (size_t v = 0; v < synthExtraVoices.size(); ++v)
        expectedExtra[v] = synthExtraVoices[v].track;

    const auto json = serializeSoloSequenceToJson();

    // Clobber before reloading so a false pass (comparing against unchanged data) is impossible.
    synthPolyMode = false;
    for (auto& step : sequence.steps)
        step = casioxw::Step {};
    for (auto& voice : synthExtraVoices)
        voice.track = casioxw::Sequence {};

    if (! applySoloSequenceText (json))
        return false;

    auto stepsMatch = [] (const casioxw::Step& a, const casioxw::Step& b)
    {
        return a.note == b.note && a.velocity == b.velocity
            && a.enabled == b.enabled && a.gatePercent == b.gatePercent;
    };

    if (synthPolyMode != expectedPoly)
        return false;

    for (int i = 0; i < 16; ++i)
    {
        if (! stepsMatch (sequence.steps[(size_t) i], expectedPrimary[(size_t) i]))
            return false;
        for (size_t v = 0; v < synthExtraVoices.size(); ++v)
            if (! stepsMatch (synthExtraVoices[v].track.steps[(size_t) i],
                               expectedExtra[v].steps[(size_t) i]))
                return false;
    }

    return true;
}

void SequencerPanel::paint (juce::Graphics& g)
{
    g.fillAll (EditorColours::chassisBg);

    if (showingArranger)
        return;   // arrangerPanel paints its own content; the step-editor cards below are hidden

    // Right-side control cards: a surface between chassis and widget fill, so panel-coloured
    // buttons/combos still read against them. (Quarter-step cues live on the trig keys
    // themselves now — StepKeyButton paints those outlines.)
    const auto cardColour = EditorColours::chassisBg.interpolatedWith (EditorColours::panelBg, 0.55f);
    for (const auto& card : { drumCardBounds, pcmCardBounds, synthCardBounds })
    {
        if (card.isEmpty())
            continue;
        g.setColour (cardColour);
        g.fillRoundedRectangle (card.toFloat(), 8.0f);
        g.setColour (EditorColours::border.withAlpha (0.3f));
        g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 8.0f, 1.0f);
    }

    // Focused-track highlight: a soft wash across whichever lane the shift arrows + the LCD's
    // "Base" display currently act on (see FocusedTrackKind's doc comment). Amber-toned
    // (EditorColours::selected), not the playhead's cyan, so playback position and edit focus
    // never share a colour -- the same rule the original playhead-vs-selected comment below states.
    juce::Rectangle<int> focusRow;
    if (focusedTrackKind == FocusedTrackKind::drum && focusedTrackIndex >= 0
        && drumTrackControls[(size_t) focusedTrackIndex] != nullptr)
        focusRow = drumTrackControls[(size_t) focusedTrackIndex]->rowBounds;
    else if (focusedTrackKind == FocusedTrackKind::pcm && focusedTrackIndex >= 0
             && pcmTrackControls[(size_t) focusedTrackIndex] != nullptr)
        focusRow = pcmTrackControls[(size_t) focusedTrackIndex]->rowBounds;
    else if (focusedTrackKind == FocusedTrackKind::soloSynth)
        focusRow = synthFocusBounds;

    if (! focusRow.isEmpty())
    {
        g.setColour (EditorColours::selected.withAlpha (0.14f));
        g.fillRoundedRectangle (focusRow.toFloat(), 6.0f);
        g.setColour (EditorColours::selected.withAlpha (0.55f));
        g.drawRoundedRectangle (focusRow.toFloat().reduced (0.5f), 6.0f, 1.5f);
    }

    if (playheadStep < 0 || playheadLaneBounds.isEmpty())
        return;

    const int clamped = juce::jlimit (0, 15, playheadStep);
    auto column = playheadLaneBounds.withX (playheadLaneBounds.getX() + clamped * kStepWidth)
                                    .withWidth (kStepWidth)
                                    .reduced (2, 0);
    // Cyan, not the amber "selected" hue -- playback position and edit focus are different facts
    // and shouldn't share a colour when a step happens to be both at once.
    g.setColour (EditorColours::playhead.withAlpha (0.28f));
    g.fillRoundedRectangle (column.toFloat(), 4.0f);
}

std::vector<juce::MidiMessage> SequencerPanel::paramMessages (const juce::String& paramId,
                                                             int instance, int value, int channel) const
{
    // Always encode via the codec's SysEx path. It runs the value through the param's per-vt
    // encoder (nf/cf/wf/tn/pk/cF) and addresses by the param's real 18-byte address, so signed/
    // scaled values are correct and every write lands on the right parameter -- both golden-tested
    // (decode(encode(x))==x for every vt) and hardware-verified by the tone editor.
    //
    // A prior "prefer NRPN to trim wire traffic" optimization was removed here: it sent the raw UI
    // value as a plain 7-bit Data Entry (wrong for every cf/wf/tn param -- e.g. touch-sense 8
    // landed as 8-64 = -56 on the synth) and used hand-derived addr-72 / addr-91 NRPN LSBs that
    // were off for the filter-envelope block (writes shifted onto neighbouring params). Over
    // USB-MIDI the SysEx byte count was never the real constraint. A correct NRPN transport could
    // return later, but only with per-vt value encoding and hardware-verified addresses.
    (void) channel;   // solo-synth params are addressed in the SysEx frame, not by channel-voice prefix
    const auto frame = codec.encode (paramId, instance, value);
    if (frame.size() < 3)
        return {};
    // createSysExMessage() re-adds its own F0/F7, so pass only the bytes between them (as sendFrame).
    return { juce::MidiMessage::createSysExMessage (frame.data() + 1, (int) frame.size() - 2) };
}

void SequencerPanel::sendParamNow (const juce::String& paramId, int instance, int value)
{
    for (const auto& m : paramMessages (paramId, instance, value, sequence.channel))
        midiIO.sendMessageNow (m);
}

void SequencerPanel::randomizeSequence()
{
    auto options = randomizeOptions;
    options.lockableIndices = randomizeComboParams ? std::vector<int>{}   // empty == all eligible
                                                   : continuousLockables;
    casioxw::randomize (sequence, rng, options);
    refreshParamControls();   // selected step's locks may have changed
    refreshStepButtons();     // has-locks markers
}

void SequencerPanel::showRandomizeOptions()
{
    auto content = std::make_unique<RandomizeOptionsComponent> (randomizeOptions, randomizeComboParams);
    activeCallout = &juce::CallOutBox::launchAsynchronously (std::move (content),
                                                             rndOptionsButton.getScreenBounds(),
                                                             nullptr);
}

void SequencerPanel::syncTransportWidgetsFromSequence()
{
    tempoSlider.setValue ((double) sequence.tempoBpm, juce::dontSendNotification);
    channelSlider.setValue ((double) sequence.channel, juce::dontSendNotification);
    rateCombo.setSelectedId (sequence.stepsPerBeat, juce::dontSendNotification);
}

void SequencerPanel::saveSequenceToFile()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Save Solo Sequence (.xwseq)");
    menu.addItem (2, "Save Drum Sequence (.xwdrm)");
    menu.addItem (3, "Save PCM Tracks (.xwpcm)");
    menu.addItem (4, "Save Sequence Set (.xwset)");
    menu.showMenuAsync (juce::PopupMenu::Options(),
                        [this] (int result)
                        {
                            if (result == 1) saveByKind (SaveKind::solo);
                            if (result == 2) saveByKind (SaveKind::drums);
                            if (result == 3) saveByKind (SaveKind::pcm);
                            if (result == 4) saveByKind (SaveKind::sequenceSet);
                        });
}

void SequencerPanel::loadSequenceFromFile()
{
    const auto fallbackDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    auto baseDir = sequenceDefaultDirectory;
    if (baseDir.existsAsFile())
        baseDir = baseDir.getParentDirectory();
    if (! baseDir.isDirectory())
        baseDir = fallbackDir;
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load sequence",
        baseDir,
        "*.xwseq;*.xwdrm;*.xwset",
        false);

    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (! file.existsAsFile())
                return;   // cancelled
            applyLoadedText (file.loadFileAsString(), file);
        });
}

void SequencerPanel::saveByKind (SaveKind kind)
{
    const auto fallbackDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    auto baseDir = sequenceDefaultDirectory;
    if (baseDir.existsAsFile())
        baseDir = baseDir.getParentDirectory();
    if (! baseDir.isDirectory())
        baseDir = fallbackDir;

    juce::String fileName;
    juce::String wildcard;
    juce::String payload;

    if (kind == SaveKind::solo)
    {
        fileName = "solo-sequence.xwseq";
        wildcard = "*.xwseq";
        payload = serializeSoloSequenceToJson();
    }
    else if (kind == SaveKind::drums)
    {
        fileName = "drum-sequence.xwdrm";
        wildcard = "*.xwdrm";
        payload = serializeDrumsToJson();
    }
    else if (kind == SaveKind::pcm)
    {
        fileName = "pcm-tracks.xwpcm";
        wildcard = "*.xwpcm";
        payload = serializePcmTracksToJson();
    }
    else
    {
        fileName = "sequence-set.xwset";
        wildcard = "*.xwset";
    }

    fileChooser = std::make_unique<juce::FileChooser> (
        "Save sequence",
        baseDir.getChildFile (fileName),
        wildcard,
        false);

    fileChooser->launchAsync (
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, kind, payload] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File())
                return; // cancelled

            const juce::String ext = kind == SaveKind::solo ? ".xwseq"
                                     : kind == SaveKind::drums ? ".xwdrm"
                                     : kind == SaveKind::pcm ? ".xwpcm"
                                                              : ".xwset";
            if (! file.hasFileExtension (ext.substring (1)))
                file = file.withFileExtension (ext.substring (1));

            if (kind == SaveKind::sequenceSet)
            {
                const auto baseName = file.getFileNameWithoutExtension();
                const auto soloName = baseName + ".solo.xwseq";
                const auto drumName = baseName + ".drums.xwdrm";
                const auto pcmName = baseName + ".pcm.xwpcm";
                const auto soloFile = file.getSiblingFile (soloName);
                const auto drumFile = file.getSiblingFile (drumName);
                const auto pcmFile = file.getSiblingFile (pcmName);
                const auto setPayload = serializeSequenceSetToJson (soloName, drumName, pcmName);

                const bool okSolo = soloFile.replaceWithText (serializeSoloSequenceToJson());
                const bool okDrums = drumFile.replaceWithText (serializeDrumsToJson());
                const bool okPcm = pcmFile.replaceWithText (serializePcmTracksToJson());
                const bool okSet = file.replaceWithText (setPayload);

                if (okSolo && okDrums && okPcm && okSet)
                    statusLabel.setText ("Saved set + refs: " + file.getFileName(), juce::dontSendNotification);
                else
                    statusLabel.setText ("Save failed: " + file.getFullPathName(), juce::dontSendNotification);
            }
            else
            {
                if (file.replaceWithText (payload))
                    statusLabel.setText ("Saved " + file.getFileName(), juce::dontSendNotification);
                else
                    statusLabel.setText ("Save failed: " + file.getFullPathName(), juce::dontSendNotification);
            }
        });
}

juce::String SequencerPanel::serializeDrumsToJson() const
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("format", "casioxw-drum-sequence");
    root->setProperty ("version", 1);
    root->setProperty ("tempoBpm", sequence.tempoBpm);
    root->setProperty ("stepsPerBeat", sequence.stepsPerBeat);

    juce::Array<juce::var> tracks;
    for (const auto& row : drumTrackControls)
    {
        if (row == nullptr)
            continue;

        juce::DynamicObject::Ptr t = new juce::DynamicObject();
        t->setProperty ("channel", row->channel.getSelectedId());
        t->setProperty ("note", (int) row->note.getValue());
        t->setProperty ("baseVelocity", row->baseVelocity);

        juce::Array<juce::var> steps;
        juce::Array<juce::var> locks;
        for (int i = 0; i < 16; ++i)
        {
            steps.add (row->steps[(size_t) i].getToggleState());
            if (const auto v = row->velocityLocks[(size_t) i])
                locks.add (*v);
            else
                locks.add (juce::var());
        }
        t->setProperty ("steps", steps);
        t->setProperty ("velocityLocks", locks);
        tracks.add (juce::var (t.get()));
    }
    root->setProperty ("tracks", tracks);
    return juce::JSON::toString (juce::var (root.get()));
}

juce::String SequencerPanel::serializePcmTracksToJson() const
{
    // Each PCM track IS a casioxw::Sequence, so reuse sequenceToJson() per track instead of
    // hand-rolling per-field JSON (same round-trip guarantees as the Solo Synth save). Poly mode/
    // extra voices are an app-level concept core's Sequence doesn't know about (see PolyVoice's
    // doc comment) -- carried as ADDITIONAL properties on the same per-track object rather than a
    // new envelope, so an old build (or Bass/Solo1/Solo2, which never set polyMode) round-trips
    // unchanged; sequenceFromJson() ignores unknown properties.
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("format", "casioxw-pcm-tracks");
    root->setProperty ("version", 1);

    juce::Array<juce::var> tracks;
    for (const auto& row : pcmTrackControls)
    {
        if (row == nullptr)
        {
            tracks.add (juce::var());
            continue;
        }
        auto parsed = juce::JSON::parse (casioxw::sequenceToJson (row->track));
        if (auto* obj = parsed.getDynamicObject())
        {
            obj->setProperty ("polyMode", row->polyMode);
            juce::Array<juce::var> voices;
            for (const auto& voice : row->extraVoices)
                voices.add (juce::JSON::parse (casioxw::sequenceToJson (voice.track)));
            obj->setProperty ("extraVoices", voices);
        }
        tracks.add (parsed);
    }
    root->setProperty ("tracks", tracks);
    return juce::JSON::toString (juce::var (root.get()));
}

juce::String SequencerPanel::serializeSoloSequenceToJson() const
{
    // casioxw::sequenceToJson() only knows the core Sequence shape -- poly mode/extra voices are
    // an app-level concept (a voice's `lockable` is always empty: hardware has one filter/
    // envelope per PART, not per simultaneous note, so there's nothing core-shaped to add), so
    // they ride as ADDITIONAL top-level properties on the same "casioxw-sequence" object rather
    // than a new envelope format -- sequenceFromJson() ignores unknown properties, so an old
    // build (or a Solo Synth file, which never sets synthPolyMode) round-trips unchanged.
    auto parsed = juce::JSON::parse (casioxw::sequenceToJson (sequence));
    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
        return casioxw::sequenceToJson (sequence);   // shouldn't happen; fall back rather than crash

    obj->setProperty ("synthPolyMode", synthPolyMode);
    juce::Array<juce::var> voices;
    for (const auto& voice : synthExtraVoices)
        voices.add (juce::JSON::parse (casioxw::sequenceToJson (voice.track)));
    obj->setProperty ("synthExtraVoices", voices);
    return juce::JSON::toString (parsed);
}

juce::String SequencerPanel::serializeSequenceSetToJson (const juce::String& soloFile, const juce::String& drumsFile,
                                                         const juce::String& pcmFile) const
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("format", "casioxw-sequence-set-ref");
    root->setProperty ("version", 1);
    root->setProperty ("soloFile", soloFile);
    root->setProperty ("drumsFile", drumsFile);
    root->setProperty ("pcmFile", pcmFile);
    // Keep inline copies too so old/new loaders can still recover even if sidecars are moved.
    root->setProperty ("solo", juce::JSON::parse (serializeSoloSequenceToJson()));
    root->setProperty ("drums", juce::JSON::parse (serializeDrumsToJson()));
    root->setProperty ("pcm", juce::JSON::parse (serializePcmTracksToJson()));
    return juce::JSON::toString (juce::var (root.get()));
}

bool SequencerPanel::applySoloSequenceText (const juce::String& text)
{
    const auto loaded = casioxw::sequenceFromJson (text);
    if (! loaded.has_value())
        return false;

    // Adopt the file's engine FIRST (rebuilds sequence.lockable + paramDisplay's pages for that
    // engine's table) so the base-value import below matches against the right paramIds. Missing/
    // unrecognised tag -> soloSynth (old files predate multi-engine support).
    //
    // Unlike switchEngine() (the combo's user-driven path), this is NOT gated on `! playing` --
    // loading a file already reassigns sequence.steps/channel/tempo unconditionally below, so a
    // load-while-playing was already a "musical content changes out from under the transport"
    // operation before engines existed; rebuilding the lockable set here is the same class of
    // risk (params may sit stuck until stop), not a new one this change introduces.
    const auto loadedEngine = engineFromTag (loaded->engineTag);
    if (loadedEngine != currentEngine)
        applyEngine (loadedEngine);

    // Adopt the loaded musical content, but keep THIS panel's lockable set + controls intact
    // (fixed by resolveEngineLockableSet() for the now-current engine/layer, already has the
    // metadata min/max seeded). Only import each known lockable param's base value, matched by
    // id+instance -- for Hex Layer this means a saved file's per-layer bases only land if
    // currentHexLayer already matches whatever layer was selected when it was saved (same
    // one-active-selector limitation switchEngine() already has for engines).
    sequence.steps        = loaded->steps;
    sequence.channel      = loaded->channel;
    sequence.tempoBpm     = loaded->tempoBpm;
    sequence.stepsPerBeat = loaded->stepsPerBeat;
    for (auto& lp : sequence.lockable)
        for (const auto& llp : loaded->lockable)
            if (llp.paramId == lp.paramId && llp.instance == lp.instance)
                lp.baseValue = llp.baseValue;

    // Poly state (app-level, not part of casioxw::Sequence -- see serializeSoloSequenceToJson()'s
    // doc comment). Missing/absent -> mono, matching every pre-poly file's implicit state; still
    // owner-scoped to Hex Layer/Drawbar Organ regardless of what the file itself says.
    const auto polyParsed = juce::JSON::parse (text);
    auto* polyObj = polyParsed.getDynamicObject();
    const bool loadedPoly = polyObj != nullptr && (bool) polyObj->getProperty ("synthPolyMode");
    synthPolyMode = loadedPoly && currentEngine != TrackEngine::soloSynth;
    synthPolyToggle.setToggleState (synthPolyMode, juce::dontSendNotification);
    if (! synthPolyMode)
    {
        synthSubTracksExpanded = false;
        synthSubTrackArrow.setToggleState (false, juce::dontSendNotification);
        synthSubTrackArrow.setButtonText (juce::String::fromUTF8 ("\xE2\x96\xB6"));   // "▶"
    }
    clearSynthPolySelection();

    for (auto& voice : synthExtraVoices)
        for (auto& step : voice.track.steps)
            step = casioxw::Step {};   // default: no extra-voice content, overwritten below if saved

    if (polyObj != nullptr)
        if (const auto* voicesArr = polyObj->getProperty ("synthExtraVoices").getArray())
        {
            const int n = juce::jmin ((int) synthExtraVoices.size(), voicesArr->size());
            for (int i = 0; i < n; ++i)
            {
                const auto voiceLoaded = casioxw::sequenceFromJson (juce::JSON::toString ((*voicesArr)[i]));
                if (voiceLoaded.has_value())
                    synthExtraVoices[(size_t) i].track.steps = voiceLoaded->steps;
            }
        }

    resized();   // synthSubTrackArrow visibility/text and the sub-row reservation depend on the above
    repaint();   // a loaded file's poly state can shift synthCardBounds -- see synthPolyToggle.onClick
    return true;
}

bool SequencerPanel::applyDrumSequenceText (const juce::String& text)
{
    const auto parsed = juce::JSON::parse (text);
    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr || obj->getProperty ("format").toString() != "casioxw-drum-sequence")
        return false;

    const auto tempoVar = obj->getProperty ("tempoBpm");
    if (! tempoVar.isVoid())
        sequence.tempoBpm = (int) tempoVar;
    const auto rateVar = obj->getProperty ("stepsPerBeat");
    if (! rateVar.isVoid())
        sequence.stepsPerBeat = (int) rateVar;

    const auto tracks = obj->getProperty ("tracks").getArray();
    if (tracks == nullptr)
        return false;

    const int n = juce::jmin ((int) drumTrackControls.size(), tracks->size());
    for (int i = 0; i < n; ++i)
    {
        auto* t = (*tracks)[i].getDynamicObject();
        if (t == nullptr || drumTrackControls[(size_t) i] == nullptr)
            continue;

        auto& row = *drumTrackControls[(size_t) i];
        const auto chVar = t->getProperty ("channel");
        const auto noteVar = t->getProperty ("note");
        const auto baseVar = t->getProperty ("baseVelocity");
        row.channel.setSelectedId (chVar.isVoid() ? row.channel.getSelectedId() : (int) chVar, juce::dontSendNotification);
        row.note.setValue ((double) (noteVar.isVoid() ? (int) row.note.getValue() : (int) noteVar), juce::dontSendNotification);
        row.baseVelocity = juce::jlimit (1, 127, baseVar.isVoid() ? row.baseVelocity : (int) baseVar);
        row.selectedStep = -1;
        row.velocityLocks.fill (std::nullopt);
        for (int s = 0; s < 16; ++s)
            row.steps[(size_t) s].setToggleState (false, juce::dontSendNotification);

        if (const auto* steps = t->getProperty ("steps").getArray())
        {
            const int stepCount = juce::jmin (16, steps->size());
            for (int s = 0; s < stepCount; ++s)
                row.steps[(size_t) s].setToggleState ((bool) steps->getReference (s), juce::dontSendNotification);
        }

        if (const auto* locks = t->getProperty ("velocityLocks").getArray())
        {
            const int lockCount = juce::jmin (16, locks->size());
            for (int s = 0; s < lockCount; ++s)
            {
                const auto& v = locks->getReference (s);
                row.velocityLocks[(size_t) s] = v.isVoid() ? std::optional<int>() : std::optional<int> ((int) v);
            }
        }
    }
    return true;
}

bool SequencerPanel::applyPcmTracksText (const juce::String& text)
{
    const auto parsed = juce::JSON::parse (text);
    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr || obj->getProperty ("format").toString() != "casioxw-pcm-tracks")
        return false;

    const auto* tracks = obj->getProperty ("tracks").getArray();
    if (tracks == nullptr)
        return false;

    const int n = juce::jmin ((int) pcmTrackControls.size(), tracks->size());
    for (int i = 0; i < n; ++i)
    {
        if (pcmTrackControls[(size_t) i] == nullptr)
            continue;
        auto& row = *pcmTrackControls[(size_t) i];

        const auto& trackVar = (*tracks)[i];
        if (trackVar.getDynamicObject() == nullptr)
            continue;   // this track slot wasn't saved (older/shorter file) -- leave it as-is

        const auto loaded = casioxw::sequenceFromJson (juce::JSON::toString (trackVar));
        if (! loaded.has_value())
            continue;

        row.track.steps        = loaded->steps;
        row.track.channel      = loaded->channel;
        row.track.tempoBpm     = loaded->tempoBpm;
        row.track.stepsPerBeat = loaded->stepsPerBeat;
        row.channel.setSelectedId (juce::jlimit (1, 16, row.track.channel), juce::dontSendNotification);

        // Poly state (app-level, not part of casioxw::Sequence -- see serializePcmTracksToJson()'s
        // doc comment). Missing/absent -> mono; still owner-scoped to the Chords row regardless of
        // what the file says (a hand-edited or older-format file can't turn on poly for Bass/Solo1/
        // Solo2, matching PcmTrackControl::polyCapable's construction-time scope).
        auto* trackObj = trackVar.getDynamicObject();
        const bool loadedPoly = trackObj != nullptr && (bool) trackObj->getProperty ("polyMode");
        row.polyMode = loadedPoly && row.polyCapable;
        row.polyToggle.setToggleState (row.polyMode, juce::dontSendNotification);
        if (! row.polyMode)
        {
            row.subTracksExpanded = false;
            row.subTrackArrow.setToggleState (false, juce::dontSendNotification);
            row.subTrackArrow.setButtonText (juce::String::fromUTF8 ("\xE2\x96\xB6"));   // "▶"
        }
        if (row.selectedVoice != 0)
        {
            row.selectedStep = -1;
            row.selectedVoice = 0;
        }

        for (auto& voice : row.extraVoices)
            for (auto& step : voice.track.steps)
                step = casioxw::Step {};   // default: no extra-voice content, overwritten below if saved

        if (trackObj != nullptr)
            if (const auto* voicesArr = trackObj->getProperty ("extraVoices").getArray())
            {
                const int vn = juce::jmin ((int) row.extraVoices.size(), voicesArr->size());
                for (int v = 0; v < vn; ++v)
                {
                    const auto voiceLoaded = casioxw::sequenceFromJson (juce::JSON::toString ((*voicesArr)[v]));
                    if (voiceLoaded.has_value())
                        row.extraVoices[(size_t) v].track.steps = voiceLoaded->steps;
                }
            }
    }
    resized();   // polyToggle/subTrackArrow visibility and the sub-row reservation depend on the above
    repaint();   // a loaded file's poly state can shift pcmCardBounds/synthCardBounds -- see
                 // synthPolyToggle.onClick above
    return true;
}

bool SequencerPanel::applyLoadedText (const juce::String& text, const juce::File& sourceFile)
{
    const auto parsed = juce::JSON::parse (text);
    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
    {
        statusLabel.setText ("Load failed: " + sourceFile.getFileName() + " is not valid JSON",
                             juce::dontSendNotification);
        return false;
    }

    const auto format = obj->getProperty ("format").toString();

    bool ok = false;
    if (format == "casioxw-sequence")
    {
        ok = applySoloSequenceText (text);
    }
    else if (format == "casioxw-drum-sequence")
    {
        ok = applyDrumSequenceText (text);
    }
    else if (format == "casioxw-pcm-tracks")
    {
        ok = applyPcmTracksText (text);
    }
    else if (format == "casioxw-sequence-set-ref" || format == "casioxw-sequence-set")
    {
        ok = true;

        const auto soloRef = obj->getProperty ("soloFile").toString();
        const auto drumsRef = obj->getProperty ("drumsFile").toString();
        const auto pcmRef = obj->getProperty ("pcmFile").toString();   // absent in older set files
        if (soloRef.isNotEmpty() && drumsRef.isNotEmpty())
        {
            const auto dir = sourceFile.getParentDirectory();
            const auto soloFile = dir.getChildFile (soloRef);
            const auto drumsFile = dir.getChildFile (drumsRef);
            if (soloFile.existsAsFile() && drumsFile.existsAsFile())
            {
                ok = applySoloSequenceText (soloFile.loadFileAsString()) && ok;
                ok = applyDrumSequenceText (drumsFile.loadFileAsString()) && ok;
                if (pcmRef.isNotEmpty())
                {
                    const auto pcmFile = dir.getChildFile (pcmRef);
                    if (pcmFile.existsAsFile())
                        applyPcmTracksText (pcmFile.loadFileAsString());   // optional -- absent doesn't fail the set
                }
            }
            else
            {
                ok = false;
            }
        }
        else
        {
            ok = false;
        }

        // Fallback to embedded content for older/moved set files.
        if (! ok)
        {
            ok = true;
            const auto solo = obj->getProperty ("solo");
            if (solo.getDynamicObject() != nullptr)
                ok = applySoloSequenceText (juce::JSON::toString (solo)) && ok;
            else
                ok = false;
            const auto drums = obj->getProperty ("drums");
            if (drums.getDynamicObject() != nullptr)
                ok = applyDrumSequenceText (juce::JSON::toString (drums)) && ok;
            else
                ok = false;
            const auto pcm = obj->getProperty ("pcm");   // optional -- absent in older set files
            if (pcm.getDynamicObject() != nullptr)
                applyPcmTracksText (juce::JSON::toString (pcm));
        }
    }
    else
    {
        // Backward compatibility for very early files that may have lacked `format`.
        ok = applySoloSequenceText (text);
    }

    if (! ok)
    {
        statusLabel.setText ("Load failed: unsupported or invalid sequence file " + sourceFile.getFileName(),
                             juce::dontSendNotification);
        return false;
    }

    syncTransportWidgetsFromSequence();
    selectStep (-1);   // back to Base; also refreshes param controls, step markers, status
    clearDrumSelections();
    clearPcmSelections();
    clearSynthPolySelection();
    statusLabel.setText ("Loaded " + sourceFile.getFileName(), juce::dontSendNotification);
    return true;
}

void SequencerPanel::chooseSequenceDirectory()
{
    const auto fallbackDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    auto baseDir = sequenceDefaultDirectory;
    if (baseDir.existsAsFile())
        baseDir = baseDir.getParentDirectory();
    if (! baseDir.isDirectory())
        baseDir = fallbackDir;

    fileChooser = std::make_unique<juce::FileChooser> (
        "Choose sequence folder",
        baseDir,
        "*",
        false);

    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this] (const juce::FileChooser& fc)
        {
            const auto picked = fc.getResult();
            if (picked == juce::File())
                return;

            auto dir = picked.isDirectory() ? picked : picked.getParentDirectory();
            if (! dir.isDirectory())
            {
                statusLabel.setText ("Sequence folder selection failed", juce::dontSendNotification);
                return;
            }

            sequenceDefaultDirectory = dir.getFullPathName();
            saveSequenceSettings();
            if (arrangerPanel != nullptr)
                arrangerPanel->setSequenceDirectory (sequenceDefaultDirectory);
            statusLabel.setText ("Sequence folder: " + dir.getFullPathName(), juce::dontSendNotification);
        });
}

juce::File SequencerPanel::settingsFilePath() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                    .getChildFile ("CasioXWEditor");
    dir.createDirectory();
    return dir.getChildFile ("sequencer-settings.json");
}

void SequencerPanel::loadSequenceSettings()
{
    sequenceDefaultDirectory = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    const auto settingsFile = settingsFilePath();
    if (! settingsFile.existsAsFile())
        return;

    const auto parsed = juce::JSON::parse (settingsFile.loadFileAsString());
    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
        return;

    const auto path = obj->getProperty ("sequenceDefaultDirectory").toString();
    if (path.isNotEmpty())
    {
        juce::File configured (path);
        if (configured.isDirectory())
            sequenceDefaultDirectory = configured;
    }
}

void SequencerPanel::saveSequenceSettings() const
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("sequenceDefaultDirectory", sequenceDefaultDirectory.getFullPathName());
    settingsFilePath().replaceWithText (juce::JSON::toString (juce::var (root.get())));
}

bool SequencerPanel::hasAnyDrumStepSelected() const
{
    for (const auto& row : drumTrackControls)
        if (row != nullptr && row->selectedStep >= 0)
            return true;
    return false;
}

bool SequencerPanel::hasAnyPcmStepSelected() const
{
    for (const auto& row : pcmTrackControls)
        if (row != nullptr && row->selectedStep >= 0)
            return true;
    return false;
}

void SequencerPanel::clearDrumSelections()
{
    for (auto& row : drumTrackControls)
        if (row != nullptr)
            row->selectedStep = -1;
}

void SequencerPanel::clearPcmSelections()
{
    for (auto& row : pcmTrackControls)
        if (row != nullptr)
        {
            row->selectedStep = -1;
            row->selectedVoice = 0;
        }
}

// The solo lane's poly sub-voice edit target (synthPolyStep/synthSelectedVoice) is a THIRD axis
// alongside selectedStep (the primary voice's p-lock target) and the PCM lanes -- distinct
// because, unlike a PCM row, the solo lane's OWN selectedStep already means something (the p-lock
// target) even when not poly, so a sub-voice's note/gate/vel edit target needs its own state.
// Called everywhere clearDrumSelections()/clearPcmSelections() are, so selecting anything else
// always deselects an in-progress poly sub-voice edit.
void SequencerPanel::clearSynthPolySelection()
{
    synthPolyStep = -1;
    synthSelectedVoice = 0;
}

void SequencerPanel::updateClearLocksEnabled()
{
    clearLocksButton.setEnabled (editButton.getToggleState() && (selectedStep >= 0 || hasAnyDrumStepSelected()));
}

void SequencerPanel::clearAllSteps()
{
    // Wipe the PATTERN on every lane back to empty rests, but leave the sound/setup alone:
    // channels, tempo/rate, per-lane mutes, and the lockable base values all stay. Safe while
    // playing -- the feeder reads the model live, so cleared steps simply stop triggering (any
    // already-queued note this loop still gets its paired note-off).

    // Solo Synth lane: default-construct each step (enabled=false, note=C4, gate=90, vel=100, no
    // locks). Base values in sequence.lockable are untouched.
    for (auto& step : sequence.steps)
        step = casioxw::Step {};

    // Drum lanes: trigs off + per-step velocity locks cleared (base velocity/channel/mute kept).
    for (auto& row : drumTrackControls)
    {
        if (row == nullptr)
            continue;
        for (auto& b : row->steps)
            b.setToggleState (false, juce::dontSendNotification);
        for (auto& lock : row->velocityLocks)
            lock.reset();
    }

    // PCM lanes: default-construct each step of the lane's own sequence + any poly sub-voices
    // (PCM Chords' extraVoices; polyMode/subTracksExpanded themselves are sound-setup, not
    // pattern, so left alone, matching how per-lane mutes are kept elsewhere in this function).
    for (auto& row : pcmTrackControls)
        if (row != nullptr)
        {
            for (auto& step : row->track.steps)
                step = casioxw::Step {};
            for (auto& voice : row->extraVoices)
                for (auto& step : voice.track.steps)
                    step = casioxw::Step {};
        }

    // Solo lane's poly sub-voices (only ever populated while engine is Hex Layer/Drawbar Organ).
    for (auto& voice : synthExtraVoices)
        for (auto& step : voice.track.steps)
            step = casioxw::Step {};

    // Drop every edit target so nothing points at now-empty data, then refresh. selectStep(-1)
    // handles param controls / step buttons / status / clear-locks enablement (including the
    // NOTE page, since it's just another consequence of selectedStep going to -1 now).
    clearDrumSelections();
    clearPcmSelections();
    clearSynthPolySelection();
    selectStep (-1);

    statusLabel.setText ("Cleared all steps", juce::dontSendNotification);
}

void SequencerPanel::selectStep (int step)
{
    selectedStep = step;
    if (step >= 0)
    {
        clearDrumSelections();
        clearPcmSelections();
        clearSynthPolySelection();
    }
    updateClearLocksEnabled();
    refreshParamControls();
    refreshStepButtons();
    updateStatusLabel();
}

void SequencerPanel::setFocusedTrack (FocusedTrackKind kind, int index)
{
    focusedTrackKind = kind;
    focusedTrackIndex = index;
    refreshStepButtons();   // tail-calls refreshParamDisplayPages(), keeping the LCD in sync too
    updateStatusLabel();
    repaint();              // the row highlight lives in paint(), not any child widget
}

void SequencerPanel::shiftDrumTrack (DrumTrackControl& row, int delta)
{
    // Mirrors casioxw::shiftSteps()'s own rotation exactly (Sequence.cpp) -- a drum lane has no
    // Sequence backing it (its pattern is the StepKeyButtons' own toggle state + velocityLocks,
    // see DrumTrackControl's doc comment), so the rotation has to be done by hand here instead of
    // reusing the core function, but the direction/formula must stay identical or drum shifting
    // would feel backwards relative to every other lane's shift.
    constexpr int n = 16;
    const int d = ((delta % n) + n) % n;
    if (d == 0)
        return;

    std::array<bool, n> rotatedTriggers {};
    std::array<std::optional<int>, n> rotatedLocks {};
    for (int i = 0; i < n; ++i)
    {
        rotatedTriggers[(size_t) ((i + d) % n)] = row.steps[(size_t) i].getToggleState();
        rotatedLocks[(size_t) ((i + d) % n)]    = row.velocityLocks[(size_t) i];
    }
    for (int i = 0; i < n; ++i)
        row.steps[(size_t) i].setToggleState (rotatedTriggers[(size_t) i], juce::dontSendNotification);
    row.velocityLocks = rotatedLocks;
}

void SequencerPanel::shiftFocusedTrack (int delta)
{
    switch (focusedTrackKind)
    {
        case FocusedTrackKind::soloSynth:
            casioxw::shiftSteps (sequence, delta);
            if (synthPolyMode)
                for (auto& voice : synthExtraVoices)
                    casioxw::shiftSteps (voice.track, delta);
            break;

        case FocusedTrackKind::drum:
            if (focusedTrackIndex >= 0 && focusedTrackIndex < (int) drumTrackControls.size()
                && drumTrackControls[(size_t) focusedTrackIndex] != nullptr)
                shiftDrumTrack (*drumTrackControls[(size_t) focusedTrackIndex], delta);
            break;

        case FocusedTrackKind::pcm:
            if (focusedTrackIndex >= 0 && focusedTrackIndex < (int) pcmTrackControls.size()
                && pcmTrackControls[(size_t) focusedTrackIndex] != nullptr)
            {
                auto& row = *pcmTrackControls[(size_t) focusedTrackIndex];
                casioxw::shiftSteps (row.track, delta);
                if (row.polyMode)
                    for (auto& voice : row.extraVoices)
                        casioxw::shiftSteps (voice.track, delta);
            }
            break;
    }
    refreshParamControls();
    refreshStepButtons();
}

void SequencerPanel::mouseDown (const juce::MouseEvent& e)
{
    // A label click is a PURE focus change -- unlike a step click (which sets focus alongside its
    // own selection), it has no step of its own to select. A step selection left over on some
    // OTHER lane would otherwise still win refreshParamDisplayPages()'s priority order (it checks
    // selections before falling back to focus), so the LCD would show that stale step instead of
    // the newly-focused track's Base -- clear every lane's selection here so focus and the LCD
    // never disagree. selectStep(-1) alone would break a step's own toggle-off click (that handler
    // relies on reading its lane's still-live selectedStep to decide select-vs-deselect), which is
    // exactly why this clear lives here and not inside setFocusedTrack() itself.
    const auto clearAllSelections = [this]
    {
        selectedStep = -1;
        clearDrumSelections();
        clearPcmSelections();
        clearSynthPolySelection();
    };

    if (e.eventComponent == &synthLabel)
    {
        clearAllSelections();
        setFocusedTrack (FocusedTrackKind::soloSynth, -1);
        return;
    }
    for (size_t i = 0; i < drumTrackControls.size(); ++i)
        if (drumTrackControls[i] != nullptr && e.eventComponent == &drumTrackControls[i]->trackLabel)
        {
            clearAllSelections();
            setFocusedTrack (FocusedTrackKind::drum, (int) i);
            return;
        }
    for (size_t i = 0; i < pcmTrackControls.size(); ++i)
        if (pcmTrackControls[i] != nullptr && e.eventComponent == &pcmTrackControls[i]->trackLabel)
        {
            clearAllSelections();
            setFocusedTrack (FocusedTrackKind::pcm, (int) i);
            return;
        }
}

void SequencerPanel::setPLockMode (bool pLockMode)
{
    if (! pLockMode)
    {
        selectStep (-1);       // also refreshes controls/steps/status
        clearDrumSelections();
        clearPcmSelections();
        clearSynthPolySelection();
    }
    refreshParamControls();
    refreshStepButtons();
    updateStatusLabel();
    updateClearLocksEnabled();
}

void SequencerPanel::onParamEdited (int lockableIndex, int value)
{
    if (lockableIndex == kDrumNoteCell || lockableIndex == kDrumVelCell)
    {
        // A focused drum lane's base NOTE/VEL cell -- writes the lane's whole-pattern base value
        // (the same field its own always-visible note/velocity slider reads), not a per-step value.
        const int drumIndex = displayedMelodicTarget - kDrumBaseTargetBase;
        if (drumIndex < 0 || drumIndex >= (int) drumTrackControls.size()
            || drumTrackControls[(size_t) drumIndex] == nullptr)
            return;
        auto& row = *drumTrackControls[(size_t) drumIndex];
        if (lockableIndex == kDrumNoteCell)
            row.note.setValue (juce::jlimit (0, 127, value), juce::dontSendNotification);
        else
            row.baseVelocity = juce::jlimit (1, 127, value);
        refreshDrumBaseCellValues (drumIndex);
        refreshStepButtons();   // keeps the row's own note/velocity sliders in sync with this edit
        return;
    }

    if (lockableIndex < 0)   // a melodic track's step NOTE/GATE/VEL cell, not a real synth param
    {
        auto* step = melodicStepForTarget (displayedMelodicTarget);
        if (step == nullptr)
            return;   // shouldn't happen (the page wouldn't be showing), but guard anyway

        if (lockableIndex == kPcmNoteCell)      step->note        = juce::jlimit (0, 127, value);
        else if (lockableIndex == kPcmGateCell) step->gatePercent = juce::jlimit (1, 100, value);
        else if (lockableIndex == kPcmVelCell)  step->velocity    = juce::jlimit (1, 127, value);
        refreshMelodicStepCellValues (displayedMelodicTarget);
        return;
    }

    const auto& lp = sequence.lockable[(size_t) lockableIndex];

    if (selectedStep < 0)
        casioxw::setBaseValue (sequence, lp.paramId, lp.instance, value);
    else
        casioxw::setStepLock (sequence, selectedStep, lp.paramId, lp.instance, value);

    // Audition immediately only when stopped, or editing the Base sound. A base edit during
    // playback still needs the immediate send (the scheduler's step-to-step dedup won't re-emit an
    // unchanging base). A LOCK edit during playback must NOT audition globally — under look-ahead it
    // would bleed the locked value across the whole pattern; the scheduler plays it when that step
    // next comes round (<= one loop), which is the correct p-lock feel anyway.
    if (! playing || selectedStep < 0)
        sendParamNow (lp.paramId, lp.instance, value);

    refreshParamControls();
    refreshStepButtons();
}

void SequencerPanel::onParamReset (int lockableIndex)
{
    if (lockableIndex < 0)
        return;   // PCM track's step NOTE/GATE/VEL raw cells -- no lock to clear, no reset defined

    const auto& lp = sequence.lockable[(size_t) lockableIndex];
    if (selectedStep < 0)
        return;   // base mode: the cell already IS the base value, nothing to reset it against

    if (casioxw::findStepLock (sequence.steps[(size_t) selectedStep], lp.paramId, lp.instance) == nullptr)
        return;   // unlocked on this step -- already inheriting base, nothing to clear

    casioxw::clearStepLock (sequence, selectedStep, lp.paramId, lp.instance);

    // Audition immediately only when stopped -- same look-ahead-bleed reason as onParamEdited's
    // lock edits: a mid-playback unlock shouldn't jump the whole pattern back to base early.
    if (! playing)
        sendParamNow (lp.paramId, lp.instance, lp.baseValue);

    refreshParamControls();
    refreshStepButtons();
}

void SequencerPanel::refreshParamControls()
{
    const bool pLockMode = editButton.getToggleState();
    const bool synthStepMode = pLockMode && selectedStep >= 0;
    const bool baseMode = ! synthStepMode;

    for (size_t i = 0; i < sequence.lockable.size(); ++i)
    {
        const auto& lp = sequence.lockable[i];

        const int value = baseMode
            ? lp.baseValue
            : casioxw::effectiveParamValue (sequence, selectedStep, lp.paramId, lp.instance)
                  .value_or (lp.baseValue);

        // Inverted (amber) cell == this parameter holds a lock on the selected step.
        const bool locked = synthStepMode
            && casioxw::findStepLock (sequence.steps[(size_t) selectedStep], lp.paramId, lp.instance) != nullptr;

        paramDisplay->setCellState ((int) i, value, locked);
    }
}

void SequencerPanel::seedLockableFromEngine (TrackEngine engine)
{
    sequence.lockable.clear();
    const auto set = resolveEngineLockableSet (engine, codec.model(), currentSoloSynthBlock, currentSoloSynthInstance,
                                                currentHexLayer);
    for (const auto& l : set.lockables)
        sequence.lockable.push_back (casioxw::LockableParam { l.paramId, l.instance, l.base });
    sequence.engineTag = engineTag (engine);
}

void SequencerPanel::applyEngine (TrackEngine newEngine)
{
    currentEngine = newEngine;
    seedLockableFromEngine (currentEngine);
    continuousLockables.clear();   // indices were sized for the OLD engine's lockable count

    // Poly mode is owner-scoped to Hex Layer/Drawbar Organ -- force it off (and collapse/deselect)
    // rather than merely hiding the toggle, so a stale synthPolyMode=true can't survive a round
    // trip through Solo Synth and silently resume scheduling extra voices later.
    if (newEngine == TrackEngine::soloSynth && synthPolyMode)
    {
        synthPolyMode = false;
        synthPolyToggle.setToggleState (false, juce::dontSendNotification);
        synthSubTracksExpanded = false;
        clearSynthPolySelection();
    }

    synthLabel.setText (shortEngineLabel (currentEngine), juce::dontSendNotification);
    engineCombo.setSelectedId ((int) currentEngine + 1, juce::dontSendNotification);

    rebuildSynthParamPages();   // also re-seeds continuousLockables + min/max for the new table
    refreshStepButtons();
    resized();   // hexLayerCombo/soloSynthBlockCombo/soloSynthInstanceCombo/poly visibility depends on currentEngine
}

void SequencerPanel::switchEngine (TrackEngine newEngine)
{
    if (newEngine == currentEngine)
        return;

    // Both playback and a base-value sync read/iterate sequence.lockable on the shared timer;
    // switching its shape out from under either would be a real race, not just a UI glitch.
    if (playing || ! outstandingBaseSync.empty())
    {
        statusLabel.setText ("Stop playback/sync before switching engine", juce::dontSendNotification);
        engineCombo.setSelectedId ((int) currentEngine + 1, juce::dontSendNotification);   // revert
        return;
    }

    applyEngine (newEngine);
    refreshParamControls();
    refreshStepButtons();      // has-locks LEDs: the new engine's paramIds rarely match old locks
    updateStatusLabel();
    updateClearLocksEnabled();
}

void SequencerPanel::setHexLayer (int layer)
{
    if (layer == currentHexLayer || layer < 1 || layer > 6)
        return;

    // Same race as switchEngine() -- both playback and a base-value sync iterate sequence.lockable
    // on the shared timer.
    if (playing || ! outstandingBaseSync.empty())
    {
        statusLabel.setText ("Stop playback/sync before switching layer", juce::dontSendNotification);
        hexLayerCombo.setSelectedId (currentHexLayer, juce::dontSendNotification);   // revert
        return;
    }

    currentHexLayer = layer;
    seedLockableFromEngine (currentEngine);   // only reachable while currentEngine == hexLayer
    continuousLockables.clear();
    rebuildSynthParamPages();
    refreshParamControls();
    refreshStepButtons();
    updateStatusLabel();
    updateClearLocksEnabled();
}

void SequencerPanel::setSoloSynthBlock (const juce::String& block)
{
    if (block == currentSoloSynthBlock || block.isEmpty())
        return;

    if (playing || ! outstandingBaseSync.empty())
    {
        statusLabel.setText ("Stop playback/sync before switching block", juce::dontSendNotification);
        soloSynthBlockCombo.setText (currentSoloSynthBlock, juce::dontSendNotification);   // revert
        return;
    }

    currentSoloSynthBlock = block;

    // Repopulate the instance combo for the new block's instance count/labels (dontSendNotification:
    // clear()'s default async notification would otherwise queue a redundant
    // setSoloSynthInstance() re-fire after the explicit rebuild below already runs synchronously --
    // same fix SoloSynthPanel::blockSelectionChanged() already applies to its own instanceCombo).
    soloSynthInstanceCombo.clear (juce::dontSendNotification);
    if (const auto* rep = firstSoloSynthParamInBlock (codec.model(), currentSoloSynthBlock))
        for (int i = 0; i < rep->instanceCount; ++i)
            soloSynthInstanceCombo.addItem (i < rep->instanceLabels.size() ? rep->instanceLabels[i]
                                                                            : juce::String (i + 1),
                                             i + 1);
    soloSynthInstanceCombo.setSelectedId (1, juce::dontSendNotification);
    currentSoloSynthInstance = 1;

    seedLockableFromEngine (currentEngine);   // only reachable while currentEngine == soloSynth
    continuousLockables.clear();
    rebuildSynthParamPages();
    refreshParamControls();
    refreshStepButtons();
    updateStatusLabel();
    updateClearLocksEnabled();
    resized();   // instance combo's visibility (single- vs multi-instance block) may have changed
}

void SequencerPanel::setSoloSynthInstance (int instance)
{
    if (instance == currentSoloSynthInstance || instance < 1)
        return;

    if (playing || ! outstandingBaseSync.empty())
    {
        statusLabel.setText ("Stop playback/sync before switching instance", juce::dontSendNotification);
        soloSynthInstanceCombo.setSelectedId (currentSoloSynthInstance, juce::dontSendNotification);   // revert
        return;
    }

    currentSoloSynthInstance = instance;
    seedLockableFromEngine (currentEngine);
    continuousLockables.clear();
    rebuildSynthParamPages();
    refreshParamControls();
    refreshStepButtons();
    updateStatusLabel();
    updateClearLocksEnabled();
}

void SequencerPanel::rebuildSynthParamPages()
{
    const auto set = resolveEngineLockableSet (currentEngine, codec.model(), currentSoloSynthBlock,
                                                currentSoloSynthInstance, currentHexLayer);

    std::vector<ParamPageDisplay::Page> pages;
    for (const auto& pageName : set.pageNames)
        pages.push_back ({ pageName, {} });

    const auto& model = codec.model();
    for (size_t i = 0; i < sequence.lockable.size(); ++i)
    {
        const auto& l = set.lockables[i];
        const auto* info = model.find (l.paramId);
        jassert (info != nullptr);   // every lockable id must exist in xwp1.json
        if (info == nullptr)
            continue;

        // Bound randomized locks by the parameter's real range (read from metadata so a
        // differently-ranged lockable randomizes correctly).
        sequence.lockable[i].minValue = info->range.min;
        sequence.lockable[i].maxValue = info->range.max;

        // Continuous params only for Rnd by default — a per-step random filter TYPE or LFO
        // wave is the chaos the owner vetoed; combo/toggle params opt in via the call-out.
        if (casioxw::decideControlKind (*info, l.instance) == casioxw::ControlKind::Slider)
            if (std::find (continuousLockables.begin(), continuousLockables.end(), (int) i) == continuousLockables.end())
                continuousLockables.push_back ((int) i);

        ParamPageDisplay::CellSpec spec;
        spec.info = info;
        spec.instance = l.instance;
        spec.shortName = l.shortName;
        spec.lockableIndex = (int) i;
        pages[(size_t) l.page].cells.push_back (spec);
    }

    if (pages.empty())   // e.g. Drawbar Organ: no lockable params wired up yet
        pages.push_back ({ "NO LOCKS", {} });

    lockablePages = std::move (pages);
    // Force refreshParamDisplayPages() below to rebuild paramDisplay's actual page set even if the
    // freshly-computed target comes out the same as before -- lockablePages just changed (new
    // engine/table), so a stale early-return would leave the OLD engine's pages showing (or, on
    // the very first call from the ctor, leave paramDisplay with NO pages at all: target and
    // displayedMelodicTarget would both start at -1 and look "unchanged"). -2 is never a real
    // target (every real one is >= -1), so it can't collide with "nothing selected" (-1) the way
    // resetting to -1 here did.
    displayedMelodicTarget = -2;
    refreshParamDisplayPages();
}

// Decodes displayedMelodicTarget (see its doc comment in SequencerPanel.h) into a pointer at the
// actual casioxw::Step the glass panel's NOTE/GATE/VEL cells should read/write -- nullptr if
// nothing is currently selected there (shouldn't happen while the page is showing, but every
// caller guards anyway rather than assume).
casioxw::Step* SequencerPanel::melodicStepForTarget (int target)
{
    if (target < 0)
        return nullptr;
    if (target < 10)   // a PCM row's primary voice
    {
        auto& row = *pcmTrackControls[(size_t) target];
        return row.selectedStep >= 0 ? &row.track.steps[(size_t) row.selectedStep] : nullptr;
    }
    if (target < 100)   // a PCM row's poly extra voice
    {
        const size_t row = (size_t) (target - 10) / 4;
        const size_t voice = (size_t) (target - 10) % 4;   // 1..3
        auto& r = *pcmTrackControls[row];
        return r.selectedStep >= 0 ? &r.extraVoices[voice - 1].track.steps[(size_t) r.selectedStep] : nullptr;
    }
    if (target == 100)   // the solo lane's OWN primary voice
        return selectedStep >= 0 ? &sequence.steps[(size_t) selectedStep] : nullptr;
    // The solo lane's own poly extra voice.
    const size_t voice = (size_t) (target - 100);   // 1..3
    return synthPolyStep >= 0 ? &synthExtraVoices[voice - 1].track.steps[(size_t) synthPolyStep] : nullptr;
}

void SequencerPanel::refreshMelodicStepCellValues (int target)
{
    const auto* step = melodicStepForTarget (target);
    if (step == nullptr)
        return;
    paramDisplay->setCellState (kPcmNoteCell, step->note, false);
    paramDisplay->setCellState (kPcmGateCell, step->gatePercent, false);
    paramDisplay->setCellState (kPcmVelCell,  step->velocity, false);
}

void SequencerPanel::refreshDrumBaseCellValues (int drumIndex)
{
    if (drumIndex < 0 || drumIndex >= (int) drumTrackControls.size()
        || drumTrackControls[(size_t) drumIndex] == nullptr)
        return;
    const auto& row = *drumTrackControls[(size_t) drumIndex];
    paramDisplay->setCellState (kDrumNoteCell, (int) row.note.getValue(), false);
    paramDisplay->setCellState (kDrumVelCell,  row.baseVelocity, false);
}

void SequencerPanel::refreshParamDisplayPages()
{
    // The current melodic edit target across every possible source, encoded per
    // displayedMelodicTarget's doc comment. PCM rows (any voice) take priority over the solo
    // lane's poly voice, which in turn takes priority over the solo lane's own primary voice --
    // moot in practice since every selection site clears the others, so at most one of these ever
    // finds anything.
    int target = -1;
    for (size_t i = 0; i < pcmTrackControls.size(); ++i)
    {
        const auto& rowPtr = pcmTrackControls[i];
        if (rowPtr != nullptr && rowPtr->selectedStep >= 0)
        {
            target = rowPtr->selectedVoice == 0 ? (int) i : (10 + (int) i * 4 + rowPtr->selectedVoice);
            break;
        }
    }
    if (target < 0 && synthPolyStep >= 0)
        target = 100 + synthSelectedVoice;
    if (target < 0 && selectedStep >= 0)
        target = 100;   // the solo lane's own primary voice

    // No STEP is selected anywhere -- fall back to whichever track is FOCUSED (see
    // FocusedTrackKind's doc comment; setFocusedTrack() is also called from every step's onClick,
    // so an actual step selection above always matches the same lane and never disagrees with
    // this). soloSynth leaves target at -1 (its existing Base/lockablePages behaviour, unchanged).
    if (target < 0)
    {
        if (focusedTrackKind == FocusedTrackKind::drum && focusedTrackIndex >= 0
            && drumTrackControls[(size_t) focusedTrackIndex] != nullptr)
            target = kDrumBaseTargetBase + focusedTrackIndex;
        else if (focusedTrackKind == FocusedTrackKind::pcm && focusedTrackIndex >= 0
                 && pcmTrackControls[(size_t) focusedTrackIndex] != nullptr)
            target = kPcmBaseTargetBase + focusedTrackIndex;
    }

    if (target == displayedMelodicTarget)
    {
        if (target >= kPcmBaseTargetBase)
            {}   // the blank PCM-focus placeholder page has no cells to refresh
        else if (target >= kDrumBaseTargetBase)
            refreshDrumBaseCellValues (target - kDrumBaseTargetBase);
        else if (target >= 0)
            refreshMelodicStepCellValues (target);   // same target -- but the selected step may differ
        return;
    }

    displayedMelodicTarget = target;

    if (target < 0)   // Base mode: nothing selected/focused anywhere lockable -- this engine's own pages
    {
        paramDisplay->setPages (lockablePages);
        refreshParamControls();   // freshly-set cells default to range min -- populate real values
        return;
    }

    if (target >= kPcmBaseTargetBase)   // a focused PCM lane with no step selected -- no base values
    {
        const int pcmIndex = target - kPcmBaseTargetBase;
        paramDisplay->setPages ({ ParamPageDisplay::Page { kPcmTracks[(size_t) pcmIndex].label, {} } });
        return;
    }

    if (target >= kDrumBaseTargetBase)   // a focused drum lane with no step selected -- NOTE/VEL base
    {
        const int drumIndex = target - kDrumBaseTargetBase;
        paramDisplay->setPages ({ buildDrumBasePage (kDrumTracks[(size_t) drumIndex].label) });
        refreshDrumBaseCellValues (drumIndex);
        return;
    }

    if (target == 100)
    {
        // The solo lane's own primary voice: a NOTE/GATE/VEL page, THEN this engine's own p-lock
        // pages right after it -- unlike PCM/poly voices (no lockable params of their own), the
        // solo lane still has a real lockable set (FILT/OSC/etc) to page through.
        std::vector<ParamPageDisplay::Page> pages;
        pages.push_back (buildNoteGateVelPage ("NOTE"));
        for (const auto& p : lockablePages)
            pages.push_back (p);
        paramDisplay->setPages (std::move (pages));
        refreshMelodicStepCellValues (target);
        refreshParamControls();
        return;
    }

    static const char* const kPcmTrackNames[] = { "BASS", "SOLO 1", "SOLO 2", "CHORDS" };
    juce::String pageName;
    if (target < 10)
        pageName = kPcmTrackNames[(size_t) target];
    else if (target < 100)
        pageName = juce::String (kPcmTrackNames[(size_t) (target - 10) / 4]) + " V"
                 + juce::String ((target - 10) % 4);
    else
        pageName = "SYNTH V" + juce::String (target - 100);

    std::vector<ParamPageDisplay::Page> pages;
    pages.push_back (buildNoteGateVelPage (pageName));
    paramDisplay->setPages (std::move (pages));
    refreshMelodicStepCellValues (target);
}

void SequencerPanel::refreshStepButtons()
{
    const bool pLockMode = editButton.getToggleState();

    for (int i = 0; i < 16; ++i)
    {
        auto& btn = stepControls[(size_t) i]->select;
        const auto& step = sequence.steps[(size_t) i];
        btn.setToggleState (step.enabled, juce::dontSendNotification);
        btn.setLockState (! step.locks.empty(), pLockMode && i == selectedStep);

        bool synthChord = false;
        if (synthPolyMode)
            for (const auto& voice : synthExtraVoices)
                if (voice.track.steps[(size_t) i].enabled)
                    synthChord = true;
        btn.setChordState (synthChord);
    }
    baseButton.setColour (juce::TextButton::buttonColourId,
                          selectedStep < 0 ? kSelectedColour : kIdleColour);

    if (synthPolyMode)
        for (size_t v = 0; v < synthExtraVoices.size(); ++v)
            for (int i = 0; i < 16; ++i)
            {
                auto& btn = synthExtraVoices[v].steps[(size_t) i];
                btn.setToggleState (synthExtraVoices[v].track.steps[(size_t) i].enabled, juce::dontSendNotification);
                btn.setLockState (false, pLockMode && synthPolyStep == i && synthSelectedVoice == (int) v + 1);
            }

    for (auto& rowPtr : drumTrackControls)
    {
        if (rowPtr == nullptr)
            continue;

        auto& row = *rowPtr;
        const bool drumStepMode = pLockMode && row.selectedStep >= 0;
        int velocity = row.baseVelocity;
        bool locked = false;
        if (drumStepMode)
            if (const auto v = row.velocityLocks[(size_t) row.selectedStep])
            {
                velocity = *v;
                locked = true;
            }

        row.velocity.setValue ((double) velocity, juce::dontSendNotification);
        if (drumStepMode)
            row.velocityMarker.setText (locked ? "LOCKED" : "inherit", juce::dontSendNotification);
        else
            row.velocityMarker.setText ("base", juce::dontSendNotification);
        row.velocityMarker.setColour (juce::Label::textColourId, locked ? kSelectedColour : EditorColours::textMuted);

        for (int i = 0; i < 16; ++i)
        {
            auto& btn = row.steps[(size_t) i];
            btn.setLockState (row.velocityLocks[(size_t) i].has_value(),
                              pLockMode && i == row.selectedStep);
        }
    }

    for (auto& rowPtr : pcmTrackControls)
    {
        if (rowPtr == nullptr)
            continue;

        auto& row = *rowPtr;
        for (int i = 0; i < 16; ++i)
        {
            auto& btn = row.steps[(size_t) i];
            btn.setToggleState (row.track.steps[(size_t) i].enabled, juce::dontSendNotification);
            // No lock LED here -- a PCM step's note/gate/vel are always-defined, never "unlocked";
            // `selected` is the only state worth showing (which step the screen is editing).
            btn.setLockState (false, pLockMode && i == row.selectedStep && row.selectedVoice == 0);

            bool chord = false;
            if (row.polyMode)
                for (const auto& voice : row.extraVoices)
                    if (voice.track.steps[(size_t) i].enabled)
                        chord = true;
            btn.setChordState (chord);
        }

        if (row.polyMode)
            for (size_t v = 0; v < row.extraVoices.size(); ++v)
                for (int i = 0; i < 16; ++i)
                {
                    auto& btn = row.extraVoices[v].steps[(size_t) i];
                    btn.setToggleState (row.extraVoices[v].track.steps[(size_t) i].enabled, juce::dontSendNotification);
                    btn.setLockState (false, pLockMode && i == row.selectedStep && row.selectedVoice == (int) v + 1);
                }
    }

    refreshParamDisplayPages();   // keep the screen in sync with whichever lane owns the selection
}

void SequencerPanel::updateStatusLabel()
{
    // The edit-target readout lives in the parameter display's header (the "screen"), like the
    // mode/pattern line on a hardware unit. The footer statusLabel is file messages only.
    juce::String text;
    if (! editButton.getToggleState())
        text = "GRID  BASE SOUND";
    else if (selectedStep < 0)
    {
        static const char* const kPcmTrackNames[] = { "BASS", "SOLO 1", "SOLO 2", "CHORDS" };
        bool hasTarget = false;
        for (size_t i = 0; i < pcmTrackControls.size(); ++i)
        {
            const auto& row = pcmTrackControls[i];
            if (row != nullptr && row->selectedStep >= 0)
            {
                text = juce::String (kPcmTrackNames[i]) + "  STEP "
                     + juce::String (row->selectedStep + 1).paddedLeft ('0', 2);
                hasTarget = true;
                break;
            }
        }
        for (size_t i = 0; ! hasTarget && i < drumTrackControls.size(); ++i)
        {
            const auto& row = drumTrackControls[i];
            if (row != nullptr && row->selectedStep >= 0)
            {
                text = "P-LOCK  DRUM " + juce::String ((int) i + 1)
                     + "  STEP " + juce::String (row->selectedStep + 1).paddedLeft ('0', 2) + " VEL";
                hasTarget = true;
            }
        }
        if (! hasTarget)
        {
            if (focusedTrackKind == FocusedTrackKind::drum && focusedTrackIndex >= 0)
                text = "P-LOCK  DRUM " + juce::String (focusedTrackIndex + 1) + "  BASE";
            else if (focusedTrackKind == FocusedTrackKind::pcm && focusedTrackIndex >= 0)
                text = juce::String (kPcmTrackNames[(size_t) focusedTrackIndex]) + "  BASE";
            else
                text = "P-LOCK  BASE SOUND";
        }
    }
    else
        text = "P-LOCK  STEP " + juce::String (selectedStep + 1).paddedLeft ('0', 2);

    paramDisplay->setStatus (text);
}

void SequencerPanel::setShowingArranger (bool shouldShow)
{
    if (showingArranger == shouldShow)
        return;

    showingArranger = shouldShow;
    arrangerModeButton.setButtonText (showingArranger ? "Editor" : "Arranger");
    arrangerModeButton.setToggleState (showingArranger, juce::dontSendNotification);

    // The rest of the transport toolbar is step-editor-specific (tempo/rate/channel belong to
    // `sequence`, STEP/P-LOCK/Clear/Save/Load act on the live pattern) and sits ABOVE arrangerPanel's
    // own bounds, so z-order alone can't hide it -- explicitly hide it in Arranger mode so nothing
    // implies it still applies, and Save/Load/Clear can't be mistaken for arranger actions.
    juce::Component* const editorOnlyToolbarWidgets[] = {
        &tempoLabel, &tempoSlider, &rateLabel, &rateCombo, &channelLabel, &channelSlider,
        &stepModeButton, &editButton, &clearLocksButton, &clearAllButton,
        &saveButton, &loadButton, &sequenceDirButton
    };
    for (auto* c : editorOnlyToolbarWidgets)
        c->setVisible (! showingArranger);

    arrangerPanel->setVisible (showingArranger);
    if (showingArranger)
        arrangerPanel->toFront (false);   // everything else was added as a child EARLIER in the ctor,
                                          // so without this it would paint UNDER those widgets, not over

    resized();
    repaint();
}

void SequencerPanel::play()
{
    if (playing)
        return;

    // The step editor and the arranger share one MidiIO output -- only one transport may schedule
    // at a time (see ArrangerPanel::beforePlay for the reverse direction).
    if (arrangerPanel != nullptr && arrangerPanel->isPlaying())
        arrangerPanel->stop();

    if (! midiIO.isOutputOpen())
    {
        statusLabel.setText ("Not connected - open a MIDI output on the Solo Synth tab first",
                              juce::dontSendNotification);
        return;
    }

    playing = true;
    outstandingBaseSync.clear();    // an in-flight base sync yields the timer to the feeder
    midiIO.startPlaybackThread();   // JUCE's high-res output thread dispatches the timestamps

    transportStartMs = (double) juce::Time::getMillisecondCounter() + kStartLeadMs;
    nextStepStartMs  = 0.0;
    nextStepIndex    = 0;
    // The device is already sitting at every param's base value here (a Sync pulls the device's
    // values into base, and stop() resets the device back to base), so the first step must NOT
    // re-dump the whole baseline -- that ~24-param NRPN/SysEx burst, fired at the same instant as
    // the first note-ons, is what made the XW-P1 drop notes and respond late on step 1. The
    // kPrevStepBaseline sentinel makes scheduleStep() emit only the params step 0 actually locks
    // away from base (usually none), so nothing floods the synth and no pre-roll gap is needed.
    prevStepIndex = casioxw::kPrevStepBaseline;
    scheduledPlayheadMarks.clear(); // playhead follows this fresh schedule (see updatePlayheadStep)
    playheadStep = -1;              // hidden until step 0's boundary is actually reached

    playStopButton.setButtonText ("Stop");
    playStopButton.setColour (juce::TextButton::buttonColourId, EditorColours::green);
    playStopButton.setColour (juce::TextButton::textColourOffId, EditorColours::base03);
    // Prime the whole first loop up front (floored at kStartPrimeFloorMs for fast tempos) so the
    // startup message-thread spike can't delay the feeder into a rushed catch-up burst.
    feedLookahead (juce::jmax (kStartPrimeFloorMs, 16.0 * casioxw::stepIntervalMs (sequence)));
    updatePlayheadStep();
    startTimer (kSchedulerTickMs);
}

void SequencerPanel::stop()
{
    if (! playing)
        return;

    stopTimer();
    playing = false;
    scheduledPlayheadMarks.clear();   // drop the schedule the playhead was following
    playStopButton.setButtonText ("Play");
    playStopButton.removeColour (juce::TextButton::buttonColourId);
    playStopButton.removeColour (juce::TextButton::textColourOffId);

    // Discard everything still queued for future dispatch (incl. not-yet-fired note-offs), then
    // release + reset explicitly since those dropped note-offs won't arrive on their own.
    midiIO.stopPlaybackThread();
    midiIO.sendAllNotesOff (sequence.channel);
    for (const auto& row : drumTrackControls)
        if (row != nullptr)
            midiIO.sendAllNotesOff (juce::jlimit (1, 16, row->channel.getSelectedId()));
    for (const auto& row : pcmTrackControls)
        if (row != nullptr)
            midiIO.sendAllNotesOff (juce::jlimit (1, 16, row->track.channel));

    // Reset every parameter to its base so a p-lock can't leave the filter stuck closed/resonant.
    for (const auto& lp : sequence.lockable)
        sendParamNow (lp.paramId, lp.instance, lp.baseValue);

    updatePlayheadStep();
    updateStatusLabel();
}

void SequencerPanel::feedLookahead (double lookaheadMs)
{
    // Fill everything whose start time falls within [now, now + lookahead). The feeder only has to
    // stay ahead of the horizon; the output thread delivers each event at its exact timestamp, so
    // jitter in this (message-thread) callback doesn't move the notes.
    const double now     = (double) juce::Time::getMillisecondCounter();
    const double horizon = now + lookaheadMs;

    while (transportStartMs + nextStepStartMs < horizon)
    {
        juce::MidiBuffer buffer;
        const double stepMs = casioxw::stepIntervalMs (sequence);
        const double drumGateMs = juce::jmax (1.0, stepMs * 0.5);
        if (! muteSynthButton.getToggleState())
            for (const auto& e : casioxw::scheduleStep (sequence, nextStepIndex, prevStepIndex, nextStepStartMs))
            {
                const int samplePos = (int) std::llround (e.timeMs);   // 1 sample == 1 ms (kScheduleSampleRate)
                switch (e.type)
                {
                    case casioxw::ScheduledEvent::Type::noteOn:
                        buffer.addEvent (juce::MidiMessage::noteOn (e.channel, e.note, (juce::uint8) e.velocity), samplePos);
                        break;
                    case casioxw::ScheduledEvent::Type::noteOff:
                        buffer.addEvent (juce::MidiMessage::noteOff (e.channel, e.note), samplePos);
                        break;
                    case casioxw::ScheduledEvent::Type::paramChange:
                        for (const auto& m : paramMessages (e.paramId, e.instance, e.value, sequence.channel))
                            buffer.addEvent (m, samplePos);
                        break;
                }
            }

        // Solo lane poly voices (Hex Layer/Drawbar Organ only -- see synthPolyMode's doc comment):
        // additional SIMULTANEOUS notes on the same part/channel, not independent lanes, so they
        // share the mute button and the primary voice's own scheduleStep() path -- each voice's
        // `lockable` is empty, so this only ever emits noteOn/noteOff, never paramChange (p-locks
        // stay on the shared `sequence.lockable` table, sent once above via the primary voice).
        if (synthPolyMode && ! muteSynthButton.getToggleState())
            for (auto& voice : synthExtraVoices)
            {
                voice.track.channel      = sequence.channel;
                voice.track.tempoBpm     = sequence.tempoBpm;
                voice.track.stepsPerBeat = sequence.stepsPerBeat;

                for (const auto& e : casioxw::scheduleStep (voice.track, nextStepIndex, prevStepIndex, nextStepStartMs))
                {
                    const int samplePos = (int) std::llround (e.timeMs);
                    if (e.type == casioxw::ScheduledEvent::Type::noteOn)
                        buffer.addEvent (juce::MidiMessage::noteOn (e.channel, e.note, (juce::uint8) e.velocity), samplePos);
                    else if (e.type == casioxw::ScheduledEvent::Type::noteOff)
                        buffer.addEvent (juce::MidiMessage::noteOff (e.channel, e.note), samplePos);
                }
            }

        for (const auto& row : drumTrackControls)
        {
            if (row == nullptr || row->mute.getToggleState() || ! row->steps[(size_t) nextStepIndex].getToggleState())
                continue;

            const int note = juce::jlimit (0, 127, (int) row->note.getValue());
            int velocity = row->baseVelocity;
            if (const auto locked = row->velocityLocks[(size_t) nextStepIndex])
                velocity = *locked;
            velocity = juce::jlimit (1, 127, velocity);
            const int channel = juce::jlimit (1, 16, row->channel.getSelectedId() > 0
                                                         ? row->channel.getSelectedId()
                                                         : sequence.channel);
            const int onPos = (int) std::llround (nextStepStartMs);
            const int offPos = (int) std::llround (nextStepStartMs + drumGateMs);
            buffer.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) velocity), onPos);
            buffer.addEvent (juce::MidiMessage::noteOff (channel, note), offPos);
        }

        // PCM tracks (Bass/Solo 1/Solo 2/Chords) are melodic like the Solo Synth track, so they go
        // through the exact same scheduleStep()->ScheduledEvent path — no bespoke event-building.
        // tempoBpm/stepsPerBeat are mirrored from the main sequence every tick rather than edited
        // independently, so all melodic tracks + drums share one clock even across live tempo/rate
        // changes. Each track's own `lockable` is empty in this pass, so scheduleStep only ever
        // emits noteOn/noteOff for it (no paramChange p-locks yet - a follow-up chunk).
        for (const auto& row : pcmTrackControls)
        {
            if (row == nullptr || row->mute.getToggleState())
                continue;

            row->track.tempoBpm     = sequence.tempoBpm;
            row->track.stepsPerBeat = sequence.stepsPerBeat;

            for (const auto& e : casioxw::scheduleStep (row->track, nextStepIndex, prevStepIndex, nextStepStartMs))
            {
                const int samplePos = (int) std::llround (e.timeMs);
                switch (e.type)
                {
                    case casioxw::ScheduledEvent::Type::noteOn:
                        buffer.addEvent (juce::MidiMessage::noteOn (e.channel, e.note, (juce::uint8) e.velocity), samplePos);
                        break;
                    case casioxw::ScheduledEvent::Type::noteOff:
                        buffer.addEvent (juce::MidiMessage::noteOff (e.channel, e.note), samplePos);
                        break;
                    case casioxw::ScheduledEvent::Type::paramChange:
                        break;   // no lockable params on a PCM track yet
                }
            }

            // Poly extra voices (Chords only, in practice -- see PcmTrackControl::polyCapable):
            // additional simultaneous notes on the SAME channel, sharing this row's own mute.
            if (row->polyMode)
                for (auto& voice : row->extraVoices)
                {
                    voice.track.channel      = row->track.channel;
                    voice.track.tempoBpm     = sequence.tempoBpm;
                    voice.track.stepsPerBeat = sequence.stepsPerBeat;

                    for (const auto& e : casioxw::scheduleStep (voice.track, nextStepIndex, prevStepIndex, nextStepStartMs))
                    {
                        const int samplePos = (int) std::llround (e.timeMs);
                        if (e.type == casioxw::ScheduledEvent::Type::noteOn)
                            buffer.addEvent (juce::MidiMessage::noteOn (e.channel, e.note, (juce::uint8) e.velocity), samplePos);
                        else if (e.type == casioxw::ScheduledEvent::Type::noteOff)
                            buffer.addEvent (juce::MidiMessage::noteOff (e.channel, e.note), samplePos);
                    }
                }
        }

        if (! buffer.isEmpty())
            midiIO.scheduleBlock (buffer, transportStartMs, kScheduleSampleRate);

        // Record this step's exact absolute start so the playhead can follow the real schedule
        // (see scheduledPlayheadMarks) rather than a tempo-sensitive division.
        scheduledPlayheadMarks.push_back ({ transportStartMs + nextStepStartMs, nextStepIndex });

        prevStepIndex   = nextStepIndex;
        nextStepIndex   = (nextStepIndex + 1) % 16;
        nextStepStartMs += casioxw::stepIntervalMs (sequence);   // per-step read = live tempo/rate changes
    }
}

void SequencerPanel::timerCallback()
{
    if (playing)
    {
        feedLookahead (kLookaheadMs);   // steady-state horizon (keeps live edits responsive)
        updatePlayheadStep();
        return;
    }

    // Stopped + timer running == a base-value sync is in flight; poll the receive queue.
    for (auto& frame : midiIO.drainReceived())
    {
        const auto d = codec.decode (frame);
        if (! d.ok || d.ambiguous)
            continue;

        const auto it = outstandingBaseSync.find (d.paramId + "#" + juce::String (d.instance));
        if (it == outstandingBaseSync.end())
            continue;

        const auto& lp = sequence.lockable[(size_t) it->second];
        casioxw::setBaseValue (sequence, lp.paramId, lp.instance, d.value);
        outstandingBaseSync.erase (it);
    }

    constexpr juce::uint32 kBaseSyncTimeoutMs = 2000;
    if (outstandingBaseSync.empty())
    {
        stopTimer();
        refreshParamControls();
        statusLabel.setText ("Base sound synced from synth", juce::dontSendNotification);
    }
    else if (juce::Time::getMillisecondCounter() - baseSyncStartedMs > kBaseSyncTimeoutMs)
    {
        stopTimer();
        refreshParamControls();   // adopt whatever did arrive
        statusLabel.setText (juce::String ((int) outstandingBaseSync.size())
                                 + " base param(s) did not reply (timeout)",
                             juce::dontSendNotification);
        outstandingBaseSync.clear();
    }
}

void SequencerPanel::syncBaseValuesFromSynth()
{
    if (playing)
    {
        statusLabel.setText ("Stop playback before syncing base values", juce::dontSendNotification);
        return;
    }
    if (! midiIO.isOutputOpen() || ! midiIO.isInputOpen())
    {
        statusLabel.setText ("Not connected - open MIDI devices on the Solo Synth tab first",
                             juce::dontSendNotification);
        return;
    }

    outstandingBaseSync.clear();
    for (size_t i = 0; i < sequence.lockable.size(); ++i)
    {
        const auto& lp = sequence.lockable[i];
        midiIO.sendFrame (casioxw::MidiIO::syncRequest (codec, lp.paramId, lp.instance));
        outstandingBaseSync[lp.paramId + "#" + juce::String (lp.instance)] = (int) i;
    }

    if (outstandingBaseSync.empty())
        return;

    statusLabel.setText ("Syncing " + juce::String ((int) outstandingBaseSync.size())
                             + " base value(s) from synth...",
                         juce::dontSendNotification);
    baseSyncStartedMs = juce::Time::getMillisecondCounter();
    startTimerHz (20);
}

void SequencerPanel::updatePlayheadStep()
{
    // Follow the audio's own schedule: advance through every step boundary whose start time has
    // already passed, landing on the latest one. This stays locked to the note-ons the feeder
    // queued even when the tempo was dragged mid-playback — unlike dividing elapsed time by the
    // current stepMs, which mis-counts the pre-change span and drifts the highlight off the note.
    int nextPlayhead = playing ? playheadStep : -1;
    if (playing)
    {
        const double now = (double) juce::Time::getMillisecondCounter();
        while (! scheduledPlayheadMarks.empty() && scheduledPlayheadMarks.front().first <= now)
        {
            nextPlayhead = scheduledPlayheadMarks.front().second;
            scheduledPlayheadMarks.pop_front();
        }
    }

    if (nextPlayhead == playheadStep)
        return;

    playheadStep = nextPlayhead;
    repaint (playheadLaneBounds);
}

void SequencerPanel::resized()
{
    auto bounds = getLocalBounds().reduced (8);

    // ---- footer: file save/load messages, pinned to the bottom -----------------------------
    statusLabel.setBounds (bounds.removeFromBottom (kFooterHeight));
    bounds.removeFromBottom (4);

    // ---- transport toolbar: global controls only, in a wrapping flow -----------------------
    {
        struct Item { juce::Component* c; int w; int gapAfter; };
        const Item items[] = {
            { &playStopButton, 72, 12 },
            { &arrangerModeButton, 90, 20 },
            { &tempoLabel, 34, 2 }, { &tempoSlider, 150, 12 },
            { &rateLabel, 38, 2 }, { &rateCombo, 74, 12 },
            { &channelLabel, 26, 2 }, { &channelSlider, 118, 20 },
            { &stepModeButton, 60, 2 }, { &editButton, 74, 12 },
            { &clearLocksButton, 96, 6 }, { &clearAllButton, 84, 20 },
            { &saveButton, 58, 4 }, { &loadButton, 58, 4 }, { &sequenceDirButton, 70, 0 },
        };
        int x = bounds.getX();
        int y = bounds.getY();
        for (const auto& item : items)
        {
            if (x + item.w > bounds.getRight())
            {
                x = bounds.getX();
                y += kToolbarRowHeight;
            }
            item.c->setBounds (x, y, item.w, kToolbarRowHeight - 4);
            x += item.w + item.gapAfter;
        }
        bounds.removeFromTop ((y + kToolbarRowHeight) - bounds.getY() + 6);
    }

    if (showingArranger)
    {
        arrangerPanel->setBounds (bounds);
        return;   // the step-editor's own cards/grid below are hidden while in Arranger mode
    }

    const int cardX = bounds.getX() + kStepGridWidth + kSectionGap + kLaneLabelWidth + kSectionGap;

    // ---- drum section ----------------------------------------------------------------------
    auto drumHeader = bounds.removeFromTop (20);
    drumTracksLabel.setBounds (drumHeader.removeFromLeft (140));
    bounds.removeFromTop (4);

    const int playheadTop = bounds.getY();
    const int drumTop = bounds.getY();
    for (const auto& row : drumTrackControls)
    {
        if (row == nullptr)
            continue;
        auto r = bounds.removeFromTop (kDrumTrackRowHeight);
        const int rowLeft = r.getX();
        auto stepCells = r.removeFromLeft (kStepGridWidth);
        r.removeFromLeft (kSectionGap);
        const auto labelArea = r.removeFromLeft (kLaneLabelWidth);
        row->trackLabel.setBounds (labelArea);
        // Steps + label only -- NOT the mute/channel/note/velocity card to the right (owner: the
        // focus wash spilling onto the card "looks weird", live-flagged against the solo lane).
        row->rowBounds = juce::Rectangle<int> (rowLeft, r.getY(), labelArea.getRight() - rowLeft, r.getHeight());
        r.removeFromLeft (kSectionGap);

        auto controls = r.removeFromLeft (kCardWidth).reduced (8, 2);
        row->mute.setBounds (controls.removeFromLeft (46).withSizeKeepingCentre (46, 24));
        controls.removeFromLeft (6);
        row->channel.setBounds (controls.removeFromLeft (58).withSizeKeepingCentre (58, 24));
        controls.removeFromLeft (8);
        auto noteVel = controls;
        row->note.setBounds (noteVel.removeFromTop (23));
        noteVel.removeFromTop (2);
        auto velRow = noteVel.removeFromTop (23);
        row->velocityMarker.setBounds (velRow.removeFromRight (58));
        row->velocity.setBounds (velRow);

        for (auto& b : row->steps)
        {
            auto cell = stepCells.removeFromLeft (kStepWidth);
            b.setBounds (cell.withSizeKeepingCentre (kStepWidth - 6, kDrumKeyHeight));
        }

        bounds.removeFromTop (2);
    }
    drumCardBounds = juce::Rectangle<int> (cardX, drumTop - 2, kCardWidth, bounds.getY() - drumTop + 2);

    bounds.removeFromTop (10);

    // ---- PCM-track section: own lane per track (label/mute/channel + 16 step keys), same shape
    // as a drum row -- note/gate/velocity for the selected step live in the screen, not here. ----
    auto pcmHeader = bounds.removeFromTop (20);
    pcmTracksLabel.setBounds (pcmHeader.removeFromLeft (140));
    bounds.removeFromTop (4);

    const int pcmTop = bounds.getY();
    for (const auto& row : pcmTrackControls)
    {
        if (row == nullptr)
            continue;
        auto r = bounds.removeFromTop (kPcmTrackRowHeight);
        const int rowLeft = r.getX();
        auto stepCells = r.removeFromLeft (kStepGridWidth);
        r.removeFromLeft (kSectionGap);
        const auto labelArea = r.removeFromLeft (kLaneLabelWidth);
        row->trackLabel.setBounds (labelArea);
        // Steps + label only -- NOT the mute/channel/poly card to the right (see the drum row's
        // identical comment above).
        row->rowBounds = juce::Rectangle<int> (rowLeft, r.getY(), labelArea.getRight() - rowLeft, r.getHeight());
        r.removeFromLeft (kSectionGap);

        auto controls = r.removeFromLeft (kCardWidth).reduced (8, 2);
        row->mute.setBounds (controls.removeFromLeft (46).withSizeKeepingCentre (46, 24));
        controls.removeFromLeft (6);
        row->channel.setBounds (controls.removeFromLeft (58).withSizeKeepingCentre (58, 24));
        if (row->polyCapable)
        {
            controls.removeFromLeft (10);
            row->polyToggle.setBounds (controls.removeFromLeft (54).withSizeKeepingCentre (54, 24));
            controls.removeFromLeft (4);
            row->subTrackArrow.setBounds (controls.removeFromLeft (26).withSizeKeepingCentre (26, 24));
        }
        else
        {
            row->polyToggle.setBounds (0, 0, 0, 0);
            row->subTrackArrow.setBounds (0, 0, 0, 0);
        }

        for (auto& b : row->steps)
        {
            auto cell = stepCells.removeFromLeft (kStepWidth);
            b.setBounds (cell.withSizeKeepingCentre (kStepWidth - 6, kDrumKeyHeight));
        }

        bounds.removeFromTop (2);

        // Poly sub-track rows: only actually laid out (consuming real space) while poly mode is
        // on AND expanded -- see setSize()'s kPolyReserve comment for why collapsed/mono states
        // are safe to just leave the reserved space unused rather than shrinking the window.
        if (row->polyMode && row->subTracksExpanded)
        {
            for (auto& voice : row->extraVoices)
            {
                auto vr = bounds.removeFromTop (kPolyVoiceRowHeight);
                auto vStepCells = vr.removeFromLeft (kStepGridWidth);
                for (auto& b : voice.steps)
                {
                    auto cell = vStepCells.removeFromLeft (kStepWidth);
                    b.setBounds (cell.withSizeKeepingCentre (kStepWidth - 6, kPolyVoiceRowHeight - 6));
                }
                bounds.removeFromTop (kPolySubRowGap);
            }
        }
        else
        {
            for (auto& voice : row->extraVoices)
                for (auto& b : voice.steps)
                    b.setBounds (0, 0, 0, 0);
        }
    }
    pcmCardBounds = juce::Rectangle<int> (cardX, pcmTop - 2, kCardWidth, bounds.getY() - pcmTop + 2);

    bounds.removeFromTop (10);

    // ---- synth section ---------------------------------------------------------------------
    // synthLabel no longer lives in this header row -- it's positioned below, in the lane-label-
    // gutter column (same place as a DrumTrackControl/PcmTrackControl row's trackLabel). engineCombo
    // already displays the same "Solo Synth"/"Hex Layer"/"Drawbar Organ" text on its own, so freeing
    // this space isn't a loss of information, just a redundant label removed.
    auto synthHeader = bounds.removeFromTop (20);
    engineCombo.setBounds (synthHeader.removeFromLeft (140).reduced (2, 1));
    synthHeader.removeFromLeft (4);
    if (currentEngine == TrackEngine::hexLayer)
    {
        hexLayerCombo.setBounds (synthHeader.removeFromLeft (90).reduced (2, 1));
        synthHeader.removeFromLeft (4);
    }
    else
    {
        hexLayerCombo.setBounds (0, 0, 0, 0);
    }
    if (currentEngine == TrackEngine::soloSynth)
    {
        soloSynthBlockCombo.setBounds (synthHeader.removeFromLeft (110).reduced (2, 1));
        synthHeader.removeFromLeft (4);
        if (soloSynthInstanceCombo.getNumItems() > 1)
        {
            soloSynthInstanceCombo.setBounds (synthHeader.removeFromLeft (90).reduced (2, 1));
            synthHeader.removeFromLeft (4);
        }
        else
        {
            soloSynthInstanceCombo.setBounds (0, 0, 0, 0);
        }
    }
    else
    {
        soloSynthBlockCombo.setBounds (0, 0, 0, 0);
        soloSynthInstanceCombo.setBounds (0, 0, 0, 0);
    }
    // Poly mode is owner-scoped to Hex Layer/Drawbar Organ -- Solo Synth never shows it (forced
    // off in applyEngine() too, not just hidden here).
    if (currentEngine != TrackEngine::soloSynth)
    {
        synthPolyToggle.setBounds (synthHeader.removeFromLeft (54).reduced (2, 1));
        synthHeader.removeFromLeft (4);
        synthSubTrackArrow.setBounds (synthHeader.removeFromLeft (26).reduced (2, 1));
        synthHeader.removeFromLeft (4);
    }
    else
    {
        synthPolyToggle.setBounds (0, 0, 0, 0);
        synthSubTrackArrow.setBounds (0, 0, 0, 0);
    }
    bounds.removeFromTop (4);

    // synthSection's height reserves room for the solo lane's poly sub-track rows (below the
    // primary note/gate/velocity columns) only while actually poly+expanded -- see setSize()'s
    // kPolyReserve comment for why a collapsed/mono state safely leaves that space unused rather
    // than needing a live window resize.
    const int synthPolyRowsHeight = (synthPolyMode && synthSubTracksExpanded)
        ? (int) synthExtraVoices.size() * (kPolyVoiceRowHeight + kPolySubRowGap) : 0;
    auto synthSection = bounds.removeFromTop (juce::jmax (kStepColumnHeight + synthPolyRowsHeight, kSynthSectionHeight));
    const int playheadBottom = synthSection.getY() + kSynthStepTopInset + kStepColumnHeight;

    auto stepCols = synthSection.removeFromLeft (kStepGridWidth);
    const int gridX = stepCols.getX();
    stepCols.removeFromTop (kSynthStepTopInset);
    auto trigRow = stepCols.removeFromTop (kStepColumnHeight);
    // Poly sub-track rows sit directly under the select-key row now (not bottom-anchored) --
    // that row is the column's only other content once note/gate/vel aren't always-visible knobs
    // here anymore. See setSize()'s kPolyReserve comment for why a collapsed/mono state is safe to
    // just leave the space below unused rather than needing a live window resize. Uses
    // kSynthTrigToPolyGap, not the standard "2", because the trig row's cell has no padding of
    // its own to contribute (see that constant's doc comment) -- a flat "2" here left visibly
    // less whitespace before the first sub-row than between sub-rows themselves, an inconsistent
    // gap the owner flagged live.
    if (synthPolyRowsHeight > 0)
        stepCols.removeFromTop (kSynthTrigToPolyGap);
    auto synthPolyRowsArea = synthPolyRowsHeight > 0 ? stepCols.removeFromTop (synthPolyRowsHeight)
                                                      : juce::Rectangle<int>();
    synthSection.removeFromLeft (kSectionGap);

    // Lane label gutter: same column width the drum/PCM cards feed cardX from, but now actually
    // POPULATED by synthLabel (owner: the clickable engine-name label belongs in the same place as
    // every drum/PCM track label, not up in the header) -- aligned to trigRow's Y/height, not the
    // whole (much taller, poly-inclusive) synthSection, so it sits at the same visual height as the
    // step keys rather than stretching down over the poly sub-rows.
    auto synthLabelGutter = synthSection.removeFromLeft (kLaneLabelWidth);
    synthLabel.setBounds (synthLabelGutter.withY (trigRow.getY()).withHeight (trigRow.getHeight()));
    synthSection.removeFromLeft (kSectionGap);

    auto card = synthSection.removeFromLeft (kCardWidth);
    auto cardInner = card.reduced (8);
    auto headerRow = cardInner.removeFromTop (24);

    shiftRightButton.setBounds (headerRow.removeFromRight (28));
    headerRow.removeFromRight (2);
    shiftLeftButton.setBounds (headerRow.removeFromRight (28));
    headerRow.removeFromRight (6);
    rndOptionsButton.setBounds (headerRow.removeFromRight (22));   // reads as one "Rnd ▾" pair
    randomizeButton.setBounds (headerRow.removeFromRight (42));
    headerRow.removeFromRight (6);
    muteSynthButton.setBounds (headerRow.removeFromRight (50));
    headerRow.removeFromRight (6);
    syncBaseButton.setBounds (headerRow.removeFromRight (50));
    headerRow.removeFromRight (6);
    baseButton.setBounds (headerRow.removeFromRight (50));

    cardInner.removeFromTop (6);
    paramDisplay->setBounds (cardInner);
    paramDisplay->setVisible (true);
    synthCardBounds = card;
    // The solo lane's rowBounds counterpart (DrumTrackControl/PcmTrackControl have their own field;
    // the solo lane has no per-row array to hold one) -- steps through the (now-relocated)
    // synthLabel's right edge, same shape as a drum/PCM row's rowBounds (steps+label, not the
    // card). A drum/PCM row's OWN rowBounds is its full 52px row height around a 34px button --
    // 9px of built-in breathing room top/bottom (kDrumTrackRowHeight/kPcmTrackRowHeight vs
    // kDrumKeyHeight) -- but trigRow's cell has NO such padding of its own (matches its button
    // height exactly, see kSynthTrigToPolyGap's doc comment), so using trigRow's bounds bare left
    // the wash flush against the step keys, tighter than every other lane's (owner: "too close
    // now" after a first cut that went too far the other way and used the whole card). Pad it out
    // by that same 9px to match, rather than introducing a third distinct row-padding constant.
    constexpr int kSynthFocusPad = (kDrumTrackRowHeight - kDrumKeyHeight) / 2;
    synthFocusBounds = juce::Rectangle<int> (gridX, trigRow.getY() - kSynthFocusPad,
                                             synthLabelGutter.getRight() - gridX,
                                             trigRow.getHeight() + 2 * kSynthFocusPad);

    for (int i = 0; i < 16; ++i)
    {
        auto col = trigRow.removeFromLeft (kStepWidth).reduced (3, 0);
        stepControls[(size_t) i]->select.setBounds (col);
    }

    if (synthPolyRowsHeight > 0)
    {
        auto pr = synthPolyRowsArea;
        for (auto& voice : synthExtraVoices)
        {
            auto vr = pr.removeFromTop (kPolyVoiceRowHeight);
            for (auto& b : voice.steps)
            {
                auto cell = vr.removeFromLeft (kStepWidth);
                b.setBounds (cell.withSizeKeepingCentre (kStepWidth - 6, kPolyVoiceRowHeight - 6));
            }
            pr.removeFromTop (kPolySubRowGap);
        }
    }
    else
    {
        for (auto& voice : synthExtraVoices)
            for (auto& b : voice.steps)
                b.setBounds (0, 0, 0, 0);
    }

    playheadLaneBounds = { gridX, playheadTop, kStepGridWidth, playheadBottom - playheadTop };
}
