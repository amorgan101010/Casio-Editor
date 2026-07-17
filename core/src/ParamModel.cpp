#include "casioxw/ParamModel.h"

#include <stdexcept>

namespace casioxw
{
    namespace
    {
        int asInt (const juce::var& v, int fallback = 0)
        {
            return v.isVoid() ? fallback : static_cast<int> (v);
        }

        bool asBool (const juce::var& v, bool fallback = false)
        {
            return v.isVoid() ? fallback : static_cast<bool> (v);
        }
    }

    ParamModel ParamModel::fromFile (const juce::File& jsonFile)
    {
        if (! jsonFile.existsAsFile())
            throw std::runtime_error ("ParamModel: file not found: "
                                      + jsonFile.getFullPathName().toStdString());

        return fromJsonString (jsonFile.loadFileAsString());
    }

    ParamModel ParamModel::fromJsonString (const juce::String& json)
    {
        juce::var root;
        const auto result = juce::JSON::parse (json, root);
        if (result.failed())
            throw std::runtime_error ("ParamModel: JSON parse error: "
                                      + result.getErrorMessage().toStdString());

        auto* sections = root.getProperty ("sections", juce::var()).getDynamicObject();
        if (sections == nullptr)
            throw std::runtime_error ("ParamModel: missing 'sections' object");

        ParamModel model;

        // Top-level "enums" table (soloSynthWaves, soloPcmWaves, filterType, filterGain,
        // lfoWave, ...) — UI metadata, ignored by SysExCodec.
        if (auto* enumsObj = root.getProperty ("enums", juce::var()).getDynamicObject())
        {
            for (auto& e : enumsObj->getProperties())
            {
                std::vector<EnumEntry> list;
                if (auto* arr = e.value.getArray())
                {
                    for (const auto& item : *arr)
                    {
                        EnumEntry entry;
                        entry.value = asInt (item.getProperty ("value", juce::var()));
                        entry.label = item.getProperty ("label", juce::var()).toString();
                        list.push_back (std::move (entry));
                    }
                }
                model.enums[e.name.toString()] = std::move (list);
            }
        }

        for (auto& sectionProp : sections->getProperties())
        {
            const juce::String sectionName = sectionProp.name.toString();
            const juce::var section = sectionProp.value;
            const juce::var paramsVar = section.getProperty ("params", juce::var());
            auto* paramsArr = paramsVar.getArray();
            if (paramsArr == nullptr)
                continue;   // stubbed section with no params yet

            for (const auto& pv : *paramsArr)
            {
                ParamInfo info;
                info.id      = pv.getProperty ("id", juce::var()).toString();
                info.section = sectionName;
                info.vt      = pv.getProperty ("vt", juce::var()).toString();

                const juce::var address = pv.getProperty ("address", juce::var());
                info.ct   = asInt (address.getProperty ("ct", juce::var()));
                info.addr = asInt (address.getProperty ("id", juce::var()));
                info.ai   = asInt (address.getProperty ("ai", juce::var()));
                info.an   = asInt (address.getProperty ("an", juce::var()));

                const juce::var instances = pv.getProperty ("instances", juce::var());
                info.instanceCount    = asInt (instances.getProperty ("count", juce::var()), 1);
                info.addressByteIndex = asInt (instances.getProperty ("addressByteIndex", juce::var()), 10);

                if (auto* labelsArr = instances.getProperty ("labels", juce::var()).getArray())
                    for (const auto& l : *labelsArr)
                        info.instanceLabels.add (l.toString());

                const juce::var perOsc = pv.getProperty ("perOsc", juce::var());
                const juce::var waveBase = perOsc.getProperty ("waveBaseOffset", juce::var());
                if (auto* wbObj = waveBase.getDynamicObject())
                    for (auto& e : wbObj->getProperties())
                        info.waveBaseOffset[e.name.toString().getIntValue()] = static_cast<int> (e.value);

                const juce::var enumPerOscVar = perOsc.getProperty ("enumPerOsc", juce::var());
                if (auto* epObj = enumPerOscVar.getDynamicObject())
                    for (auto& e : epObj->getProperties())
                        if (! e.value.isVoid())   // JSON null -> no enum for this instance (disabled)
                            info.enumPerOscByInstance[e.name.toString().getIntValue()] = e.value.toString();

                const juce::var maxPerOscVar = perOsc.getProperty ("maxPerOsc", juce::var());
                if (auto* mpObj = maxPerOscVar.getDynamicObject())
                    for (auto& e : mpObj->getProperties())
                        info.maxPerOsc[e.name.toString().getIntValue()] = asInt (e.value);

                // ---- UI metadata (additive) --------------------------------------------------
                info.name  = pv.getProperty ("name", juce::var()).toString();
                info.block = pv.getProperty ("block", juce::var()).toString();
                info.note  = pv.getProperty ("note", juce::var()).toString();
                info.unit  = pv.getProperty ("unit", juce::var()).toString();

                const juce::var rangeVar = pv.getProperty ("range", juce::var());
                info.range.min = asInt (rangeVar.getProperty ("min", juce::var()));
                info.range.max = asInt (rangeVar.getProperty ("max", juce::var()));

                const juce::var defaultVar = pv.getProperty ("default", juce::var());
                if (! defaultVar.isVoid())
                    info.defaultValue = static_cast<int> (defaultVar);

                const juce::var uiVar = pv.getProperty ("ui", juce::var());
                info.ui.control    = uiVar.getProperty ("control", juce::var()).toString();
                info.ui.enumName   = uiVar.getProperty ("enum", juce::var()).toString();
                info.ui.enumPerOsc = asBool (uiVar.getProperty ("enumPerOsc", juce::var()));

                model.params.push_back (std::move (info));
            }
        }

        model.index();
        return model;
    }

    void ParamModel::index()
    {
        byId.clear();
        byAddress.clear();

        for (int i = 0; i < (int) params.size(); ++i)
        {
            const auto& p = params[(size_t) i];
            byId[p.id] = i;

            for (int inst = 1; inst <= p.instanceCount; ++inst)
            {
                // Build the 18-byte address key (block byte = instance-1) as
                // 36 lowercase hex chars (no spaces) — used for reverse lookup.
                std::uint8_t a[18] = {};
                a[0] = (std::uint8_t) p.ct;
                a[(size_t) p.addressByteIndex] = (std::uint8_t) (inst - 1);
                a[12] = (std::uint8_t) p.addr;
                a[14] = (std::uint8_t) p.ai;
                a[16] = (std::uint8_t) p.an;

                juce::String key;
                for (auto b : a)
                    key << juce::String::toHexString (&b, 1, 0).paddedLeft ('0', 2);

                byAddress[key].push_back ({ i, inst });
            }
        }
    }

    const ParamInfo* ParamModel::find (const juce::String& id) const
    {
        const auto it = byId.find (id);
        return it == byId.end() ? nullptr : &params[(size_t) it->second];
    }

    const std::vector<ParamModel::AddressHit>* ParamModel::lookupAddress (const juce::String& key18) const
    {
        const auto it = byAddress.find (key18);
        return it == byAddress.end() ? nullptr : &it->second;
    }

    bool ParamModel::isAmbiguous (const juce::String& key18) const
    {
        const auto* hits = lookupAddress (key18);
        return hits != nullptr && hits->size() > 1;
    }

    const std::vector<EnumEntry>* ParamModel::enumValues (const juce::String& name) const
    {
        if (name.isEmpty())
            return nullptr;
        const auto it = enums.find (name);
        return it == enums.end() ? nullptr : &it->second;
    }

    juce::String resolveEnumName (const ParamInfo& info, int instance)
    {
        if (info.ui.enumPerOsc)
        {
            const auto it = info.enumPerOscByInstance.find (instance);
            return it == info.enumPerOscByInstance.end() ? juce::String() : it->second;
        }
        return info.ui.enumName;
    }

    ControlKind decideControlKind (const ParamInfo& info, int instance)
    {
        if (info.ui.control == "toggle")
            return ControlKind::Toggle;

        if (info.ui.control == "combo")
        {
            if (info.ui.enumPerOsc)
                return resolveEnumName (info, instance).isEmpty() ? ControlKind::Disabled
                                                                    : ControlKind::ComboEnumPerOsc;
            return info.ui.enumName.isNotEmpty() ? ControlKind::ComboEnum : ControlKind::ComboRange;
        }

        // "slider", or any unrecognised control string — default to a numeric slider rather
        // than silently rendering nothing.
        return ControlKind::Slider;
    }
}
