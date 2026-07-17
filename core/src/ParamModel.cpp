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

                const juce::var perOsc = pv.getProperty ("perOsc", juce::var());
                const juce::var waveBase = perOsc.getProperty ("waveBaseOffset", juce::var());
                if (auto* wbObj = waveBase.getDynamicObject())
                    for (auto& e : wbObj->getProperties())
                        info.waveBaseOffset[e.name.toString().getIntValue()] = static_cast<int> (e.value);

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
}
