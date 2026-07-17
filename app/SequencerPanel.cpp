#include "SequencerPanel.h"

#include "casioxw/NoteNames.h"

#include <cmath>

namespace
{
    constexpr int kStepAreaTop = 76;
    constexpr int kStepWidth = 56;
}

SequencerPanel::SequencerPanel (casioxw::MidiIO& midiIOIn)
    : midiIO (midiIOIn)
{
    for (auto& control : stepControls)
    {
        control = std::make_unique<StepControl>();
        addAndMakeVisible (control->enabled);

        control->note.setRange (0.0, 127.0, 1.0);
        control->note.setValue (60.0, juce::dontSendNotification);
        control->note.textFromValueFunction = [] (double v) { return casioxw::midiNoteName ((int) v); };
        control->note.valueFromTextFunction = [] (const juce::String& t) -> double
        {
            const auto n = casioxw::noteNameToMidi (t);
            return n.has_value() ? (double) *n : 0.0;
        };
        control->note.updateText();
        addAndMakeVisible (control->note);
    }

    playStopButton.onClick = [this] { playing ? stop() : play(); };
    addAndMakeVisible (playStopButton);

    tempoSlider.setRange (40.0, 240.0, 1.0);
    tempoSlider.setValue (120.0, juce::dontSendNotification);
    addAndMakeVisible (tempoSlider);
    addAndMakeVisible (tempoLabel);

    channelSlider.setRange (1.0, 16.0, 1.0);
    channelSlider.setValue (1.0, juce::dontSendNotification);
    addAndMakeVisible (channelSlider);
    addAndMakeVisible (channelLabel);

    velocitySlider.setRange (1.0, 127.0, 1.0);
    velocitySlider.setValue (100.0, juce::dontSendNotification);
    addAndMakeVisible (velocitySlider);
    addAndMakeVisible (velocityLabel);

    statusLabel.setText ("Stopped - open a MIDI output on the Solo Synth tab, then Play",
                          juce::dontSendNotification);
    addAndMakeVisible (statusLabel);

    setSize (kStepWidth * 16 + 16, kStepAreaTop + 140);
    resized();
}

SequencerPanel::~SequencerPanel()
{
    stop();
}

void SequencerPanel::play()
{
    if (playing)
        return;

    if (! midiIO.isOutputOpen())
    {
        statusLabel.setText ("Not connected - open a MIDI output on the Solo Synth tab first",
                              juce::dontSendNotification);
        return;
    }

    playing = true;
    currentStep = 0;
    playStopButton.setButtonText ("Stop");
    statusLabel.setText ("Playing", juce::dontSendNotification);
    timerCallback();   // fire step 0 immediately rather than waiting one full interval
}

void SequencerPanel::stop()
{
    if (! playing)
        return;

    stopTimer();
    playing = false;
    playStopButton.setButtonText ("Play");
    statusLabel.setText ("Stopped", juce::dontSendNotification);

    if (soundingNote.has_value())
    {
        midiIO.sendNoteOff (soundingChannel, *soundingNote);
        soundingNote.reset();
    }
    // Safety net: covers the case where a note-off was dropped, or the channel was changed
    // mid-play so soundingChannel no longer matches what's currently selected.
    midiIO.sendAllNotesOff ((int) channelSlider.getValue());
}

casioxw::Sequence SequencerPanel::buildSequence() const
{
    casioxw::Sequence seq;
    seq.tempoBpm = (int) tempoSlider.getValue();
    seq.channel = (int) channelSlider.getValue();
    const int velocity = (int) velocitySlider.getValue();

    for (int i = 0; i < 16; ++i)
    {
        auto& step = seq.steps[(size_t) i];
        step.enabled = stepControls[(size_t) i]->enabled.getToggleState();
        step.note = (int) stepControls[(size_t) i]->note.getValue();
        step.velocity = velocity;
    }
    return seq;
}

void SequencerPanel::timerCallback()
{
    const auto seq = buildSequence();

    // Always release the previously-sent note first, before anything else can send a new one —
    // note-off targets what was SENT, never re-derived from the current (possibly since-edited)
    // step, so an edit mid-play can never orphan a sounding note.
    if (soundingNote.has_value())
    {
        midiIO.sendNoteOff (soundingChannel, *soundingNote);
        soundingNote.reset();
    }

    if (const auto event = casioxw::stepEvent (seq, currentStep))
    {
        midiIO.sendNoteOn (event->channel, event->note, event->velocity);
        soundingNote = event->note;
        soundingChannel = event->channel;
    }

    currentStep = (currentStep + 1) % 16;

    startTimer ((int) std::lround (casioxw::stepIntervalMs (seq)));
}

void SequencerPanel::resized()
{
    auto bounds = getLocalBounds().reduced (8);

    auto transportRow = bounds.removeFromTop (28);
    playStopButton.setBounds (transportRow.removeFromLeft (80));
    transportRow.removeFromLeft (8);
    tempoLabel.setBounds (transportRow.removeFromLeft (80));
    tempoSlider.setBounds (transportRow.removeFromLeft (150));
    transportRow.removeFromLeft (16);
    channelLabel.setBounds (transportRow.removeFromLeft (90));
    channelSlider.setBounds (transportRow.removeFromLeft (130));
    transportRow.removeFromLeft (16);
    velocityLabel.setBounds (transportRow.removeFromLeft (60));
    velocitySlider.setBounds (transportRow.removeFromLeft (130));

    bounds.removeFromTop (8);
    statusLabel.setBounds (bounds.removeFromTop (20));
    bounds.removeFromTop (8);

    for (int i = 0; i < 16; ++i)
    {
        auto col = bounds.removeFromLeft (kStepWidth).reduced (3);
        stepControls[(size_t) i]->enabled.setBounds (col.removeFromTop (24));
        stepControls[(size_t) i]->note.setBounds (col);
    }
}
