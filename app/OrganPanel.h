#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ParamControl.h"
#include "casioxw/MidiIO.h"
#include "casioxw/SysExCodec.h"

#include <map>
#include <memory>
#include <vector>

//==============================================================================
/** The Drawbar Organ editing panel -- category 0x07 ("Drawbar" in the official MIDI
    implementation manual, XW-P1 only). Like PCMEnginePanel this is a lean sibling panel (see
    .wolf/cerebrum.md: "new panel per engine", not a shared mega-panel) rather than a
    generalization of SoloSynthPanel: one block ("DrawbarOrgan"), one instance for every param
    except the drawbars themselves.

    The 9 drawbars ("Position", one JSON param with instanceCount==9 -- one per foot length,
    16' down to 1') are the one place this panel needs SoloSynthPanel's grid-layout trick rather
    than PCMEnginePanel's plain vertical list: all 9 render SIMULTANEOUS ParamControls (not one
    instance selected via a dropdown, the way SoloSynthPanel's OSC1-6 works) as compact vertical
    faders in a row -- the actual physical drawbar-organ metaphor, where every bar is visible and
    settable at once. Each fader's caption is the foot-length label ("16'", "8'", ...), not the
    repeated param name "Position" -- see ParamControl's labelOverride parameter, added for
    exactly this case. Every other param (Percussion, Click, General, Vibrato) is a single
    always-one-instance ParamControl row, same shape as PCMEnginePanel.

    PROVENANCE / TRUST NOTE (read before treating this like soloSynth): drawbarOrgan's
    params/xwp1.json entries are hand-transcribed from XWP1_midi_EN.pdf section 25 (printed
    p71-72) -- franky's CTRLR panel has an organ controller (022_XWOrgan.lua) but it drives
    everything live via NRPN/CC, never SysEx (unlike Solo Synth's sendXWSX path), so there is no
    tone-edit-buffer Lua source for this domain at all, same situation pcmMelody was in.
    HARDWARE-UNVERIFIED (the owner chose to ship flagged rather than probe live hardware first,
    2026-07-18) -- organPosition (the drawbars) carries the riskiest guesses of the section: the
    "Select Bar" block-byte position and whether the wire value is direct or inverted relative to
    UI value. See organPosition's own params/xwp1.json note and the generator's ORGAN_PARAMS
    comment (tools/gen_xwp1.py) for the full reasoning. Budget a midi-probe scan pass (bug-124's
    method) before trusting this panel's Sync/edit path the way SoloSynthPanel's is trusted.

    Reuses the shared casioxw::MidiIO connection opened from the Solo Synth tab -- no device
    combo/Connect button of its own, only a status label and a Sync button, same pattern
    PCMEnginePanel and SequencerPanel already established. */
class OrganPanel : public juce::Component,
                   private juce::Timer
{
public:
    OrganPanel (casioxw::SysExCodec& codec, casioxw::MidiIO& midiIO);
    ~OrganPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    casioxw::SysExCodec& codec;
    casioxw::MidiIO& midiIO;

    juce::Label statusLabel;
    juce::TextButton syncButton { "Sync" };

    juce::Viewport paramViewport;
    juce::Component paramContainer;
    std::vector<std::unique_ptr<ParamControl>> controls;
    std::vector<std::unique_ptr<juce::Component>> groupHeaders;

    // Same shape as SoloSynthPanel::LayoutItem: a plain fixed-height row, OR a wrapping grid of
    // same-size cells (used for the Drawbars fader bank; nothing else in this panel needs it,
    // but PCMEnginePanel's simpler Row-only struct can't express a grid at all).
    struct LayoutItem
    {
        juce::Component* rowComponent = nullptr;
        int rowHeight = 0;
        std::vector<ParamControl*> gridControls;
        int cellWidth = 0, cellHeight = 0;
        int gapAfter = 0;
    };
    std::vector<LayoutItem> layoutItems;

    /** Built once from the model (section=="drawbarOrgan") in the constructor -- there is nothing
        to rebuild in response to (one block, and the only per-instance param -- the drawbars --
        shows every instance at once rather than switching between them). */
    void buildParamControls();

    int layoutSequential (int width);    // walks layoutItems, assigns bounds, returns total height
    void layoutParamContainerWidth();    // shared by buildParamControls() and resized()

    // ---- Sync (poll-on-demand, juce::Timer -- never a busy loop; same pattern as PCMEnginePanel) --
    std::map<juce::String, ParamControl*> outstandingSync;
    juce::uint32 syncStartedMs = 0;

    void syncButtonClicked();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrganPanel)
};
