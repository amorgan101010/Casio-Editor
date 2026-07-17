#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/** Read-only graphical envelope display (Chunk 7c item 5) — draws the 9-point shape (Init
    Level, Attack Time/Level, Decay Time -> Sustain Level, Release1 Time/Level, Release2
    Time/Level) exactly as diagrammed in the manual (page E-24) and as franky's
    reference/lua/017_ENVpaint.lua draws it on the real hardware's own editor panel.

    Values are pushed in via setStage()/setAll() — this component does not read SysEx/ParamModel
    itself; the owning panel (SoloSynthPanel) feeds it from live control changes and sync
    replies, keyed by casioxw::envelopeStageIds(). repaint()s on every update, so it stays live.

    v1 scope: READ-ONLY. No mouse/drag interaction — that's an explicitly out-of-scope future
    chunk (interactive envelope editing). */
class EnvelopeDisplay : public juce::Component
{
public:
    /** @param levelMin/levelMax  the level axis' value range, taken from the envelope group's
                                   own param metadata (pitch envelopes are -64..+63 centered;
                                   filter/amp envelopes are 0..127) — normalization must follow
                                   the JSON, not a hardcoded assumption (see cerebrum). */
    EnvelopeDisplay (int levelMinIn, int levelMaxIn);

    /** Stage indices, matching casioxw::EnvelopeStageIds' field order. */
    enum Stage
    {
        InitLevel = 0, AttackTime, AttackLevel, DecayTime, SustainLevel,
        Release1Time, Release1Level, Release2Time, Release2Level,
        kNumStages
    };

    /** Update one stage value and repaint. Time stages (AttackTime/DecayTime/Release1Time/
        Release2Time) are always 0..127; level stages are normalized against levelMin/levelMax. */
    void setStage (Stage stage, int value);

    void paint (juce::Graphics&) override;

private:
    int levelMin, levelMax;
    int values[kNumStages] = {};

    float normLevel (int v) const noexcept;   // -> 0..1, 0 = bottom of the plot
    static float normTime (int v) noexcept;   // 0..127 -> 0..1

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnvelopeDisplay)
};
