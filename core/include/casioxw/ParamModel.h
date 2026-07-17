#pragma once

#include <juce_core/juce_core.h>

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace casioxw
{
    /** One logical XW-P1 parameter, distilled from params/xwp1.json.

        Only the fields the SysEx codec needs are kept; UI metadata is ignored.
        Addresses/encoders come from the reverse-engineered Lua (the working
        source of truth) via the JSON, NOT from the official manual. */
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
    };

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

    private:
        void index();

        std::vector<ParamInfo> params;
        std::map<juce::String, int> byId;                          // id -> index into params
        std::map<juce::String, std::vector<AddressHit>> byAddress;  // 18-byte key -> hits
    };
}
