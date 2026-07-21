// Offscreen component-snapshot renderer: constructs a real ParamControl (or SoloSynthPanel) and
// rasterizes it to a PNG via juce::Component::createComponentSnapshot(), with no window, no X11
// display, no native peer involved. A debugging aid for verifying GUI rendering/layout without
// needing a live display or the project owner's screen -- not part of the shipped app.
//
// Usage:
//   gui-preview knob <paramId> <instance> <outPath.png>
//   gui-preview panel <outPath.png>   (renders the full SoloSynthPanel at its default size)
//   gui-preview icon <outPath.png>    (renders the app's taskbar/window icon, see app/Main.cpp)

#include "BinaryData.h"

#include "../../app/EditorLookAndFeel.h"
#include "../../app/HexLayerPanel.h"
#include "../../app/OrganPanel.h"
#include "../../app/PCMEnginePanel.h"
#include "../../app/ParamControl.h"
#include "../../app/SequencerPanel.h"
#include "../../app/SoloSynthPanel.h"
#include "../../app/WavePicker.h"
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

    if (mode == "icon")
    {
        if (argc < 3)
        {
            std::fprintf (stderr, "icon requires: <outPath.png>\n");
            return 1;
        }
        // Same rasterization app/Main.cpp's loadAppIconImage() does -- kept in sync by hand since
        // it's ~8 lines; a genuine second offscreen consumer would be worth sharing properly.
        auto svg = juce::XmlDocument::parse (juce::String (BinaryData::xwp1_svg, (size_t) BinaryData::xwp1_svgSize));
        std::unique_ptr<juce::Drawable> drawable = juce::Drawable::createFromSVG (*svg);
        constexpr int kIconSize = 256;
        juce::Image icon (juce::Image::ARGB, kIconSize, kIconSize, true);
        juce::Graphics g (icon);
        drawable->drawWithin (g, icon.getBounds().toFloat(), juce::RectanglePlacement::centred, 1.0f);

        juce::PNGImageFormat png;
        auto stream = juce::File (argv[2]).createOutputStream();
        const bool ok = stream != nullptr && png.writeImageToStream (icon, *stream);
        std::printf (ok ? "wrote %s (size %dx%d)\n" : "FAILED to write %s\n", argv[2], kIconSize, kIconSize);
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

    if (mode == "pcm")
    {
        if (argc < 3)
        {
            std::fprintf (stderr, "pcm requires: <outPath.png>\n");
            return 1;
        }
        auto model = casioxw::ParamModel::fromFile (jsonPath);
        casioxw::SysExCodec codec (std::move (model));
        casioxw::MidiIO midiIO;
        PCMEnginePanel panel (codec, midiIO);
        const bool ok = saveSnapshot (panel, juce::File (argv[2]));
        std::printf (ok ? "wrote %s (size %dx%d)\n" : "FAILED to write %s\n",
                     argv[2], panel.getWidth(), panel.getHeight());
        return ok ? 0 : 1;
    }

    if (mode == "organ")
    {
        if (argc < 3)
        {
            std::fprintf (stderr, "organ requires: <outPath.png>\n");
            return 1;
        }
        auto model = casioxw::ParamModel::fromFile (jsonPath);
        casioxw::SysExCodec codec (std::move (model));
        casioxw::MidiIO midiIO;
        OrganPanel panel (codec, midiIO);
        const bool ok = saveSnapshot (panel, juce::File (argv[2]));
        std::printf (ok ? "wrote %s (size %dx%d)\n" : "FAILED to write %s\n",
                     argv[2], panel.getWidth(), panel.getHeight());
        return ok ? 0 : 1;
    }

    if (mode == "hexlayer")
    {
        if (argc < 3)
        {
            std::fprintf (stderr, "hexlayer requires: <outPath.png> [width] [height]\n");
            return 1;
        }
        auto model = casioxw::ParamModel::fromFile (jsonPath);
        casioxw::SysExCodec codec (std::move (model));
        casioxw::MidiIO midiIO;
        HexLayerPanel panel (codec, midiIO);
        // Optional width/height: the panel's own natural size (below) is narrow enough that its
        // Layer-block card grid only ever shows one column -- Main.cpp actually sizes every tab to
        // the widest tab in the shared TabbedComponent (Sequencer, ~1490px), which is what lets the
        // 2x3 grid wrap to 3 columns in the real app. Pass an explicit size to preview that.
        if (argc >= 5)
            panel.setSize (std::atoi (argv[3]), std::atoi (argv[4]));
        const bool ok = saveSnapshot (panel, juce::File (argv[2]));
        std::printf (ok ? "wrote %s (size %dx%d)\n" : "FAILED to write %s\n",
                     argv[2], panel.getWidth(), panel.getHeight());
        return ok ? 0 : 1;
    }

    if (mode == "sequencer" || mode == "sequencer-demo" || mode == "sequencer-pcm-demo"
        || mode == "sequencer-hex-demo" || mode == "sequencer-arranger-demo" || mode == "sequencer-poly-demo")
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
        if (mode == "sequencer-demo")       // representative trigs/locks/selection/playhead state
            panel.applyPreviewDemoState();
        else if (mode == "sequencer-pcm-demo")   // a PCM track's step selected, screen showing NOTE/GATE/VEL
            panel.applyPcmStepEditPreviewState();
        else if (mode == "sequencer-hex-demo")   // engine switched to Hex Layer, a step p-locked
            panel.applyHexLayerPreviewState();
        else if (mode == "sequencer-arranger-demo")   // Arranger mode, a few representative rows
            panel.applyArrangerPreviewState();
        else if (mode == "sequencer-poly-demo")   // Chords row poly mode expanded with a triad
            panel.applyPolyPreviewState();
        const bool ok = saveSnapshot (panel, juce::File (argv[2]));
        std::printf (ok ? "wrote %s (size %dx%d)\n" : "FAILED to write %s\n",
                     argv[2], panel.getWidth(), panel.getHeight());
        return ok ? 0 : 1;
    }

    if (mode == "wavepicker-bench")
    {
        // Times how long it takes to OPEN a WavePicker for each of the app's large wave enums --
        // the exact class of freeze reported live against the old juce::ComboBox-backed picker
        // (bug: soloPcmWaves' 2158-entry dropdown hangs the app). A ComboBox popup for a list
        // this size does O(items) Component construction + text measurement before it can even
        // show; WavePicker's popup only builds a search box + an empty-until-scrolled ListBox, so
        // opening it should cost single-digit milliseconds regardless of list size -- this proves
        // that experimentally rather than by inspection alone (gui-preview snapshots alone can't:
        // createComponentSnapshot() never opens a popup, see the hexlayer mode's own history).
        auto model = casioxw::ParamModel::fromFile (jsonPath);
        const char* enumNames[] = { "soloSynthWaves", "soloPcmWaves", "hexLayerWaves" };
        bool allOk = true;
        for (const auto* name : enumNames)
        {
            const auto* entries = model.enumValues (juce::String (name));
            if (entries == nullptr)
            {
                std::printf ("%-16s MISSING from model.enums\n", name);
                allOk = false;
                continue;
            }
            WavePicker picker;
            picker.setEntries (entries);
            picker.setBounds (0, 0, 200, 24);
            picker.setVisible (true);

            const auto start = juce::Time::getMillisecondCounterHiRes();
            picker.triggerOpenForPreview();
            const auto elapsedMs = juce::Time::getMillisecondCounterHiRes() - start;

            std::printf ("%-16s %5d entries: open() took %.2f ms\n",
                         name, (int) entries->size(), elapsedMs);
            if (elapsedMs > 200.0)
                allOk = false;
        }
        std::printf (allOk ? "PASS: all pickers opened in well under a freeze-worthy time\n"
                            : "FAIL: at least one picker was slow or missing\n");
        return allOk ? 0 : 1;
    }

    if (mode == "sequencer-pcm-roundtrip")
    {
        // Headless correctness check for PCM track save/load -- no snapshot, no display needed.
        auto model = casioxw::ParamModel::fromFile (jsonPath);
        casioxw::SysExCodec codec (std::move (model));
        casioxw::MidiIO midiIO;
        SequencerPanel panel (codec, midiIO);
        const bool ok = panel.verifyPcmRoundTripForPreview();
        std::printf (ok ? "PASS: PCM tracks round-trip through serialize/apply intact\n"
                         : "FAIL: PCM tracks round-trip lost data\n");
        return ok ? 0 : 1;
    }

    if (mode == "sequencer-solo-poly-roundtrip")
    {
        // Headless correctness check for the solo lane's poly save/load path, which is separate
        // code from the PCM check above (serializeSoloSequenceToJson()/applySoloSequenceText()).
        auto model = casioxw::ParamModel::fromFile (jsonPath);
        casioxw::SysExCodec codec (std::move (model));
        casioxw::MidiIO midiIO;
        SequencerPanel panel (codec, midiIO);
        const bool ok = panel.verifySoloPolyRoundTripForPreview();
        std::printf (ok ? "PASS: solo-lane poly state round-trips through serialize/apply intact\n"
                         : "FAIL: solo-lane poly state round-trip lost data\n");
        return ok ? 0 : 1;
    }

    std::fprintf (stderr, "unknown mode '%s' (expected knob|bar|panel|pcm|organ|hexlayer|icon|sequencer|sequencer-demo|sequencer-pcm-demo|sequencer-hex-demo|sequencer-arranger-demo|sequencer-poly-demo|sequencer-pcm-roundtrip|sequencer-solo-poly-roundtrip|wavepicker-bench)\n", mode.toRawUTF8());
    return 1;
}
