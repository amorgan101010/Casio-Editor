#pragma once

#include <juce_core/juce_core.h>

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace casioxw
{
    /** One logical XW-P1 parameter, distilled from params/xwp1.json.

        The codec-facing fields (id/section/ct/addr/ai/an/vt/instanceCount/
        addressByteIndex/waveBaseOffset) are unchanged from Chunk 5/6 — SysExCodec
        depends only on those and nothing here may change their meaning.

        Everything below "UI metadata" is ADDITIVE (Chunk 7a): the same single
        parse of xwp1.json now also carries the display/range/enum metadata the
        GUI needs, so there is exactly one JSON parser for both the codec and
        the UI. None of it is read by SysExCodec. */
    struct ParamInfo
    {
        juce::String id;              // e.g. "tssOSCwf"
        juce::String section;         // e.g. "soloSynth"
        int ct   = 0;                 // category byte (0x09 solo synth)
        int addr = 0;                 // parameter id byte (address.id)
        int ai   = 0;                 // array index byte
        int an   = 0;                 // array number byte
        juce::String vt;              // value-type: nf cf cF pk tn wf ...

        int instanceCount   = 1;      // number of blocks (OSC=6, PWM/LFO=2, else 1)
        int addressByteIndex = 10;    // index within the 18-byte address of the per-instance byte

        // For vt=="wf": per-oscillator (1-based) wave base offset (P1 model).
        std::map<int, int> waveBaseOffset;

        // ---- UI metadata (additive; SysExCodec never reads any of this) --------------------
        juce::String name;            // display label, e.g. "OSC Waveform"
        juce::String block;           // e.g. "OSC", "PWM", "Etc", "TotalFilter", "LFO"
        juce::String group;           // Chunk 7c: visual sub-grouping within a block's param
                                       // list, e.g. "Pitch Envelope" — see ParamModel::groupOrder()
        juce::String note;            // optional free-text implementer note

        struct Range { int min = 0; int max = 0; };
        Range range;

        std::optional<int> defaultValue;   // "default" in JSON; absent/null -> nullopt
        juce::String unit;

        struct UiInfo
        {
            juce::String control;     // "slider" | "combo" | "toggle"
            juce::String enumName;    // ui.enum — set when one enum backs every instance
            bool enumPerOsc = false;  // ui.enumPerOsc — enum source depends on instance
        };
        UiInfo ui;

        juce::StringArray instanceLabels;  // instances.labels, e.g. OSC's Synth1.."Noise"

        // For combo params with ui.enumPerOsc / vt=="wf": per-instance (1-based) enum name.
        // An instance absent here (JSON null, e.g. OSC5/OSC6 wave) has no enum -> disabled.
        std::map<int, juce::String> enumPerOscByInstance;

        // For vt=="wf" / enumPerOsc params: per-instance (1-based) UI value upper bound.
        std::map<int, int> maxPerOsc;
    };

    /** One {value,label} entry from the top-level xwp1.json "enums" object
        (e.g. "soloSynthWaves", "filterType"). */
    struct EnumEntry
    {
        int value = 0;
        juce::String label;
    };

    /** How a ParamControl should render a given (ParamInfo, instance) pair. Kept as a pure
        function of already-parsed metadata (no JUCE GUI types involved) specifically so the
        decision logic is unit-testable from casioxw_tests, which links casioxw_core but not
        juce_gui_basics. */
    enum class ControlKind
    {
        Toggle,           // ui.control == "toggle" (on/off, range 0..1)
        ComboEnum,        // ui.control == "combo", one enum (ui.enum) backs every instance
        ComboEnumPerOsc,  // ui.control == "combo", ui.enumPerOsc == true, this instance has an enum
        ComboRange,       // ui.control == "combo", no enum at all (e.g. tssLFOsync) — numeric items
        Slider,           // ui.control == "slider" (or any unrecognised value, defensively)
        Disabled          // ui.enumPerOsc == true but this instance has no enum (e.g. OSC5/OSC6 wave)
    };

    /** Decide which widget a ParamControl should show for `info` at the given 1-based
        `instance`. Pure function of already-parsed ParamInfo metadata — no ParamModel lookup
        needed, no JUCE GUI type involved. */
    ControlKind decideControlKind (const ParamInfo& info, int instance);

    /** The enums-table name that backs `info` at the given 1-based `instance` (resolves
        ui.enum or perOsc.enumPerOsc[instance] as appropriate). Empty string when there is no
        backing enum (ComboRange, Slider, Toggle, or a disabled per-osc instance). Look the
        result up with ParamModel::enumValues(). */
    juce::String resolveEnumName (const ParamInfo& info, int instance);

    /** The 9 sibling param ids making up a single envelope group (Chunk 7c item 5), e.g. given
        "tssOSCPENViL" (or any of its 8 siblings) returns { "tssOSCPENViL", "tssOSCPENVaT", ...,
        "tssOSCPENVr2L" }. Field order matches the manual's own IL/AT/AL/DT/SL/RT1/RL1/RT2/RL2
        envelope diagram (E-24), and franky's 017_ENVpaint.lua drawing order. */
    struct EnvelopeStageIds
    {
        juce::String initLevel, attackTime, attackLevel, decayTime, sustainLevel,
                      release1Time, release1Level, release2Time, release2Level;

        bool isValid() const noexcept { return initLevel.isNotEmpty(); }
    };

    /** Derives the 9 sibling ids from any one envelope-stage param id by string substitution
        (pure — no ParamModel lookup needed). `anyEnvParamId` must contain "ENV" followed by one
        of the 9 known stage suffixes (iL/aT/aL/dT/sL/r1T/r1L/r2T/r2L); anything else returns a
        default-constructed (invalid, see isValid()) result rather than guessing. */
    EnvelopeStageIds envelopeStageIds (const juce::String& anyEnvParamId);

    /** True if `group` is one of the 9-stage envelope groups (e.g. "Pitch Envelope") as opposed
        to a plain param group (e.g. "Pitch", "LFO"). Purely a naming convention set by
        gen_xwp1.py's group_for() — every "*Envelope" group's members are drawn from the 9
        ENV-suffixed stage ids (see envelopeStageIds()). Pure string check, no ParamModel lookup
        needed — used by the app layer both to decide whether to show an EnvelopeDisplay above a
        group's controls (Chunk 7c item 5) and whether a plain Slider-kind param in that group
        should render as a full-width bar (envelope stages, unchanged) or a compact rotary knob
        (everything else, Chunk 7d item 2) — see SoloSynthPanel::rebuildParamControls(). */
    bool isEnvelopeGroup (const juce::String& group);

    /** Loads and indexes the XW-P1 parameter map. GUI-less. */
    class ParamModel
    {
    public:
        /** Parse from a params/xwp1.json file. Throws std::runtime_error on failure. */
        static ParamModel fromFile (const juce::File& jsonFile);

        /** Parse from a JSON string (same schema as the file). */
        static ParamModel fromJsonString (const juce::String& json);

        /** Find a parameter by logical id (e.g. "tssOSCwf"). */
        const ParamInfo* find (const juce::String& id) const;

        const std::vector<ParamInfo>& all() const noexcept { return params; }

        /** Reverse lookup by 18-byte address key (space-free lowercase hex, 36 chars).
            A key may map to MORE THAN ONE param when the Lua source has an address
            collision (the flagged lfo1D/2D typos); all candidates are returned.
            paramIndex indexes all(); resolve with paramAt(). */
        struct AddressHit
        {
            int paramIndex = -1;
            int instance   = 1;         // 1-based
        };
        const std::vector<AddressHit>* lookupAddress (const juce::String& key18) const;

        const ParamInfo& paramAt (int index) const { return params[(size_t) index]; }

        /** True if the given 18-byte address key resolves to more than one param. */
        bool isAmbiguous (const juce::String& key18) const;

        /** Look up a top-level "enums" table by name (e.g. "soloSynthWaves", "filterType").
            Returns nullptr if no such enum exists. Use with resolveEnumName() to find the
            right table name for a given ParamInfo/instance. */
        const std::vector<EnumEntry>* enumValues (const juce::String& name) const;

        /** Top-level "groupOrder" array (Chunk 7c) — the canonical display order for
            ParamInfo::group values, single source of truth (params/xwp1.json), not hardcoded in
            the UI layer. May be empty if the JSON omits it. Use with orderedGroupsForBlock(). */
        const std::vector<juce::String>& groupOrder() const noexcept { return groupOrderList; }

    private:
        void index();

        std::vector<ParamInfo> params;
        std::map<juce::String, int> byId;                          // id -> index into params
        std::map<juce::String, std::vector<AddressHit>> byAddress;  // 18-byte key -> hits
        std::map<juce::String, std::vector<EnumEntry>> enums;       // name -> {value,label} list
        std::vector<juce::String> groupOrderList;                   // top-level "groupOrder"
    };

    /** The groups actually present among `section`+`block`'s params (in model.all()), ordered
        per model.groupOrder() first, then any leftover group not mentioned there appended in
        first-seen (JSON param) order as a fallback safety net (so a group is never silently
        dropped just because the generator forgot to list it in groupOrder). Pure function of
        already-parsed ParamModel data — used by SoloSynthPanel to render group headers without
        hardcoding group names/order in the app layer (single source of truth stays the JSON). */
    std::vector<juce::String> orderedGroupsForBlock (const ParamModel& model,
                                                       const juce::String& section,
                                                       const juce::String& block);
}
