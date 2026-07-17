#include "ParamControl.h"

using casioxw::ControlKind;

namespace
{
    constexpr int kLabelWidth = 170;
    constexpr int kControlHeight = 24;

    // Chunk 7d item 2: knob-mode cell size — deliberately small/fixed so several fit per row in
    // SoloSynthPanel's wrapping grid (see kKnobCellWidth/Height there, which must match this).
    // 88 wide gives a little more breathing room than a bare 80 for longer labels like
    // "OSC Waveform" / "Key Follow Base" before minimumHorizontalScale has to kick in.
    constexpr int kKnobWidth = 88;
    constexpr int kKnobHeight = 92;
    constexpr int kKnobLabelHeight = 16;

    // Chunk 7e item 3: vertical-fader cell size — narrow so 9 (one full envelope shape) fit
    // side-by-side in one row without wrapping on SoloSynthPanel's ~860px default content width
    // (9 * 56 = 504px, comfortably under that). Must match kFaderCellWidth/Height in
    // SoloSynthPanel.cpp, same "ParamControl owns its own size, the grid just tiles it" pattern
    // as the knob cell above. Taller than the knob cell (150 vs 92) so the vertical throw is
    // actually usable — a cramped vertical slider is worse than a horizontal bar, not better.
    constexpr int kFaderWidth = 56;
    constexpr int kFaderHeight = 150;
    // Still one line (same shrink-to-fit + tooltip trick as knob mode below) but envelope-stage
    // names run longer ("Pitch Env Rel1 Time") than a typical knob label, so a little extra label
    // height reduces how aggressively minimumHorizontalScale has to shrink the font.
    constexpr int kFaderLabelHeight = 20;
    constexpr int kCompactTextBoxHeight = 16;   // shared by both compact modes' value text box

    juce::String displayName (const casioxw::ParamInfo& info)
    {
        juce::String s = info.name.isNotEmpty() ? info.name : info.id;
        // "note" is a DISPLAY FORMATTER SELECTOR (-> Slider::textFromValueFunction below), not a
        // unit of measure — appending "(note)" to every Key Follow Base label would be noise.
        if (info.unit.isNotEmpty() && info.unit != "note")
            s << " (" << info.unit << ")";
        return s;
    }
}

ParamControl::ParamControl (const casioxw::ParamModel& model, const casioxw::ParamInfo& infoIn, int instanceIn,
                             RenderMode modeIn)
    : info (infoIn), instance (instanceIn), kind (casioxw::decideControlKind (info, instance)),
      mode (kind == ControlKind::Slider ? modeIn : RenderMode::Default)
{
    const bool knobMode = mode == RenderMode::Knob;
    const bool faderMode = mode == RenderMode::VerticalFader;
    const bool compact = knobMode || faderMode;

    setSize (knobMode ? kKnobWidth : faderMode ? kFaderWidth : kLabelWidth + 220,
             knobMode ? kKnobHeight : faderMode ? kFaderHeight : kControlHeight);

    nameLabel.setText (displayName (info), juce::dontSendNotification);
    nameLabel.setJustificationType (compact ? juce::Justification::centred
                                             : juce::Justification::centredLeft);
    if (compact)
    {
        // Both compact cells are narrower than most param names ("OSC Waveform", "Key Follow
        // Base", "Pitch Env Rel1 Time" once the unit suffix is appended) — shrink-to-fit rather
        // than hard-truncate, and keep the full name reachable via tooltip.
        nameLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
        nameLabel.setMinimumHorizontalScale (0.6f);
        nameLabel.setTooltip (displayName (info));
    }
    addAndMakeVisible (nameLabel);

    switch (kind)
    {
        case ControlKind::Toggle:
        {
            toggle = std::make_unique<juce::ToggleButton>();
            toggle->onClick = [this] { notify (toggle->getToggleState() ? 1 : 0); };
            addAndMakeVisible (*toggle);
            break;
        }

        case ControlKind::ComboEnum:
        case ControlKind::ComboEnumPerOsc:
        {
            combo = std::make_unique<juce::ComboBox>();
            const juce::String enumName = casioxw::resolveEnumName (info, instance);
            if (const auto* entries = model.enumValues (enumName))
                for (const auto& e : *entries)
                    combo->addItem (e.label, e.value + 1);   // JUCE ids are 1-based; 0 is illegal
            combo->onChange = [this]
            {
                const int id = combo->getSelectedId();
                if (id > 0)
                    notify (id - 1);
            };
            addAndMakeVisible (*combo);
            break;
        }

        case ControlKind::ComboRange:
        {
            combo = std::make_unique<juce::ComboBox>();
            for (int v = info.range.min; v <= info.range.max; ++v)
                combo->addItem (juce::String (v), (v - info.range.min) + 1);
            combo->onChange = [this]
            {
                const int id = combo->getSelectedId();
                if (id > 0)
                    notify ((id - 1) + info.range.min);
            };
            addAndMakeVisible (*combo);
            break;
        }

        case ControlKind::Slider:
        {
            // Chunk 7e item 3: the 9 envelope-stage params (SoloSynthPanel's per-param
            // envelopeStageIds() check) render as a compact VERTICAL fader instead of the
            // original full-width bar, laid out side-by-side in a row by the owning panel — the
            // actual space-saving move (9 vertical faders in a row vs. 9 full-width bars
            // stacked). Every other Slider-kind param still renders as a compact rotary knob
            // (Chunk 7d item 2, unchanged); anything left over stays the default full-width bar.
            const auto sliderStyle = knobMode  ? juce::Slider::RotaryHorizontalVerticalDrag
                                    : faderMode ? juce::Slider::LinearVertical
                                                : juce::Slider::LinearHorizontal;
            const auto textBoxPos = compact ? juce::Slider::TextBoxBelow : juce::Slider::TextBoxRight;
            slider = std::make_unique<juce::Slider> (sliderStyle, textBoxPos);
            slider->setRange ((double) info.range.min, (double) info.range.max, 1.0);
            slider->onValueChange = [this] { notify ((int) slider->getValue()); };
            if (knobMode)
                slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, kKnobWidth - 8, kCompactTextBoxHeight);
            else if (faderMode)
                slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, kFaderWidth - 4, kCompactTextBoxHeight);

            // Key Follow Base params (unit=="note", metadata-driven per gen_xwp1.py's OVR table,
            // not a hardcoded param-id list): show/accept MIDI note names ("C-1".."G9") instead
            // of raw integers, via casioxw::midiNoteName()/noteNameToMidi() (Chunk 7c item 3).
            if (info.unit == "note")
            {
                slider->textFromValueFunction = [] (double v) { return casioxw::midiNoteName ((int) v); };
                slider->valueFromTextFunction = [] (const juce::String& t) -> double
                {
                    const auto n = casioxw::noteNameToMidi (t);
                    return n.has_value() ? (double) *n : 0.0;
                };
                // NOT slider->setValue(slider->getValue(), ...) -- Slider::setValue() no-ops when
                // the new value equals the current one (no repaint), so that "refresh" silently
                // did nothing whenever the default being displayed was already correct (e.g. 0).
                // updateText() is JUCE's purpose-built call for "re-render the text box now".
                slider->updateText();
            }

            addAndMakeVisible (*slider);
            break;
        }

        case ControlKind::Disabled:
        {
            nameLabel.setText (displayName (info) + "  (n/a for this instance)", juce::dontSendNotification);
            nameLabel.setEnabled (false);
            break;
        }
    }

    if (info.defaultValue.has_value() && kind != ControlKind::Disabled)
        setValueFromSync (*info.defaultValue);

    // setSize() above ran before toggle/combo/slider existed (they're constructed lazily inside
    // the switch, since a ParamControl is exactly ONE of several mutually-exclusive widget types
    // -- unlike e.g. a fixed pair of always-present knobs). That first resized() pass therefore
    // skipped positioning the widget entirely (resized()'s null guard). Whether anything ever
    // repositions it again depends on the CALLER's later setBounds() using a genuinely different
    // size -- true for every list-style control (SoloSynthPanel assigns the real container width,
    // which differs from this constructor's default and so DOES re-trigger resized()), but FALSE
    // for compact-mode controls (Knob AND VerticalFader), where the owning grid/row layout reuses
    // this exact same fixed cell size, so JUCE's setBounds() sees no size change and never calls
    // resized() again -- leaving the widget stuck at its default zero bounds forever (confirmed
    // via tools/gui-preview: a rendered knob produced zero non-text pixels before this fix; same
    // check re-run against the new vertical-fader mode below). Same failure class as
    // bug-009/MainWindow's fix; force one final, correct layout pass now that the widget is
    // guaranteed to exist, regardless of what the caller does with setBounds() afterward.
    resized();
}

void ParamControl::notify (int uiValue)
{
    if (onValueChanged)
        onValueChanged (uiValue);
}

void ParamControl::setValueFromSync (int value)
{
    switch (kind)
    {
        case ControlKind::Toggle:
            if (toggle != nullptr)
                toggle->setToggleState (value != 0, juce::dontSendNotification);
            break;

        case ControlKind::ComboEnum:
        case ControlKind::ComboEnumPerOsc:
            // Clamp before display: a synced value can legitimately fall outside the UI's known
            // range (e.g. tssOSCwf decoding to -1 when the real hardware's wire value is 0 --
            // verified against franky's own SX2v.wf formula and reproducible across repeated
            // reads, not a decode bug -- the Lua's own encoder comment notes waveform wire values
            // "start at 1", so 0 is simply outside the normal domain for this tone's current
            // state). Without this, setSelectedId() on an out-of-range id silently selects
            // nothing (JUCE combo id 0 = no selection), leaving the combo looking broken/blank
            // with no indication why. Clamping to the nearest real entry keeps the display honest
            // (shows a plausible value) rather than mysterious.
            if (combo != nullptr && combo->getNumItems() > 0)
                combo->setSelectedId (juce::jlimit (0, combo->getNumItems() - 1, value) + 1,
                                      juce::dontSendNotification);
            break;

        case ControlKind::ComboRange:
            if (combo != nullptr)
            {
                const int clamped = juce::jlimit (info.range.min, info.range.max, value);
                combo->setSelectedId ((clamped - info.range.min) + 1, juce::dontSendNotification);
            }
            break;

        case ControlKind::Slider:
            // juce::Slider clamps to its own setRange() bounds internally, so this is defensive
            // consistency with the combo cases above rather than a required fix on its own.
            if (slider != nullptr)
                slider->setValue (juce::jlimit ((double) info.range.min, (double) info.range.max,
                                                (double) value),
                                  juce::dontSendNotification);
            break;

        case ControlKind::Disabled:
            break;   // nothing to display
    }
}

void ParamControl::resized()
{
    auto bounds = getLocalBounds();

    if (mode == RenderMode::Knob)
    {
        nameLabel.setBounds (bounds.removeFromTop (kKnobLabelHeight));
        if (slider != nullptr)
            slider->setBounds (bounds);
        return;
    }

    if (mode == RenderMode::VerticalFader)
    {
        nameLabel.setBounds (bounds.removeFromTop (kFaderLabelHeight));
        if (slider != nullptr)
            slider->setBounds (bounds);
        return;
    }

    nameLabel.setBounds (bounds.removeFromLeft (kLabelWidth));

    if (toggle != nullptr)
        toggle->setBounds (bounds.removeFromLeft (60));
    else if (combo != nullptr)
        combo->setBounds (bounds);
    else if (slider != nullptr)
        slider->setBounds (bounds);
}
