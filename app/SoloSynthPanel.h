#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "EnvelopeDisplay.h"
#include "ParamControl.h"
#include "casioxw/MidiIO.h"
#include "casioxw/SysExCodec.h"

#include <functional>
#include <map>
#include <memory>
#include <vector>

//==============================================================================
/** The Solo Synth editing panel — Chunk 7a's first real GUI, wired end-to-end to a real
    casioxw::MidiIO + casioxw::SysExCodec + casioxw::ParamModel.

    Mirrors the XW-P1's own navigation paradigm (confirmed against the manual): a block
    selector (OSC/PWM/Etc/TotalFilter/LFO) and, for blocks with instanceCount > 1, an instance
    selector using that block's instances.labels verbatim (so OSC reads "Synth1/Synth2/PCM1/
    PCM2/EXT/Noise" — exactly what's printed on the real synth). Below that: every Solo Synth
    param belonging to the selected block, as a data-driven ParamControl (see ParamControl.h).

    Device connection + Sync are poll-on-demand, matching real hardware behaviour: the XW-P1
    never pushes updates unsocilited, and per .wolf/cerebrum.md its own front-panel display
    doesn't even live-refresh on an incoming SysEx edit (only on page re-entry) — so there is no
    "live" state to mirror continuously here either; Sync is an explicit, one-shot user action. */
class SoloSynthPanel : public juce::Component,
                       private juce::Timer
{
public:
    SoloSynthPanel (casioxw::SysExCodec& codec, casioxw::MidiIO& midiIO);
    ~SoloSynthPanel() override;

    void resized() override;

private:
    casioxw::SysExCodec& codec;
    casioxw::MidiIO& midiIO;

    // ---- Device panel ------------------------------------------------------------------------
    juce::Label deviceLabel { {}, "MIDI device:" };
    juce::ComboBox inputCombo, outputCombo;
    juce::TextButton connectButton { "Connect" };
    juce::Label statusLabel;
    std::vector<juce::MidiDeviceInfo> inputDevices, outputDevices;

    void refreshDeviceLists();
    void connectButtonClicked();

    // ---- Block / instance / group navigation --------------------------------------------------
    juce::Label blockLabel { {}, "Block:" };
    juce::ComboBox blockCombo;
    juce::Label instanceLabel { {}, "Instance:" };
    juce::ComboBox instanceCombo;
    // Chunk 7d item 1: show one param group at a time (the full-block stacked list got "massive"
    // per owner feedback) instead of every group for the block stacked in one long scroll.
    juce::Label groupLabel { {}, "Group:" };
    juce::ComboBox groupCombo;
    juce::TextButton syncButton { "Sync" };

    juce::StringArray blockOrder;                                // e.g. OSC, PWM, Etc, TotalFilter, LFO
    juce::String currentBlock;
    int currentInstance = 1;
    std::vector<juce::String> currentGroupList;                  // groups for currentBlock, in display order
    juce::String currentGroup;                                   // the ONE group currently rendered

    void buildBlockList();
    void blockSelectionChanged();
    void instanceSelectionChanged();

    /** Repopulates groupCombo/currentGroupList from casioxw::orderedGroupsForBlock() for
        currentBlock and selects the first group (or clears currentGroup if the block somehow has
        none). Called whenever currentBlock changes; does NOT call rebuildParamControls() itself
        — callers do that afterwards, same pattern as buildBlockList()/blockSelectionChanged(). */
    void buildGroupList();
    void groupSelectionChanged();

    /** Chunk 7c item 1: re-run the Sync flow automatically after a block/instance switch or a
        successful Connect — but only when both ends are actually open (matches the manual
        Sync button's own guard). No-op otherwise, so the user isn't nagged with a status message
        on every tab switch while disconnected. */
    void autoSyncIfConnected();

    // ---- Param list ----------------------------------------------------------------------------
    juce::Viewport paramViewport;
    juce::Component paramContainer;
    std::vector<std::unique_ptr<ParamControl>> controls;

    // Chunk 7c item 4: group headers (bold label + separator), and item 5: EnvelopeDisplay
    // widgets — both interleaved with `controls` inside paramContainer, but owned separately
    // since they're not per-param. Generic juce::Component storage + a uniform width-relayout in
    // resized() (see layoutParamContainerWidth()) avoids maintaining two divergent layout paths.
    std::vector<std::unique_ptr<juce::Component>> groupRows;

    // Feeds live envelope-stage values into their EnvelopeDisplay (both from user edits, via
    // ParamControl::onValueChanged, and from sync replies, via timerCallback()) without ParamControl
    // itself knowing envelopes exist. Key: "<paramId>#<instance>", same as outstandingSync.
    std::map<juce::String, std::function<void (int)>> envelopeSinks;

    // Chunk 7d item 2: knob-mode ParamControls (non-envelope Slider-kind params) laid out in a
    // wrapping grid rather than the one-per-row list. Non-owning pointers into `controls`, in
    // the order they should flow through the grid; knobGridStartY is the container-Y the grid
    // begins at (set once by rebuildParamControls(), reused by layoutKnobGrid() on resize).
    // Invariant relied on by layoutParamContainerWidth(): the knob grid, when present, is always
    // the LAST thing in paramContainer for the currently-selected group (envelope groups never
    // produce knobs, and only one group is ever shown at a time) — so its bottom Y can be used
    // directly as the container's total content height. If a future change ever adds content
    // after the grid, this height calc needs revisiting.
    std::vector<ParamControl*> knobControls;
    int knobGridStartY = 0;

    void rebuildParamControls();
    void layoutParamContainerWidth();   // shared by rebuildParamControls() and resized()
    int layoutKnobGrid (int width);     // positions knobControls from knobGridStartY; returns bottom Y

    // ---- Sync (poll-on-demand, juce::Timer — never a busy loop) --------------------------------
    std::map<juce::String, ParamControl*> outstandingSync;   // key: "<paramId>#<instance>"
    juce::uint32 syncStartedMs = 0;

    void syncButtonClicked();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoloSynthPanel)
};
