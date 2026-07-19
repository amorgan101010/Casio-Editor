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
    siblings (address layout doesn't change), but rebuildParamControls() skips building its
    ParamControl entirely when currentInstance is odd (1/3/5) -- there's nothing for those layers
    to lock TO. See gen_xwp1.py's HEXLAYER_PITCH_LOCK_NOTE for the still-open ai=2 address question.
    A block combo picks between them; an instance combo (Layer 1..6, hidden for Global) picks
    which layer is shown, the same "one instance at a time" navigation SoloSynthPanel's OSC block
    already established for a comparable instance count -- unlike the Drawbar Organ's 9
    simultaneous drawbar faders, a hex layer's own params (envelope/amp/filter offsets) are edited
    per-layer the way an oscillator is, not compared side-by-side the way drawbar registration is.

    Deliberately lean: no group selector (every block here is small enough -- at most 16 params --
    that group-at-a-time navigation isn't worth the friction, same call PCMEnginePanel/OrganPanel
    made), no envelope graphic (no 9-stage envelope params in this section), no knob/fader grid
    (no repeated-instance-of-one-param case the way Organ's drawbars are). Every group for the
    current block renders as plain full-width ParamControl rows under a bold group header, exactly
    PCMEnginePanel's shape, with a block/instance combo layered on top for the two-block/six-
    instance navigation this section (unlike PCM's single flat block) actually needs.

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

    // ---- Block / instance navigation ----------------------------------------------------------
    juce::Label blockLabel { {}, "Block:" };
    juce::ComboBox blockCombo;
    juce::Label instanceLabel { {}, "Layer:" };
    juce::ComboBox instanceCombo;

    juce::StringArray blockOrder;   // "Layer", "Global"
    juce::String currentBlock;
    int currentInstance = 1;

    void buildBlockList();
    void blockSelectionChanged();
    void instanceSelectionChanged();

    /** Re-run Sync automatically after a block/instance switch, matching PCMEnginePanel/
        SoloSynthPanel's autoSyncIfConnected() pattern -- but only when both MIDI ends are open. */
    void autoSyncIfConnected();

    // ---- Param list -----------------------------------------------------------------------------
    juce::Viewport paramViewport;
    juce::Component paramContainer;
    std::vector<std::unique_ptr<ParamControl>> controls;
    std::vector<std::unique_ptr<juce::Component>> groupHeaders;

    // Simple vertical stack -- no knob/fader grid, same as PCMEnginePanel (this section has no
    // repeated-instance-of-one-param case; every row is full width).
    struct Row
    {
        juce::Component* component = nullptr;
        int height = 0;
        int gapAfter = 0;
    };
    std::vector<Row> rows;

    /** Rebuilds `controls`/`rows` for (currentBlock, currentInstance). Called whenever either
        changes. */
    void rebuildParamControls();

    int layoutRows (int width);          // assigns row bounds, returns total content height
    void layoutParamContainerWidth();    // shared by rebuildParamControls() and resized()

    // ---- Sync (poll-on-demand, juce::Timer -- never a busy loop; same pattern as PCMEnginePanel) --
    std::map<juce::String, ParamControl*> outstandingSync;
    juce::uint32 syncStartedMs = 0;

    void syncButtonClicked();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HexLayerPanel)
};
