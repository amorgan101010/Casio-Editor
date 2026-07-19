#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ParamControl.h"
#include "casioxw/MidiIO.h"
#include "casioxw/SysExCodec.h"

#include <functional>
#include <map>
#include <memory>
#include <vector>

//==============================================================================
/** The Hex Layer engine editing panel -- category 0x08 ("Hex Layer Parameter" in the official
    MIDI implementation manual, XW-P1 only), section "hexLayer" in params/xwp1.json.

    Two blocks, per the manual's own address layout: "Layer" (instanceCount=6, per-layer Pan/
    Pitch/Amp/Filter/Effects/Range offsets -- the block selector bit-range "2-0:Layer Number") and
    "Global" (instanceCount=1, Detune Number plus the entire LFO section -- one shared Pitch/Amp
    LFO pair for all 6 layers, per the manual's fixed Block 00000000 for those params).

    Pitch Lock (id 0x14, "Layer" block) was hand-authored and shipped as a Global/1-instance param,
    REMOVED 2026-07-19 after owner hardware testing found no effect and no corresponding synth-menu
    setting, then RE-ADDED the same day once the owner re-read the manual and found the real scope:
    "Pitch Lock (Layers 2, 4, and 6 only)" -- turning it on for an even layer copies the odd
    layer's pitch onto it (Layer2<-1, Layer4<-3, Layer6<-5). The original negative test is
    consistent with the removed version targeting the wrong SCOPE (hex-layer-wide instead of
    per-layer), not with the control not existing. It's declared with 6 instances like its Layer
    siblings (address layout doesn't change), but buildLayerGrid() skips building its ParamControl
    entirely on odd layer cards (1/3/5) -- there's nothing for those layers to lock TO. See
    gen_xwp1.py's HEXLAYER_PITCH_LOCK_NOTE for the still-open ai=2 address question.

    REDESIGNED 2026-07-19 (owner brief + two follow-up passes): the "Layer" block used to be a
    one-instance-at-a-time view (six pages behind an instance combo), the same per-layer paging
    pain the physical synth has. It's now ONE continuous page, no block combo at all -- a 2-wide x
    3-tall grid of LayerCard components (all 6 layers, paired 1&2/3&4/5&6 to match Pitch Lock's own
    relationship) followed by a single full-width GlobalSection carrying the shared Pitch/Amp LFO
    pair + Detune. See buildLayerGrid()/buildGlobalSection()/layoutContent()'s own comments for the
    layout mechanics, and MiniKnob's doc comment for why the numeric params render on a bespoke
    compact knob widget rather than ParamControl::RenderMode::Knob (that widget's fixed ~50px of
    internal label/textbox overhead can't produce a legible dial at the cell size this layout
    needs -- see MiniKnob's comment for the full reasoning and the sequencer-step-knob precedent it
    matches instead).

    On/Off + Pitch Lock share ONE row per card (left/right half each, Pitch Lock blank-but-same-
    height on odd layers) so every card's header is identical height and the knob grids below line
    up across all 6 cards. Wave stays a full-width ParamControl row (needs WavePicker, not a knob).
    No per-card group headers (General/Amp/Filter/Effects/Range) -- all 6 cards repeat the
    identical param set/order, so the layout is learned once rather than paying for 5 sub-headers
    times 6 cards.

    PROVENANCE / TRUST NOTE (read before treating this like soloSynth): hexLayer's
    params/xwp1.json entries are hand-transcribed from XWP1_midi_EN.pdf section 26 (printed
    p72-73) -- franky's CTRLR panel has a Hex Layer controller (020_ToneHexLayer.lua) but it only
    ever sends NRPN (not SysEx), and only for a small per-layer-level + all-layer mixer subset,
    not this panel's full per-layer offset/LFO set -- so there is no tone-edit-buffer Lua source
    to cross-check against for ANY of this panel's params, and NONE of it has been exercised
    against real hardware yet. Given the Drawbar Organ precedent (a SysEx write that landed and
    persisted but did not reach the running voice, needing an NRPN live-fader path instead -- see
    app/OrganPanel.cpp's sendDrawbarNrpn), a cat=0x08 write here may face the same issue, since the
    Lua's own live path for Hex Layer is also NRPN, not SysEx. Budget a hardware read/write/audible
    check (same shape as the original soloSynth Detune round-trip) before trusting this panel's
    Sync/edit path the way SoloSynthPanel's is trusted; see params/xwp1.json's hexLayer section
    note for the full provenance statement.

    Reuses the shared casioxw::MidiIO connection opened from the Solo Synth tab -- like
    PCMEnginePanel/OrganPanel/SequencerPanel, this panel has no device combo/Connect button of its
    own, only a status label and a Sync button. Every param is data-driven (ParamControl for
    toggle/combo, MiniKnob for numeric sliders), matching the app's "no per-param hand-authored
    widget" convention -- MiniKnob is a second reusable renderer for a param, the same relationship
    ParamPageDisplay::Cell already has to ParamControl elsewhere in this codebase. */
class HexLayerPanel : public juce::Component,
                      private juce::Timer
{
public:
    HexLayerPanel (casioxw::SysExCodec& codec, casioxw::MidiIO& midiIO);
    ~HexLayerPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    casioxw::SysExCodec& codec;
    casioxw::MidiIO& midiIO;

    juce::Label statusLabel;
    juce::TextButton syncButton { "Sync" };

    // ---- Param display --------------------------------------------------------------------------
    // One continuous page, no block combo -- Layer's 6 instances (the LayerCard grid) and Global's
    // 1 instance (GlobalSection) are both always visible, built once in the constructor and never
    // rebuilt in response to any nav control (there isn't one any more).
    juce::Viewport paramViewport;
    juce::Component paramContainer;

    // Toggle/combo params (On/Off, Pitch Lock, Wave, the 2 LFO Wave Type combos) -- these need
    // ParamControl's real widgets (ToggleButton/WavePicker/ComboBox), not a knob.
    std::vector<std::unique_ptr<ParamControl>> controls;

    class MiniKnob;
    // Every numeric slider param (90 across the 6 LayerCards + 13 in GlobalSection) -- see
    // MiniKnob's own doc comment for why these are a bespoke widget rather than
    // ParamControl::RenderMode::Knob.
    std::vector<std::unique_ptr<MiniKnob>> miniKnobs;

    class LayerCard;
    std::vector<std::unique_ptr<LayerCard>> layerCards;   // 6 of them, instance order 1..6

    class GlobalSection;
    std::unique_ptr<GlobalSection> globalSection;

    void buildParamControls();     // builds layerCards + globalSection + wires every control once
    void buildLayerGrid();         // 6 LayerCards, all instances at once
    void buildGlobalSection();     // the one shared-LFO/Detune section

    /** Lays out layerCards as a 2-wide grid, then globalSection full-width beneath it. Returns
        total content height (paramContainer is sized to this). */
    int layoutContent (int width);
    void layoutParamContainerWidth();    // shared by buildParamControls() and resized()

    // ---- Sync (poll-on-demand, juce::Timer -- never a busy loop; same pattern as PCMEnginePanel) --
    // Value is a type-erased "push this synced value into the widget" callback rather than a
    // ParamControl* directly, since a sync target may be either a ParamControl or a MiniKnob.
    std::map<juce::String, std::function<void (int)>> outstandingSync;
    juce::uint32 syncStartedMs = 0;

    void syncButtonClicked();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HexLayerPanel)
};
