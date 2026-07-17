#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ParamControl.h"
#include "casioxw/MidiIO.h"
#include "casioxw/SysExCodec.h"

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

    // ---- Block / instance navigation ----------------------------------------------------------
    juce::Label blockLabel { {}, "Block:" };
    juce::ComboBox blockCombo;
    juce::Label instanceLabel { {}, "Instance:" };
    juce::ComboBox instanceCombo;
    juce::TextButton syncButton { "Sync" };

    juce::StringArray blockOrder;                                // e.g. OSC, PWM, Etc, TotalFilter, LFO
    juce::String currentBlock;
    int currentInstance = 1;

    void buildBlockList();
    void blockSelectionChanged();
    void instanceSelectionChanged();

    // ---- Param list ----------------------------------------------------------------------------
    juce::Viewport paramViewport;
    juce::Component paramContainer;
    std::vector<std::unique_ptr<ParamControl>> controls;

    void rebuildParamControls();

    // ---- Sync (poll-on-demand, juce::Timer — never a busy loop) --------------------------------
    std::map<juce::String, ParamControl*> outstandingSync;   // key: "<paramId>#<instance>"
    juce::uint32 syncStartedMs = 0;

    void syncButtonClicked();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoloSynthPanel)
};
