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
        // Size to fully contain the taller/wider of the two tabs plus the actual tab-bar depth
        // (not a hardcoded 30), so whichever tab is shown on open gets its full height and neither
        // is clipped as panels grow.
        setSize (juce::jmax (soloSynthPanel.getWidth(), sequencerPanel.getWidth()),
                 juce::jmax (soloSynthPanel.getHeight(), sequencerPanel.getHeight()) + tabs.getTabBarDepth());
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

            // Capture the intended size from the content component (known-correct the moment its
            // constructor returns) BEFORE setContentOwned, so no window-peer timing is involved.
            auto* content = new MainContentComponent();
            const int contentW = content->getWidth();
            const int contentH = content->getHeight();

            setContentOwned (content, true);
            setResizable (true, true);

            // Floor the size so no window manager can map the window at a near-zero default (jmin
            // guards the degenerate case where the content is somehow smaller than the floor).
            setResizeLimits (juce::jmin (760, contentW), juce::jmin (520, contentH), 10000, 10000);

            // Size + centre BEFORE showing -- honoured by well-behaved WMs...
            centreWithSize (contentW, contentH);
            setVisible (true);
            // ...then re-assert AFTER setVisible, once the native peer exists. Some Linux WMs drop
            // bounds set before the window is mapped and open it genuinely tiny (bug-071: "stuff is
            // visible but no room to display most of it, a manual resize brings it to life" -- the
            // bug-009 launch-timing class). Re-centring on the live peer forces the intended size;
            // it's a flicker-free no-op on WMs that already applied the pre-map bounds.
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
    std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
START_JUCE_APPLICATION (CasioXWEditorApplication)
