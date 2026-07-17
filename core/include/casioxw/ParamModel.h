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

    private:
        void index();

        std::vector<ParamInfo> params;
        std::map<juce::String, int> byId;                          // id -> index into params
        std::map<juce::String, std::vector<AddressHit>> byAddress;  // 18-byte key -> hits
        std::map<juce::String, std::vector<EnumEntry>> enums;       // name -> {value,label} list
    };
}
