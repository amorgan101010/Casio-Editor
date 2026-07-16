#include <juce_gui_basics/juce_gui_basics.h>

#include "casioxw/SysExCodec.h"

//==============================================================================
/** Placeholder content component. Real editor UI (params driven by
    params/xwp1.json) is built in later chunks. */
class PlaceholderComponent : public juce::Component
{
public:
    PlaceholderComponent() { setSize (640, 400); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
        g.drawText ("Casio XW-P1 Editor",
                    getLocalBounds().reduced (20).removeFromTop (120),
                    juce::Justification::centred, false);

        g.setColour (juce::Colours::grey);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("casioxw_core " + juce::String (casioxw::coreVersion()) + " — placeholder",
                    getLocalBounds().reduced (20),
                    juce::Justification::centred, false);
    }
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
            setContentOwned (new PlaceholderComponent(), true);
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
