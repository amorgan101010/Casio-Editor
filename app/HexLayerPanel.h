#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ParamControl.h"
#include "casioxw/MidiIO.h"
#include "casioxw/SysExCodec.h"

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

    REDESIGNED 2026-07-19 (owner brief): the "Layer" block used to be a one-instance-at-a-time view
    (six pages behind an instance combo), the same per-layer paging pain the physical synth has.
    It's now a single grid of LayerCard components, 2 wide x 3 tall (a fixed layout, not a
    responsive wrap -- see layoutLayerGrid()'s own comment) -- all 6 layers visible at once, no
    instance nav at all (the instance combo/label were removed; Global's instanceCount is always 1
    so it never needed one anyway). The grid is deliberately 2-COLUMN, not 3: layerCards is built
    in instance order 1..6, so 2 columns pairs (Layer 1, Layer 2) / (3, 4) / (5, 6) side by side --
    exactly the pairing Pitch Lock needs (Layer 2 locks to 1, 4 to 3, 6 to 5), per owner feedback
    after the first version (which wrapped to 3-wide, an unrelated grouping) landed.
    Each card is a compact clone of the old full-width row list: On/Off + Pitch Lock share ONE row
    (left/right half each, Pitch Lock blank on odd layers -- owner feedback: this keeps every
    card's header the same height, so the knob grids below line up across ALL 6 cards, not just
    within a card-row) + a full-width Wave row, then every remaining Layer param as a
    ParamControl::RenderMode::Knob -- the owner's explicit choice (offered against the sequencer's
    screen-cell p-lock knobs, picked "matches the rest of the editor's normal control chrome"
    instead) -- tiled in a 5-column grid (owner feedback: fewer/wider knob rows per card, once the
    2-wide card layout freed up width to do it), same cellWidth/cellHeight-tiling convention
    OrganPanel's drawbar grid and SoloSynthPanel's OSC knob grid already use.
    Knob cells intentionally stay at ParamControl's standard 100x110 size, NOT shrunk to fit more
    per row -- SequencerPanel's per-step knobs were once smaller ("tiny dots", owner screenshot)
    and were enlarged to their current size for readability/grabbability; repeating that mistake
    here to cram more per card was rejected in favour of using the width the app already has
    (Sequencer's own tab is ~1490px wide, comfortably fitting a 2-wide card grid at the wider
    5-knob-column size) and letting the existing vertical-scrolling paramViewport absorb the 3
    rows of card pairs, rather than shrinking the widget that was already corrected once for this
    reason.
    Global (the shared Pitch/Amp LFO pair + Detune, one instance, not part of the per-layer paging
    pain to begin with) is UNCHANGED: same flat group-header + full-width-row list behind the block
    combo. No per-card group headers (General/Amp/Filter/Effects/Range) -- all 6 cards repeat the
    identical param set/order, so the layout is learned once rather than paying for 5 sub-headers
    times 6 cards; see buildLayerGrid()'s own comment for the same tradeoff stated inline.

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
    own, only a status label and a Sync button. Every param is a data-driven ParamControl (see
    ParamControl.h), matching the app's "no per-param hand-authored widget" convention. */
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

    // ---- Block navigation (Layer grid vs. Global LFO/Detune list) -----------------------------
    // No instance combo: Global's instanceCount is always 1, and Layer now shows all 6 instances
    // at once (the LayerCard grid below) instead of one at a time -- there is nothing left for an
    // instance selector to do.
    juce::Label blockLabel { {}, "Block:" };
    juce::ComboBox blockCombo;

    juce::StringArray blockOrder;   // "Layer", "Global"
    juce::String currentBlock;

    void buildBlockList();
    void blockSelectionChanged();

    /** Re-run Sync automatically after a block switch, matching PCMEnginePanel/SoloSynthPanel's
        autoSyncIfConnected() pattern -- but only when both MIDI ends are open. */
    void autoSyncIfConnected();

    // ---- Param display --------------------------------------------------------------------------
    juce::Viewport paramViewport;
    juce::Component paramContainer;
    std::vector<std::unique_ptr<ParamControl>> controls;
    std::vector<std::unique_ptr<juce::Component>> groupHeaders;   // Global block only

    class LayerCard;
    std::vector<std::unique_ptr<LayerCard>> layerCards;   // Layer block only, 6 of them

    // Global block: simple vertical stack of full-width rows -- no knob/fader grid, same as
    // PCMEnginePanel (one instance, so no repeated-instance-of-one-param case).
    struct Row
    {
        juce::Component* component = nullptr;
        int height = 0;
        int gapAfter = 0;
    };
    std::vector<Row> rows;

    /** Rebuilds `controls` + (`layerCards` or `rows`, per currentBlock). Called whenever the
        block combo changes. */
    void rebuildParamControls();
    void buildLayerGrid();     // currentBlock == "Layer": 6 LayerCards, all instances at once
    void buildGlobalList();    // currentBlock == "Global": unchanged group-header + row list

    int layoutRows (int width);          // Global: assigns row bounds, returns total content height
    int layoutLayerGrid (int width);     // Layer: wraps LayerCards, returns total content height
    void layoutParamContainerWidth();    // shared by rebuildParamControls() and resized()

    // ---- Sync (poll-on-demand, juce::Timer -- never a busy loop; same pattern as PCMEnginePanel) --
    std::map<juce::String, ParamControl*> outstandingSync;
    juce::uint32 syncStartedMs = 0;

    void syncButtonClicked();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HexLayerPanel)
};
