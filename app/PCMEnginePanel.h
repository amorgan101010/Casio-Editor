#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ParamControl.h"
#include "casioxw/MidiIO.h"
#include "casioxw/SysExCodec.h"

#include <map>
#include <memory>
#include <vector>

//==============================================================================
/** The PCM engine editing panel -- category 0x05 ("Melody Parameter" in the official MIDI
    implementation manual; midi-spec.md's own condensed vocabulary for it is "PCM Melody", see
    reference/midi-spec.md:749 -- the project's UI calls it the "PCM engine").

    Unlike SoloSynthPanel this section has exactly ONE block ("Melody") and a single instance --
    no per-oscillator/LFO navigation, no 9-stage envelope shape -- so this is a deliberately lean
    sibling panel rather than a generalization of SoloSynthPanel (see .wolf/cerebrum.md: "new
    panel per engine", not a shared mega-panel): the block's few groups (Envelope/Vibrato/
    General, per params/xwp1.json's pcmMelody section) render as plain full-width ParamControl
    rows under a bold group header, full stop.

    PROVENANCE / TRUST NOTE (read before treating this like soloSynth): pcmMelody's
    params/xwp1.json entries are hand-transcribed directly from XWP1_midi_EN.pdf section 23
    (printed p70) -- there is no CTRLR Lua source for this domain (franky's panel never
    implemented a Melody/PCM tone page) and, unlike soloSynth, this section has not yet been
    exercised against real hardware. The 18-byte address / vt encoding follows the same general
    SX frame layout soloSynth's franky-derived address already matches (SysExCodec is
    category-agnostic -- no codec changes were needed for this section), and encode()/decode()
    are covered by tests/PcmMelodyTests.cpp's hand-computed frames, but whether a real XW-P1
    accepts a cat=0x05 IPS and applies it to the live Melody tone buffer is UNVERIFIED. Budget a
    hardware read/write check (same shape as the original soloSynth Detune round-trip) before
    trusting this panel's Sync/edit path the way SoloSynthPanel's is trusted.

    Reuses the shared casioxw::MidiIO connection opened from the Solo Synth tab -- like
    SequencerPanel, this panel has no device combo / Connect button of its own, only a status
    label and a Sync button. Every param is a data-driven ParamControl (see ParamControl.h),
    matching the app's "no per-param hand-authored widget" convention. */
class PCMEnginePanel : public juce::Component,
                       private juce::Timer
{
public:
    PCMEnginePanel (casioxw::SysExCodec& codec, casioxw::MidiIO& midiIO);
    ~PCMEnginePanel() override;

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

    // Simple vertical stack -- no knob/fader grid, no block/instance switching, so there is no
    // need for SoloSynthPanel's richer LayoutItem (grid support etc); every row is full width.
    struct Row
    {
        juce::Component* component = nullptr;
        int height = 0;
        int gapAfter = 0;
    };
    std::vector<Row> rows;

    /** Built once from the model (section=="pcmMelody") in the constructor -- there is nothing
        to rebuild in response to (there's only one block/instance, unlike SoloSynthPanel). */
    void buildParamControls();

    int layoutRows (int width);          // assigns row bounds, returns total content height
    void layoutParamContainerWidth();    // shared by buildParamControls() and resized()

    // ---- Sync (poll-on-demand, juce::Timer -- never a busy loop; same pattern as SoloSynthPanel) --
    std::map<juce::String, ParamControl*> outstandingSync;
    juce::uint32 syncStartedMs = 0;

    void syncButtonClicked();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PCMEnginePanel)
};
