#include <juce_gui_basics/juce_gui_basics.h>

#include "SequencerPanel.h"
#include "SoloSynthPanel.h"
#include "casioxw/MidiIO.h"
#include "casioxw/ParamModel.h"
#include "casioxw/SysExCodec.h"

//==============================================================================
/** Owns the long-lived model/codec/MIDI objects the GUI is wired to, plus the panels. Chunk 7a:
    replaces the Chunk 4 PlaceholderComponent — this is the first real editor UI. Sequencer MVP
    adds a second tab that shares the same MidiIO as the Solo Synth panel (one output port —
    Connect on either tab opens it for both). codec/midiIO must outlive both panels (SoloSynthPanel
    and ParamControl keep references into codec.model()), so they are declared before the panels
    in this class and never moved/copied afterwards. */
class MainContentComponent : public juce::Component
{
public:
    MainContentComponent()
        : codec (casioxw::ParamModel::fromFile (juce::File (CASIOXW_PARAMS_JSON))),
          soloSynthPanel (codec, midiIO),
          sequencerPanel (codec, midiIO)
    {
        tabs.addTab ("Solo Synth", juce::Colours::darkgrey, &soloSynthPanel, false);
        tabs.addTab ("Sequencer", juce::Colours::darkgrey, &sequencerPanel, false);
        addAndMakeVisible (tabs);
        setSize (juce::jmax (soloSynthPanel.getWidth(), sequencerPanel.getWidth()),
                 soloSynthPanel.getHeight() + 30);
    }

    void resized() override
    {
        tabs.setBounds (getLocalBounds());
    }

private:
    casioxw::SysExCodec codec;
    casioxw::MidiIO midiIO;
    SoloSynthPanel soloSynthPanel;
    SequencerPanel sequencerPanel;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};

//==============================================================================
class CasioXWEditorApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return "Casio XW-P1 Editor"; }
    const juce::String getApplicationVersion() override    { return "0.6.0-sequencer"; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise (const juce::String&) override
    {
        mainWindow.reset (new MainWindow ("Casio XW-P1 Editor  v" + getApplicationVersion()));
    }

    void shutdown() override { mainWindow = nullptr; }

    void systemRequestedQuit() override { quit(); }

    //==============================================================================
    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (juce::String name)
            : DocumentWindow (name,
                              juce::Colours::darkgrey,
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);

            // NOT centreWithSize(getWidth(), getHeight()) -- that reads the size back off this
            // DocumentWindow itself, implicitly trusting that setContentOwned(..., true)'s
            // resize-to-fit has already landed by this point. On at least one Linux WM/native
            // title bar combination that timing didn't hold, so the window opened at some tiny
            // pre-resize default and only a manual drag fixed it (same failure class as bug-009 --
            // an implicit "has the layout already happened?" dependency). Read the size directly
            // from the content component we just built instead: it is known-correct the moment
            // MainContentComponent's constructor returns, with no window-peer timing involved.
            auto* content = new MainContentComponent();
            setContentOwned (content, true);
            setResizable (true, true);
            centreWithSize (content->getWidth(), content->getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
START_JUCE_APPLICATION (CasioXWEditorApplication)
