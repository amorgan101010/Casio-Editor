#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/** Shared colour tokens for the whole editor, one Solarized Dark swatch (the owner's own editor
    theme) reused everywhere instead of the ad hoc juce::Colours::orange/darkgrey/whitesmoke that
    used to be scattered per-panel. Canonical hex values from the Solarized spec, not eyeballed.
    Semantic slots keep the meanings the panels already had (selected==warm, filled/on==blue,
    has-locks==the other warm tone) so this is a retint, not a UX change. */
namespace EditorColours
{
    // ---- Solarized Dark base ramp (bg -> fg) -------------------------------------------------
    const juce::Colour base03 (0xff002b36);   // darkest — window/chassis background
    const juce::Colour base02 (0xff073642);   // panel/card surface, idle step fill
    const juce::Colour base01 (0xff586e75);   // muted text, hairlines/borders
    const juce::Colour base00 (0xff657b83);   // secondary/dim text
    const juce::Colour base0  (0xff839496);   // primary body text
    const juce::Colour base1  (0xff93a1a1);   // emphasized/header text

    // ---- Solarized accents -------------------------------------------------------------------
    const juce::Colour yellow  (0xffb58900);   // has-locks / secondary warm accent
    const juce::Colour orange  (0xffcb4b16);   // primary selection/active accent
    const juce::Colour red     (0xffdc322f);   // destructive/error (sparing use)
    const juce::Colour blue    (0xff268bd2);   // active/filled/"on" state
    const juce::Colour cyan    (0xff2aa198);   // playhead / connected / positive state
    const juce::Colour green   (0xff859900);   // transport "go" (Play) — reserved for that alone

    // ---- Semantic aliases used across panels -------------------------------------------------
    const juce::Colour chassisBg   = base03;
    const juce::Colour panelBg     = base02;
    const juce::Colour border      = base01;
    const juce::Colour textPrimary = base0;
    const juce::Colour textHeader  = base1;
    const juce::Colour textMuted   = base01;
    const juce::Colour selected    = orange;
    const juce::Colour hasLocks    = yellow;
    const juce::Colour filledStep  = blue;
    const juce::Colour idleStep    = base02;
    const juce::Colour playhead    = cyan;

    // ---- The "screen" (parameter display) palette island -------------------------------------
    // The sequencer's pageable parameter sub-window is drawn as a hardware LCD: darker than the
    // chassis so it reads as backlit glass, with phosphor-cyan content. Locked parameters render
    // INVERTED (amber fill, dark text) — the Digitakt manual's own convention for a locked value.
    const juce::Colour screenBg     (0xff001419);            // near-black teal glass
    const juce::Colour screenLine   = cyan.withAlpha (0.22f); // bezel inner line / dividers
    const juce::Colour screenText   (0xff9fc7c3);            // phosphor primary
    const juce::Colour screenDim    (0xff4a6a6e);            // phosphor secondary / idle labels
    const juce::Colour screenAccent = cyan;                  // value arcs / live emphasis
    const juce::Colour screenInvert = yellow;                // locked-cell fill (inverted graphics)
}

//==============================================================================
/** Shared font recipes, so the panels don't each hand-roll their own sizes/weights. The screen
    and every step numeral use the monospaced face (tabular digits read like a hardware display);
    section headers use the same bold-caps + tracking treatment as the tab bar. */
namespace EditorFonts
{
    inline juce::Font mono (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size,
                                              bold ? juce::Font::bold : juce::Font::plain));
    }

    inline juce::Font header (float size = 12.0f)
    {
        juce::Font f (juce::FontOptions (size, juce::Font::bold));
        f.setExtraKerningFactor (0.08f);
        return f;
    }
}

//==============================================================================
/** App-wide LookAndFeel: seeds LookAndFeel_V4's ColourScheme from the Solarized palette above
    (re-themes stock buttons/sliders/comboboxes/labels for free) then adds a handful of targeted
    overrides for the bits that carry the editor's own identity — a segmented hardware-style tab
    bar instead of JUCE's default curved tabs, rotary knobs drawn like a panel LED indicator
    (glass track + amber pointer) instead of the stock grey dial, and bold button-cap text instead
    of V4's regular weight. */
class EditorLookAndFeel : public juce::LookAndFeel_V4
{
public:
    EditorLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                            float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                            juce::Slider&) override;

    void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool isMouseOver, bool isMouseDown) override;
    void drawTabbedButtonBarBackground (juce::TabbedButtonBar&, juce::Graphics&) override;
    int getTabButtonBestWidth (juce::TabBarButton&, int tabDepth) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};
