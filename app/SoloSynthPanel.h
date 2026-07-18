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

    void paint (juce::Graphics&) override;
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
    juce::String currentGroup;                                   // the ONE group currently rendered when
                                                                   // the Group selector is shown (see
                                                                   // groupSelectorNeeded below)

    // Chunk 7e item 2: only OSC has enough params (56, vs. PWM=3/Etc=11/TotalFilter=20/LFO=8) to
    // benefit from group-at-a-time navigation at all -- "the OSC block is the only thing that
    // needs groups like that", so the selector is hidden for small blocks and every one of that
    // block's groups renders together instead (see groupsToRender() in the .cpp). Data-driven
    // threshold on total param count, not a hardcoded block-name check, so a future large block
    // gets the same treatment automatically.
    bool groupSelectorNeeded = false;

    void buildBlockList();
    void blockSelectionChanged();
    void instanceSelectionChanged();

    /** Repopulates groupCombo/currentGroupList from casioxw::orderedGroupsForBlock() for
        currentBlock, decides groupSelectorNeeded (see above) and shows/hides groupLabel/groupCombo
        accordingly, and selects the first group (or clears currentGroup if the block somehow has
        none). Called whenever currentBlock changes; does NOT call rebuildParamControls() itself
        — callers do that afterwards, same pattern as buildBlockList()/blockSelectionChanged(). */
    void buildGroupList();
    void groupSelectionChanged();

    /** The groups to actually render right now: just {currentGroup} when the selector is shown,
        or the whole of currentGroupList (every group for this block, in order) when it's hidden
        because the block is small enough not to need group-at-a-time navigation. */
    std::vector<juce::String> groupsToRender() const;

    /** Chunk 7c item 1: re-run the Sync flow automatically after a block/instance switch or a
        successful Connect — but only when both ends are actually open (matches the manual
        Sync button's own guard). No-op otherwise, so the user isn't nagged with a status message
        on every tab switch while disconnected. */
    void autoSyncIfConnected();

    // ---- Param list ----------------------------------------------------------------------------
    juce::Viewport paramViewport;
    juce::Component paramContainer;
    std::vector<std::unique_ptr<ParamControl>> controls;

    // Chunk 7c item 5: EnvelopeDisplay widgets, interleaved with `controls` inside paramContainer
    // but owned separately since they're not per-param.
    std::vector<std::unique_ptr<juce::Component>> groupRows;

    // Feeds live envelope-stage values into their EnvelopeDisplay (both from user edits, via
    // ParamControl::onValueChanged, and from sync replies, via timerCallback()) without ParamControl
    // itself knowing envelopes exist. Key: "<paramId>#<instance>", same as outstandingSync.
    std::map<juce::String, std::function<void (int)>> envelopeSinks;

    // Chunk 7e: one sequential layout pass replaces the old single-knob-grid-is-always-last
    // assumption, which broke once groupsToRender() can render SEVERAL groups at once (small
    // blocks with the Group selector hidden) — each with its own EnvelopeDisplay/knob-grid/
    // fader-row, at genuinely different Y offsets that must cascade correctly if an earlier
    // region's row count changes on resize (fewer columns -> more rows -> everything after it
    // shifts down). A LayoutItem is either one fixed-height row (a ParamControl in list mode, or
    // a groupRows entry) whose WIDTH tracks the viewport, or a wrapping GRID of same-size cells
    // (knob or vertical-fader ParamControls) whose row count depends on the current width.
    // rebuildParamControls() builds this list once from the model; layoutSequential() (called by
    // both rebuildParamControls() and layoutParamContainerWidth() on resize) is the ONLY place
    // that assigns Y positions, so a resize is always a full, correct recomputation rather than
    // patching each region independently.
    struct LayoutItem
    {
        juce::Component* rowComponent = nullptr;   // set for a plain fixed-height row; else...
        int rowHeight = 0;
        std::vector<ParamControl*> gridControls;   // ...set (non-empty) for a wrapping grid
        int cellWidth = 0, cellHeight = 0;
        int gapAfter = 0;                          // extra Y consumed after this item, before the next
    };
    std::vector<LayoutItem> layoutItems;

    void rebuildParamControls();
    void layoutParamContainerWidth();   // shared by rebuildParamControls() and resized()
    int layoutSequential (int width);   // walks layoutItems, assigns bounds, returns total height

    // ---- Sync (poll-on-demand, juce::Timer — never a busy loop) --------------------------------
    std::map<juce::String, ParamControl*> outstandingSync;   // key: "<paramId>#<instance>"
    juce::uint32 syncStartedMs = 0;

    void syncButtonClicked();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoloSynthPanel)
};
