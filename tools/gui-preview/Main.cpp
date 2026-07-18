// Offscreen component-snapshot renderer: constructs a real ParamControl (or SoloSynthPanel) and
// rasterizes it to a PNG via juce::Component::createComponentSnapshot(), with no window, no X11
// display, no native peer involved. A debugging aid for verifying GUI rendering/layout without
// needing a live display or the project owner's screen -- not part of the shipped app.
//
// Usage:
//   gui-preview knob <paramId> <instance> <outPath.png>
//   gui-preview panel <outPath.png>   (renders the full SoloSynthPanel at its default size)

#include "../../app/EditorLookAndFeel.h"
#include "../../app/ParamControl.h"
#include "../../app/SequencerPanel.h"
#include "../../app/SoloSynthPanel.h"
#include <casioxw/MidiIO.h>
#include <casioxw/ParamModel.h>
#include <casioxw/SysExCodec.h>

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdio>

namespace
{
    bool saveSnapshot (juce::Component& c, const juce::File& outFile)
    {
        c.setVisible (true);
        auto image = c.createComponentSnapshot (c.getLocalBounds());
        juce::PNGImageFormat png;
        auto stream = outFile.createOutputStream();
        if (stream == nullptr)
            return false;
        return png.writeImageToStream (image, *stream);
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;   // RAII, local (not static) -- safe per buglog bug-007

    static EditorLookAndFeel laf;   // static: must outlive every snapshot taken below
    juce::LookAndFeel::setDefaultLookAndFeel (&laf);

    if (argc < 2)
    {
        std::fprintf (stderr, "usage: gui-preview knob <paramId> <instance> <outPath.png>\n"
                               "       gui-preview panel <outPath.png>\n");
        return 1;
    }

    const juce::String mode = argv[1];
    const auto jsonPath = juce::File::getCurrentWorkingDirectory().getChildFile ("params/xwp1.json");
    if (! jsonPath.existsAsFile())
    {
        std::fprintf (stderr, "params/xwp1.json not found relative to cwd -- run from repo root\n");
        return 1;
    }

    if (mode == "knob" || mode == "bar" || mode == "fader")
    {
        if (argc < 5)
        {
            std::fprintf (stderr, "knob/bar/fader requires: <paramId> <instance> <outPath.png>\n");
            return 1;
        }
        auto model = casioxw::ParamModel::fromFile (jsonPath);
        const auto* info = model.find (juce::String (argv[2]));
        if (info == nullptr)
        {
            std::fprintf (stderr, "unknown paramId: %s\n", argv[2]);
            return 1;
        }
        const int instance = juce::String (argv[3]).getIntValue();
        const auto renderMode = mode == "knob"  ? ParamControl::RenderMode::Knob
                                : mode == "fader" ? ParamControl::RenderMode::VerticalFader
                                                   : ParamControl::RenderMode::Default;
        ParamControl ctrl (model, *info, instance, renderMode);
        const bool ok = saveSnapshot (ctrl, juce::File (argv[4]));
        std::printf (ok ? "wrote %s (size %dx%d)\n" : "FAILED to write %s\n",
                     argv[4], ctrl.getWidth(), ctrl.getHeight());
        return ok ? 0 : 1;
    }

    if (mode == "panel")
    {
        if (argc < 3)
        {
            std::fprintf (stderr, "panel requires: <outPath.png>\n");
            return 1;
        }
        auto model = casioxw::ParamModel::fromFile (jsonPath);
        casioxw::SysExCodec codec (std::move (model));
        casioxw::MidiIO midiIO;
        SoloSynthPanel panel (codec, midiIO);
        const bool ok = saveSnapshot (panel, juce::File (argv[2]));
        std::printf (ok ? "wrote %s (size %dx%d)\n" : "FAILED to write %s\n",
                     argv[2], panel.getWidth(), panel.getHeight());
        return ok ? 0 : 1;
    }

    if (mode == "sequencer" || mode == "sequencer-demo")
    {
        if (argc < 3)
        {
            std::fprintf (stderr, "sequencer requires: <outPath.png>\n");
            return 1;
        }
        auto model = casioxw::ParamModel::fromFile (jsonPath);
        casioxw::SysExCodec codec (std::move (model));
        casioxw::MidiIO midiIO;
        SequencerPanel panel (codec, midiIO);
        if (mode == "sequencer-demo")   // representative trigs/locks/selection/playhead state
            panel.applyPreviewDemoState();
        const bool ok = saveSnapshot (panel, juce::File (argv[2]));
        std::printf (ok ? "wrote %s (size %dx%d)\n" : "FAILED to write %s\n",
                     argv[2], panel.getWidth(), panel.getHeight());
        return ok ? 0 : 1;
    }

    std::fprintf (stderr, "unknown mode '%s' (expected knob|bar|panel|sequencer)\n", mode.toRawUTF8());
    return 1;
}
