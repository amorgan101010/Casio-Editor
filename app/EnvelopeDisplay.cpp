#include "EditorLookAndFeel.h"
#include "EnvelopeDisplay.h"

namespace
{
    // Colour sequence lifted straight from franky's reference/lua/017_ENVpaint.lua drawENV()
    // calls (drawENV(gobj, os, line, startColour, endColour) per segment) — same visual language
    // as the parameter's own hardware editor, not an arbitrary new palette. Left untouched by the
    // Solarized retint: these are hardware-accurate, not a stylistic choice.
    const juce::Colour kYellow ((juce::uint8) 0xff, (juce::uint8) 0xff, (juce::uint8) 0x00);
    const juce::Colour kRed    ((juce::uint8) 0xff, (juce::uint8) 0x00, (juce::uint8) 0x00);
    const juce::Colour kOrange ((juce::uint8) 0xf9, (juce::uint8) 0xa6, (juce::uint8) 0x02);
    const juce::Colour kGreen  ((juce::uint8) 0x00, (juce::uint8) 0xff, (juce::uint8) 0x00);
    // Bg/zero-line DO follow the app palette — this is chrome, not part of the Lua's own drawing.
    const juce::Colour kBg     = EditorColours::chassisBg;
    const juce::Colour kZeroLine = EditorColours::cyan;

    // Fixed visual width (in the same "time units" as the 0..127 time params) given to the
    // sustain hold segment, which has no time value of its own — it's held indefinitely while a
    // key is down. Matches the Lua's own fixed 75-unit sustain width (017_ENVpaint.lua:55).
    constexpr float kSustainVisualUnits = 75.0f;
    constexpr float kPad = 6.0f;
}

EnvelopeDisplay::EnvelopeDisplay (int levelMinIn, int levelMaxIn)
    : levelMin (levelMinIn), levelMax (levelMaxIn)
{
    setSize (300, 90);
}

void EnvelopeDisplay::setStage (Stage stage, int value)
{
    jassert (stage >= 0 && stage < kNumStages);
    values[(size_t) stage] = value;
    repaint();
}

float EnvelopeDisplay::normLevel (int v) const noexcept
{
    const int span = levelMax - levelMin;
    if (span <= 0)
        return 0.0f;
    return juce::jlimit (0.0f, 1.0f, (float) (v - levelMin) / (float) span);
}

float EnvelopeDisplay::normTime (int v) noexcept
{
    return juce::jlimit (0.0f, 1.0f, (float) v / 127.0f);
}

void EnvelopeDisplay::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    const auto bounds = getLocalBounds().toFloat().reduced (kPad);
    if (bounds.getWidth() <= 1.0f || bounds.getHeight() <= 1.0f)
        return;

    const int initLevel = values[InitLevel];
    const int attackTime = values[AttackTime];
    const int attackLevel = values[AttackLevel];
    const int decayTime = values[DecayTime];
    const int sustainLevel = values[SustainLevel];
    const int rel1Time = values[Release1Time];
    const int rel1Level = values[Release1Level];
    const int rel2Time = values[Release2Time];
    const int rel2Level = values[Release2Level];

    // x-axis: sequential segments sized by their time value (0..127), except the sustain hold
    // which gets a fixed visual width (see kSustainVisualUnits) since it isn't itself a time
    // parameter. Guard against an all-zero envelope (nothing synced yet) with a floor so the
    // layout math never divides by zero.
    const float totalUnits = juce::jmax (1.0f, (float) attackTime + (float) decayTime
                                                + kSustainVisualUnits
                                                + (float) rel1Time + (float) rel2Time);
    const float pxPerUnit = bounds.getWidth() / totalUnits;

    auto xAt = [&] (float cumulativeUnits) { return bounds.getX() + cumulativeUnits * pxPerUnit; };
    auto yAt = [&] (int level)
    {
        return bounds.getY() + (1.0f - normLevel (level)) * bounds.getHeight();
    };

    // Zero-level reference line, matching 017_ENVpaint.lua's own "draw y=0 line" — meaningful
    // for centered (pitch) envelopes; harmless (draws at the plot's own bottom) for 0..127 ones.
    g.setColour (kZeroLine.withAlpha (0.6f));
    g.drawHorizontalLine ((int) yAt (0), bounds.getX(), bounds.getRight());

    struct Point { float x, y; };
    const Point p0 { xAt (0.0f), yAt (initLevel) };
    const Point p1 { xAt ((float) attackTime), yAt (attackLevel) };
    const Point p2 { xAt ((float) attackTime + (float) decayTime), yAt (sustainLevel) };
    const Point p3 { xAt ((float) attackTime + (float) decayTime + kSustainVisualUnits), yAt (sustainLevel) };
    const Point p4 { xAt ((float) attackTime + (float) decayTime + kSustainVisualUnits + (float) rel1Time),
                      yAt (rel1Level) };
    const Point p5 { xAt ((float) attackTime + (float) decayTime + kSustainVisualUnits
                           + (float) rel1Time + (float) rel2Time),
                      yAt (rel2Level) };

    auto drawSegment = [&] (Point a, Point b, juce::Colour startCol, juce::Colour endCol)
    {
        g.setColour (startCol.interpolatedWith (endCol, 0.5f));
        g.drawLine (a.x, a.y, b.x, b.y, 2.0f);
        g.setColour (startCol);
        g.fillEllipse (a.x - 3.0f, a.y - 3.0f, 6.0f, 6.0f);
        g.setColour (endCol);
        g.fillEllipse (b.x - 3.0f, b.y - 3.0f, 6.0f, 6.0f);
    };

    drawSegment (p0, p1, kYellow, kRed);      // Attack
    drawSegment (p1, p2, kRed, kOrange);      // Decay
    drawSegment (p2, p3, kOrange, kOrange);   // Sustain hold
    drawSegment (p3, p4, kOrange, kGreen);    // Release 1
    drawSegment (p4, p5, kGreen, kYellow);    // Release 2
}
