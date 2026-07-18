#include "casioxw/SysExCodec.h"

#include <stdexcept>

namespace casioxw
{
    namespace
    {
        constexpr std::uint8_t kFrameHeader[6] = { 0xF0, 0x44, 0x16, 0x03, 0x7F, 0x01 };

        // V2SX: UI value -> wire bytes, already in LSB-first on-wire order.
        // Mirrors g_xwModCalc["V2SX"] (011_initTables.lua:36-49) + sendXWSX's
        // msb/lsb reversal (014_globalFunctions.lua:320). No clamping.
        std::vector<std::uint8_t> encodeValue (const juce::String& vt,
                                               int value,
                                               int waveBaseOffset)
        {
            const auto lo7  = [] (int x) { return (std::uint8_t) (x & 0x7f); };
            const auto hi7  = [] (int x) { return (std::uint8_t) ((x >> 7) & 0xff); };

            if (vt == "nf")   return { (std::uint8_t) (value & 0xff) };
            if (vt == "cf")   return { (std::uint8_t) ((value + 64) & 0xff) };

            if (vt == "cF")   { const int w = value + 128;      return { lo7 (w), hi7 (w) }; }
            if (vt == "tn")   { const int w = 2 * (value + 256); return { lo7 (w), hi7 (w) }; }

            if (vt == "wf")   { const int w = value + waveBaseOffset; return { lo7 (w), hi7 (w), 0x00 }; }

            if (vt == "pk")
            {
                std::uint8_t sgn = 0;
                int sx = 0x30 * value;
                if (value < 0) { sgn = 0x7f; sx = 0x4000 + sx; }
                return { lo7 (sx), hi7 (sx), sgn };
            }

            throw std::runtime_error ("SysExCodec: unsupported encode vt: " + vt.toStdString());
        }
    }

    std::vector<std::uint8_t> SysExCodec::soloSynthToneHeader()
    {
        return { 0xF0, 0x44, 0x16, 0x03, 0x7F, 0x01, 0x09 };
    }

    SysExCodec::SysExCodec (ParamModel model)
        : paramModel (std::move (model))
    {
    }

    std::vector<std::uint8_t> SysExCodec::encode (const juce::String& paramId,
                                                  int instance,
                                                  int value) const
    {
        const ParamInfo* p = paramModel.find (paramId);
        if (p == nullptr)
            throw std::runtime_error ("SysExCodec: unknown paramId: " + paramId.toStdString());
        if (instance < 1 || instance > p->instanceCount)
            throw std::runtime_error ("SysExCodec: instance out of range for " + paramId.toStdString());

        // 18-byte address (createSXtssArray layout). Block byte = instance-1.
        std::uint8_t addr[18] = {};
        addr[0] = (std::uint8_t) p->ct;
        addr[(size_t) p->addressByteIndex] = (std::uint8_t) (instance - 1);
        addr[12] = (std::uint8_t) p->addr;
        addr[14] = (std::uint8_t) p->ai;
        addr[16] = (std::uint8_t) p->an;

        int waveBase = 0;
        if (p->vt == "wf")
        {
            const auto it = p->waveBaseOffset.find (instance);   // osc == instance (1-based)
            if (it == p->waveBaseOffset.end())
                throw std::runtime_error ("SysExCodec: missing wave base offset for "
                                          + paramId.toStdString());
            waveBase = it->second;
        }

        const std::vector<std::uint8_t> valueBytes = encodeValue (p->vt, value, waveBase);

        std::vector<std::uint8_t> frame;
        frame.reserve (6 + 18 + valueBytes.size() + 1);
        frame.insert (frame.end(), std::begin (kFrameHeader), std::end (kFrameHeader));
        frame.insert (frame.end(), std::begin (addr), std::end (addr));
        frame.insert (frame.end(), valueBytes.begin(), valueBytes.end());
        frame.push_back (0xF7);
        return frame;
    }

    SysExCodec::Decoded SysExCodec::decode (const std::vector<std::uint8_t>& frame) const
    {
        Decoded d;

        // Minimum: 6 header + 18 address + 1 value + F7.
        if (frame.size() < 26 || frame.front() != 0xF0 || frame.back() != 0xF7)
            return d;
        for (int i = 0; i < 5; ++i)          // manufacturer/model/device (skip act at [5])
            if (frame[(size_t) i] != kFrameHeader[i])
                return d;

        // 18-byte address key at indices 6..23 -> 36-char lowercase hex.
        juce::String key;
        for (int i = 6; i < 24; ++i)
        {
            const std::uint8_t b = frame[(size_t) i];
            key << juce::String::toHexString (&b, 1, 0).paddedLeft ('0', 2);
        }

        const auto* hits = paramModel.lookupAddress (key);
        if (hits == nullptr || hits->empty())
            return d;

        const auto& first = paramModel.paramAt ((*hits)[0].paramIndex);
        d.ok       = true;
        d.instance = (*hits)[0].instance;
        d.vt       = first.vt;
        d.paramId  = first.id;
        for (const auto& h : *hits)
            d.candidates.push_back (paramModel.paramAt (h.paramIndex).id);
        d.ambiguous = hits->size() > 1;

        // Value bytes: indices 24 .. size-2 (LSB-first), consumed in wire order
        // exactly as SX2v receives them (rxTSS: v[b]=getByte(23+b)).
        std::vector<int> v;
        for (std::size_t i = 24; i + 1 < frame.size(); ++i)
            v.push_back (frame[i]);
        const int b0 = v.size() > 0 ? v[0] : 0;
        const int b1 = v.size() > 1 ? v[1] : 0;
        const int b2 = v.size() > 2 ? v[2] : 0;

        const juce::String& vt = first.vt;
        if (vt == "nf")       d.value = b0;
        else if (vt == "cf")  d.value = b0 - 64;
        else if (vt == "cF")  d.value = b0 + (b1 << 7) - 128;
        else if (vt == "tn")
        {
            const int t = b0 + (b1 << 7);
            if (t % 2 != 0)
                throw std::runtime_error ("SysExCodec: tn value not divisible by 2");
            d.value = t / 2 - 256;
        }
        else if (vt == "wf")
        {
            const int w = b0 + (b1 << 7);
            const auto it = first.waveBaseOffset.find (d.instance);
            d.value = w - (it != first.waveBaseOffset.end() ? it->second : 0);
        }
        else if (vt == "pk")
        {
            const int raw = b0 + (b1 << 7);
            const int mag = (b2 == 0) ? raw : (raw - 0x4000);
            if (mag % 0x30 != 0)
                throw std::runtime_error ("SysExCodec: pk value not divisible by 0x30");
            d.value = mag / 0x30;
        }
        else
            throw std::runtime_error ("SysExCodec: unsupported decode vt: " + vt.toStdString());

        return d;
    }
}
