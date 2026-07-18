#include <juce_gui_basics/juce_gui_basics.h>

#include "AppVersion.h"
#include "EditorLookAndFeel.h"
#include "PCMEnginePanel.h"
#include "SequencerPanel.h"
#include "SoloSynthPanel.h"
#include "casioxw/MidiIO.h"
#include "casioxw/ParamModel.h"
#include "casioxw/SysExCodec.h"

//==============================================================================
/** Owns the long-lived model/codec/MIDI objects the GUI is wired to, plus the panels. Chunk 7a:
    replaces the Chunk 4 PlaceholderComponent — this is the first real editor UI. Sequencer MVP
    adds a second tab that shares the same MidiIO as the Solo Synth panel (one output port —
    Connect on either tab opens it for both); the PCM Engine tab (category 0x05 "Melody", see
    app/PCMEnginePanel.h) is a third sibling on the same shared MidiIO, with no device combo of
    its own — same pattern SequencerPanel already established. codec/midiIO must outlive every
    panel (SoloSynthPanel/PCMEnginePanel and ParamControl keep references into codec.model()), so
    they are declared before the panels in this class and never moved/copied afterwards. */
class MainContentComponent : public juce::Component
{
public:
    MainContentComponent()
        : codec (casioxw::ParamModel::fromFile (juce::File (CASIOXW_PARAMS_JSON))),
          soloSynthPanel (codec, midiIO),
          pcmEnginePanel (codec, midiIO),
          sequencerPanel (codec, midiIO)
    {
        // Capture the panels' natural sizes BEFORE tabbing them. addTab() reparents each panel into
        // the TabbedComponent's content area and resizes it to fit that area -- which is still 0x0
        // here -- so reading getWidth()/getHeight() afterwards yields 0. Reading it after was the
        // real cause of the window opening at 0x30 / tiny (bug-071, pinned by the launch diagnostic);
        // the panels' own constructors have already set these to the real values.
        const int contentW = juce::jmax (soloSynthPanel.getWidth(), pcmEnginePanel.getWidth(), sequencerPanel.getWidth());
        const int contentH = juce::jmax (soloSynthPanel.getHeight(), pcmEnginePanel.getHeight(), sequencerPanel.getHeight());

        // TabbedComponent's colour arg only matters for its OWN default tab-content-area fill;
        // EditorLookAndFeel::drawTabButton/drawTabbedButtonBarBackground own the visible tab bar.
        tabs.addTab ("Solo Synth", EditorColours::chassisBg, &soloSynthPanel, false);
        tabs.addTab ("PCM Engine", EditorColours::chassisBg, &pcmEnginePanel, false);
        tabs.addTab ("Sequencer", EditorColours::chassisBg, &sequencerPanel, false);
        addAndMakeVisible (tabs);
        // + actual tab-bar depth (not a hardcoded 30) so the shown tab gets its full height.
        setSize (contentW, contentH + tabs.getTabBarDepth());
    }

    void resized() override
    {
        tabs.setBounds (getLocalBounds());
    }

private:
    casioxw::SysExCodec codec;
    casioxw::MidiIO midiIO;
    SoloSynthPanel soloSynthPanel;
    PCMEnginePanel pcmEnginePanel;
    SequencerPanel sequencerPanel;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};

//==============================================================================
class CasioXWEditorApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return casioxw::app::kApplicationName; }
    const juce::String getApplicationVersion() override    { return casioxw::app::kApplicationVersion; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise (const juce::String&) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        mainWindow.reset (new MainWindow ("Casio XW-P1 Editor  v" + getApplicationVersion()));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override { quit(); }

    //==============================================================================
    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (juce::String name)
            : DocumentWindow (name,
                              EditorColours::chassisBg,
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);

            auto* content = new MainContentComponent();
            const int contentW = content->getWidth();
            const int contentH = content->getHeight();

            setContentOwned (content, true);
            setResizable (true, true);

            // Guard against near-zero launch sizes on WMs that ignore pre-map bounds.
            setResizeLimits (juce::jmin (760, contentW), juce::jmin (520, contentH), 10000, 10000);

            centreWithSize (contentW, contentH);
            setVisible (true);
            // Re-assert size on the mapped peer for Linux WMs that drop pre-map bounds.
            centreWithSize (contentW, contentH);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    EditorLookAndFeel lookAndFeel;   // declared before mainWindow: must outlive every widget it themes
    std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
START_JUCE_APPLICATION (CasioXWEditorApplication)
