#include <juce_gui_basics/juce_gui_basics.h>

#include "SoloSynthPanel.h"
#include "casioxw/MidiIO.h"
#include "casioxw/ParamModel.h"
#include "casioxw/SysExCodec.h"

//==============================================================================
/** Owns the long-lived model/codec/MIDI objects the GUI is wired to, plus the SoloSynthPanel
    itself. Chunk 7a: replaces the Chunk 4 PlaceholderComponent — this is the first real editor
    UI. codec/midiIO must outlive `panel` (SoloSynthPanel keeps references to both, and
    ParamControl keeps a reference into codec.model()'s ParamInfo vector), so they are declared
    before `panel` in this class and never moved/copied afterwards. */
class MainContentComponent : public juce::Component
{
public:
    MainContentComponent()
        : codec (casioxw::ParamModel::fromFile (juce::File (CASIOXW_PARAMS_JSON))),
          panel (codec, midiIO)
    {
        addAndMakeVisible (panel);
        setSize (panel.getWidth(), panel.getHeight());
    }

    void resized() override
    {
        panel.setBounds (getLocalBounds());
    }

private:
    casioxw::SysExCodec codec;
    casioxw::MidiIO midiIO;
    SoloSynthPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};

//==============================================================================
class CasioXWEditorApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return "Casio XW-P1 Editor"; }
    const juce::String getApplicationVersion() override    { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise (const juce::String&) override
    {
        mainWindow.reset (new MainWindow ("Casio XW-P1 Editor"));
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
            setContentOwned (new MainContentComponent(), true);
            setResizable (true, true);
            centreWithSize (getWidth(), getHeight());
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
