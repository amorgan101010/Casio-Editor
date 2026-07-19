#include <juce_gui_basics/juce_gui_basics.h>

#include "BinaryData.h"

#include "AppVersion.h"
#include "EditorLookAndFeel.h"
#include "HexLayerPanel.h"
#include "OrganPanel.h"
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
    app/PCMEnginePanel.h), the Organ tab (category 0x07 "Drawbar", see app/OrganPanel.h), and the
    Hex Layer tab (category 0x08 "Hex Layer", see app/HexLayerPanel.h) are further siblings on the
    same shared MidiIO, with no device combo of their own — same pattern SequencerPanel already
    established. codec/midiIO must outlive every panel (SoloSynthPanel/PCMEnginePanel/OrganPanel/
    HexLayerPanel and ParamControl keep references into codec.model()), so they are declared
    before the panels in this class and never moved/copied afterwards. */
class MainContentComponent : public juce::Component
{
public:
    MainContentComponent()
        : codec (casioxw::ParamModel::fromFile (juce::File (CASIOXW_PARAMS_JSON))),
          soloSynthPanel (codec, midiIO),
          pcmEnginePanel (codec, midiIO),
          organPanel (codec, midiIO),
          hexLayerPanel (codec, midiIO),
          sequencerPanel (codec, midiIO)
    {
        // Capture the panels' natural sizes BEFORE tabbing them. addTab() reparents each panel into
        // the TabbedComponent's content area and resizes it to fit that area -- which is still 0x0
        // here -- so reading getWidth()/getHeight() afterwards yields 0. Reading it after was the
        // real cause of the window opening at 0x30 / tiny (bug-071, pinned by the launch diagnostic);
        // the panels' own constructors have already set these to the real values.
        // juce::jmax has no 5-arg overload (only 2/3/4-arg templates, see .wolf/cerebrum.md
        // bug-119) -- nest two 4-arg calls rather than an initializer-list form that doesn't exist.
        const int contentW = juce::jmax (juce::jmax (soloSynthPanel.getWidth(), pcmEnginePanel.getWidth(),
                                                       organPanel.getWidth(), hexLayerPanel.getWidth()),
                                          sequencerPanel.getWidth());
        const int contentH = juce::jmax (juce::jmax (soloSynthPanel.getHeight(), pcmEnginePanel.getHeight(),
                                                       organPanel.getHeight(), hexLayerPanel.getHeight()),
                                          sequencerPanel.getHeight());

        // TabbedComponent's colour arg only matters for its OWN default tab-content-area fill;
        // EditorLookAndFeel::drawTabButton/drawTabbedButtonBarBackground own the visible tab bar.
        tabs.addTab ("Solo Synth", EditorColours::chassisBg, &soloSynthPanel, false);
        tabs.addTab ("PCM Engine", EditorColours::chassisBg, &pcmEnginePanel, false);
        tabs.addTab ("Organ", EditorColours::chassisBg, &organPanel, false);
        tabs.addTab ("Hex Layer", EditorColours::chassisBg, &hexLayerPanel, false);
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
    OrganPanel organPanel;
    HexLayerPanel hexLayerPanel;
    SequencerPanel sequencerPanel;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};

//==============================================================================
/** Rasterises the embedded xw-p1.svg (BinaryData, see app/resources/) into a square ARGB image
    for use as the taskbar/window icon. NOTE: DocumentWindow::setIcon() does NOT set this --
    it only stores an icon JUCE's own custom-painted title bar would draw, which this app never
    uses (setUsingNativeTitleBar(true)). The real OS-level icon (X11 _NET_WM_ICON + legacy
    WM_HINTS, what window managers/taskbars actually read) is set via ComponentPeer::setIcon(),
    reachable through getPeer() -- and only once the peer exists, i.e. after setVisible(true). */
static juce::Image loadAppIconImage()
{
    auto svg = juce::XmlDocument::parse (juce::String (BinaryData::xwp1_svg, (size_t) BinaryData::xwp1_svgSize));
    std::unique_ptr<juce::Drawable> drawable = juce::Drawable::createFromSVG (*svg);

    constexpr int kIconSize = 256;
    juce::Image icon (juce::Image::ARGB, kIconSize, kIconSize, true);
    juce::Graphics g (icon);
    drawable->drawWithin (g, icon.getBounds().toFloat(), juce::RectanglePlacement::centred, 1.0f);
    return icon;
}

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

            const juce::Image appIcon = loadAppIconImage();
            setIcon (appIcon);   // harmless fallback for JUCE's own title bar, unused while native

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

            // The OS-level window/taskbar icon (X11 _NET_WM_ICON + legacy WM_HINTS) is set on the
            // ComponentPeer, not the DocumentWindow -- and the peer only exists once setVisible(true)
            // has run above.
            if (auto* peer = getPeer())
                peer->setIcon (appIcon);
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
