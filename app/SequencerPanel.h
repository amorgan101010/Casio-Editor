#pragma once

#include "casioxw/MidiIO.h"
#include "casioxw/Sequence.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>
#include <optional>

/** 16-step note sequencer MVP: a step grid (enable + note per step), tempo/channel/velocity
    controls, and a Play/Stop transport. Sends Note On/Off directly via casioxw::MidiIO — no
    SysExCodec involved, this is channel-voice only, not a tone/parameter edit.

    Timer-driven, not the sequencer roadmap's look-ahead + timestamped-output scheduler — an
    accepted MVP shortcut (SEQUENCER_HANDOFF.md scope), not a silent one. Expect some timing
    looseness under load; full-fidelity scheduling is deferred.

    No p-locks, no chains/song mode, no persistence — this is the flattest possible slice that
    still exercises MidiIO's channel-voice path end to end against real hardware. */
class SequencerPanel : public juce::Component, private juce::Timer
{
public:
    explicit SequencerPanel (casioxw::MidiIO& midiIO);
    ~SequencerPanel() override;

    void resized() override;

private:
    struct StepControl
    {
        juce::ToggleButton enabled;
        juce::Slider note { juce::Slider::LinearVertical, juce::Slider::TextBoxBelow };
    };

    void timerCallback() override;
    void play();
    void stop();
    casioxw::Sequence buildSequence() const;

    casioxw::MidiIO& midiIO;

    std::array<std::unique_ptr<StepControl>, 16> stepControls;
    juce::TextButton playStopButton { "Play" };
    juce::Slider tempoSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider channelSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider velocitySlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label tempoLabel { {}, "Tempo (BPM)" };
    juce::Label channelLabel { {}, "MIDI Channel" };
    juce::Label velocityLabel { {}, "Velocity" };
    juce::Label statusLabel;

    int currentStep = 0;
    bool playing = false;

    // The note actually sent, so note-off always targets what was sent — never re-derived from
    // the model at note-off time, or editing a step (or the channel) mid-play would orphan the
    // sounding note.
    std::optional<int> soundingNote;
    int soundingChannel = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencerPanel)
};
