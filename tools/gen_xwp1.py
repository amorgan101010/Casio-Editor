#!/usr/bin/env python3
# Generator for params/xwp1.json - parses the CTRLR panel Lua (source of truth)
import re, json, os

# Paths derived from this script's location (tools/ lives under the repo root),
# so the generator is portable regardless of where the repo is checked out.
_REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LUA = os.environ.get("XWP1_LUA_DIR", os.path.join(_REPO, "reference", "lua"))
OUT = os.environ.get("XWP1_OUT", os.path.join(_REPO, "params", "xwp1.json"))

# ---------------------------------------------------------------------------
# 1. Parse wave enums from 012_initWaves.lua
# ---------------------------------------------------------------------------
def extract_wavelist(text, name):
    start = text.index(name)
    q = text.index('"', start) + 1
    # end = the next unescaped '"' -- wave names contain no '"', so first '"' after q
    end = text.index('"', q)
    body = text[q:end]
    parts = [p for p in re.split(r'\\n|\n', body) if p.strip() != ""]
    entries = []
    for p in parts:
        m = re.match(r'\s*(\d+)\.(.*)$', p)
        assert m, f"bad wave entry: {p!r}"
        entries.append((int(m.group(1)), m.group(2).strip()))
    # verify contiguous from 0
    nums = [n for n, _ in entries]
    assert nums == list(range(0, len(nums))), f"{name} not contiguous: {nums[:5]}..max{max(nums)} len{len(nums)}"
    return [{"value": n, "label": lbl} for n, lbl in entries]

waves = open(os.path.join(LUA, "012_initWaves.lua"), encoding="utf-8", errors="replace").read()
syn_waves = extract_wavelist(waves, "g_tssSYNwave.P1")
pcm_waves = extract_wavelist(waves, "g_tssPCMwave.P1")
print(f"SYN waves: {len(syn_waves)} (0..{len(syn_waves)-1})")
print(f"PCM waves: {len(pcm_waves)} (0..{len(pcm_waves)-1})")

# ---------------------------------------------------------------------------
# 2. Parse the solo-synth parameter blocks from 011_initTables.lua
# ---------------------------------------------------------------------------
tbl = open(os.path.join(LUA, "011_initTables.lua"), encoding="utf-8").read()

# grab the `local tssModSX = {}` ... up to `g_tssModSXrx` region
region = tbl[tbl.index("local tssModSX"):tbl.index("g_tssModSXrx = {}")]

BLOCK_ORDER = ["tssOSC", "tssPWM", "tssETC", "tssFLT", "tssLFO"]

def parse_block(region, bname):
    # find  tssXXX["<bname>"] = { ... }  by brace matching
    key = f'tssModSX["{bname}"]'
    i = region.index(key)
    br = region.index("{", i)
    depth = 0
    for j in range(br, len(region)):
        if region[j] == "{": depth += 1
        elif region[j] == "}":
            depth -= 1
            if depth == 0:
                body = region[br+1:j]; break
    ct = int(re.search(r"ct=0x([0-9a-fA-F]+)", body).group(1), 16)
    bn = int(re.search(r"bn=(\d+)", body).group(1))
    params = []  # ordered, last-wins dedupe like Lua tables
    seen = {}
    for line in body.splitlines():
        m = re.match(r'\s*(tss[A-Za-z0-9]+)\s*=\s*\{id=0x([0-9a-fA-F]+)\s*,\s*ai=(\d+)\s*,\s*an=(\d+)\s*,\s*vt=\'([^\']+)\'\}', line)
        if not m: continue
        pid, idv, ai, an, vt = m.group(1), int(m.group(2),16), int(m.group(3)), int(m.group(4)), m.group(5)
        rec = {"pid": pid, "id": idv, "ai": ai, "an": an, "vt": vt}
        if pid in seen:
            params[seen[pid]] = rec  # last wins (Lua table semantics)
        else:
            seen[pid] = len(params); params.append(rec)
    return ct, bn, params

blocks = {b: parse_block(region, b) for b in BLOCK_ORDER}

# ---------------------------------------------------------------------------
# 3. Metadata: value-type range/bytes/signedness, names, ui, manual notes
# ---------------------------------------------------------------------------
# vt -> (bytes, signed, default-range, ui default). Ranges are the UI-value ranges.
VT_RANGE = {
    "nf": (0, 127),
    "cf": (-64, 63),
    "cF": (-128, 128),
    "pk": (-256, 255),
    "tn": (-256, 255),
    "wf": (0, None),   # per-osc max
}

# section-label per block instance
INSTANCE_LABELS = {
    "tssOSC": ["Synth1", "Synth2", "PCM1", "PCM2", "EXT", "Noise"],
    "tssPWM": ["Synth1", "Synth2"],
    "tssLFO": ["LFO1", "LFO2"],
    "tssETC": ["Common"],
    "tssFLT": ["TotalFilter"],
}
# sub-block grouping label for UI
BLOCK_GROUP = {"tssOSC": "OSC", "tssPWM": "PWM", "tssETC": "Etc", "tssFLT": "TotalFilter", "tssLFO": "LFO"}

# On/off (switch) params: range 0..1, default 0, toggle
ONOFF = {"tssOSCsw","tssOSCPortaSw","tssOSCLegatoSw","tssOSC2sync",
         "tssOSCXPxtrg","tssOSCXFxtrg","tssOSCXAxtrg","tssOSCXTFxtrg","tssFLTFErtrg"}

# per-param overrides: (range_override, default, ui_control, enum_ref, address ai, note)
# note = discrepancy vs midi-spec (prefer Lua vt, record the tighter range)
OVR = {
  "tssOSCAlfo2D": dict(ai=1, note="Lua typo fix: franky's table has ai=0 (colliding with tssOSCAlfo1D); "
                       "midi-spec p75 (OSC Block Amp) defines LFO Depth as 'Array 02', so LFO2 depth = ai 1 "
                       "(same pattern the Lua itself uses for tssOSCPlfo1D/2D). Verified on hardware 2026-07-17."),
  "tssFLTFlfo2D": dict(ai=1, note="Lua typo fix: franky's table has ai=0 (colliding with tssFLTFlfo1D); "
                       "midi-spec p79 (Total Filter) defines LFO Depth as 'Array 02', so LFO2 depth = ai 1. "
                       "Verified on hardware 2026-07-17."),
  "tssLFOsync":   dict(rng=(0,2), default=0, ui="combo", note="midi-spec p78-79: Clock Sync 0=NoSync/1=Sync LFO1(LFO2 only)/2=Sync Tempo; Lua vt='nf'."),
  "tssLFOwf":     dict(rng=(0,7), default=0, ui="combo", enum="lfoWave", note="midi-spec p78-79: LFO Wave 0-7; Lua vt='nf' (would imply 0-127)."),
  "tssLFOclk":    dict(rng=(0,17), ui="combo", enum="lfoClockTrigger",
                       note="Manual E-27 'Clk.Sync': 1/4,1/3,1/2,2/3,1,3/2,2,3,4,1/4U..4U (18 values, 0-17) -- "
                            "same list as the envelope Clock Trigger but WITHOUT the leading 'Off' entry (LFO's "
                            "separate Sync param already carries Off/Tempo/LFO1). Confirmed against manual text, "
                            "not guessed. midi-spec p78-79 corroborates the 0-17 wire range."),
  "tssFLTFtype":  dict(rng=(0,2), default=0, ui="combo", enum="filterType", note="midi-spec p79: Total Filter Type 0=LPF/1=BPF/2=HPF; Lua vt='nf'."),
  "tssOSCFcoff":  dict(rng=(0,15), note="midi-spec p74: OSC Filter Cutoff 0-15; Lua vt='nf' (NRPN uses nf-15)."),
  "tssOSCFgain":  dict(rng=(0,4), ui="combo", enum="filterGain", note="midi-spec p74: OSC Filter Gain 0=Flat/1=-3/2=-6/3=-12/4=-18dB; Lua vt='nf' (NRPN uses nf-4)."),
  "tssOSCPEclk":  dict(rng=(0,18), ui="combo", enum="clockTrigger", note="midi-spec p74: Clock Trigger 0-18; Lua vt='nf'. Manual E-24: Off,1/4,1/3,1/2,2/3,1,3/2,2,3,4,1/4U..4U (19 values)."),
  "tssOSCFEclk":  dict(rng=(0,18), ui="combo", enum="clockTrigger", note="midi-spec p74: Clock Trigger 0-18; Lua vt='nf'. Manual E-24: Off,1/4,1/3,1/2,2/3,1,3/2,2,3,4,1/4U..4U (19 values)."),
  "tssOSCAEclk":  dict(rng=(0,18), ui="combo", enum="clockTrigger", note="midi-spec p75: Clock Trigger 0-18; Lua vt='nf'. Manual E-24: Off,1/4,1/3,1/2,2/3,1,3/2,2,3,4,1/4U..4U (19 values)."),
  "tssFLTFEclk":  dict(rng=(0,18), ui="combo", enum="clockTrigger", note="midi-spec p79: Clock Trigger 0-18; Lua vt='nf'. Manual E-24: Off,1/4,1/3,1/2,2/3,1,3/2,2,3,4,1/4U..4U (19 values)."),
  "tssOSCXPshmode": dict(rng=(0,3), note="midi-spec p76: Pitch Shifter Mode 0-3; Lua vt='nf'."),
  "tssOSCXPshmix":  dict(rng=(0,15), note="midi-spec p76: Pitch Shifter Mix 0-15; Lua vt='nf'."),
  # Key Follow Base params: MIDI note numbers 0-127, manual says "C-1 to G9" -- unit="note" is a
  # DISPLAY FORMATTER SELECTOR for the app (Slider::textFromValueFunction via casioxw::midiNoteName),
  # not a unit-of-measure suffix; ParamControl must not append it to the label (see displayName()).
  "tssOSCPkeyfB": dict(unit="note"),
  "tssOSCFkeyfB": dict(unit="note"),
  "tssOSCAkeyfB": dict(unit="note"),
  "tssFLTFkeyfB": dict(unit="note"),
}

# ---------------------------------------------------------------------------
# Visual grouping (Chunk 7c item 4): purely a UI-metadata field, derived from id
# structure and cross-checked against the manual's own grouping (E-24..E-27).
# Never read by SysExCodec. groupOrder (below) is the canonical display order;
# app/SoloSynthPanel renders whichever of these groups are present in a block, in
# this order, then falls back to first-seen order for anything not listed here.
# ---------------------------------------------------------------------------
GROUP_ORDER = ["General", "Pitch", "Pitch Envelope", "Filter", "Filter Envelope",
               "Amp", "Amp Envelope", "Portamento / Legato", "PWM",
               "External Input", "External Trigger", "Pitch Shifter", "LFO"]

def group_for(pid):
    if "ENV" in pid:
        if pid.startswith("tssOSCP"): return "Pitch Envelope"
        if pid.startswith("tssOSCF"): return "Filter Envelope"
        if pid.startswith("tssOSCA"): return "Amp Envelope"
        if pid.startswith("tssFLTF"): return "Filter Envelope"
    if re.match(r'tssOSCP(Eclk|Edep)$', pid): return "Pitch Envelope"
    if re.match(r'tssOSCF(Eclk|Edep)$', pid): return "Filter Envelope"
    if re.match(r'tssOSCA(Eclk)$', pid): return "Amp Envelope"
    if re.match(r'tssFLTF(Eclk|Edep|Ertrg)$', pid): return "Filter Envelope"
    if pid in ("tssOSCPortaSw", "tssOSCPortaTm", "tssOSCLegatoSw"): return "Portamento / Legato"
    if pid.startswith("tssOSCPWM"): return "PWM"
    if pid in ("tssOSCsw", "tssOSCwf", "tssOSC2sync"): return "General"
    if pid in ("tssOSCXokey", "tssOSCXinlvl"): return "External Input"
    if pid in ("tssOSCXPxtrg", "tssOSCXFxtrg", "tssOSCXAxtrg", "tssOSCXTFxtrg", "tssOSCXngth", "tssOSCXngrel"): return "External Trigger"
    if pid in ("tssOSCXPshmode", "tssOSCXPshmix"): return "Pitch Shifter"
    if pid.startswith("tssOSCP"): return "Pitch"
    if pid.startswith("tssOSCF"): return "Filter"
    if pid.startswith("tssOSCA"): return "Amp"
    if pid.startswith("tssFLTF"): return "Filter"
    if pid.startswith("tssLFO"): return "LFO"
    raise AssertionError(f"group_for: no group rule matched {pid!r}")

# Human-readable name generation ------------------------------------------------
SEC = {"P": "Pitch", "F": "Filter", "A": "Amp"}
ENV = {"iL":"Env Init Level","aT":"Env Attack Time","aL":"Env Attack Level","dT":"Env Decay Time",
       "sL":"Env Sustain Level","r1T":"Env Rel1 Time","r1L":"Env Rel1 Level","r2T":"Env Rel2 Time",
       "r2L":"Env Rel2 Level"}
SIMPLE = {
  "tssOSCsw":"OSC On/Off","tssOSCwf":"OSC Waveform","tssOSCPortaSw":"Portamento On/Off",
  "tssOSCPortaTm":"Portamento Time","tssOSCLegatoSw":"Legato On/Off",
  "tssOSCPoset":"Pitch Offset","tssOSCPdtne":"Detune","tssOSCPkeyf":"Pitch Key Follow",
  "tssOSCPkeyfB":"Pitch Key Follow Base","tssOSCPEclk":"Pitch Env Clock Trigger",
  "tssOSCPEdep":"Pitch Env Depth","tssOSCPlfo1D":"Pitch LFO1 Depth","tssOSCPlfo2D":"Pitch LFO2 Depth",
  "tssOSCFcoff":"Filter Cutoff","tssOSCFgain":"Filter Gain","tssOSCFtch":"Filter Touch Sense",
  "tssOSCFkeyf":"Filter Key Follow","tssOSCFkeyfB":"Filter Key Follow Base",
  "tssOSCFlfo1D":"Filter LFO1 Depth","tssOSCFlfo2D":"Filter LFO2 Depth",
  "tssOSCFEclk":"Filter Env Clock Trigger","tssOSCFEdep":"Filter Env Depth",
  "tssOSCAlvl":"Amp Level","tssOSCAtch":"Amp Touch Sense","tssOSCAkeyf":"Amp Key Follow",
  "tssOSCAkeyfB":"Amp Key Follow Base","tssOSCAlfo1D":"Amp LFO1 Depth","tssOSCAlfo2D":"Amp LFO2 Depth",
  "tssOSCAEclk":"Amp Env Clock Trigger",
  "tssOSCPWMpw":"PWM Pulse Width","tssOSCPWMlfo1D":"PWM LFO1 Depth","tssOSCPWMlfo2D":"PWM LFO2 Depth",
  "tssOSC2sync":"Osc Sync (OSC2->OSC1)","tssOSCXokey":"Ext Original Key",
  "tssOSCXPxtrg":"Ext Trigger Pitch Env","tssOSCXFxtrg":"Ext Trigger Filter Env",
  "tssOSCXAxtrg":"Ext Trigger Amp Env","tssOSCXTFxtrg":"Ext Trigger Total-Filter Env",
  "tssOSCXinlvl":"Mic/Inst Input Level","tssOSCXngth":"Ext Trigger Threshold",
  "tssOSCXngrel":"Ext Trigger Release","tssOSCXPshmode":"Pitch Shifter Mode","tssOSCXPshmix":"Pitch Shifter Mix",
  "tssFLTFtype":"Filter Type","tssFLTFcoff":"Filter Cutoff","tssFLTFreso":"Filter Resonance",
  "tssFLTFtch":"Filter Touch Sense","tssFLTFkeyf":"Filter Key Follow","tssFLTFkeyfB":"Filter Key Follow Base",
  "tssFLTFlfo1D":"Filter LFO1 Depth","tssFLTFlfo2D":"Filter LFO2 Depth","tssFLTFEclk":"Filter Env Clock Trigger",
  "tssFLTFEdep":"Filter Env Depth","tssFLTFErtrg":"Filter Env Retrigger",
  "tssLFOwf":"LFO Waveform","tssLFOsync":"LFO Clock Sync","tssLFOrate":"LFO Rate","tssLFOdep":"LFO Depth",
  "tssLFOdelay":"LFO Delay","tssLFOrise":"LFO Rise","tssLFOclk":"LFO Clock Trigger","tssLFOmdep":"LFO Mod Depth",
}
def name_for(pid):
    if pid in SIMPLE: return SIMPLE[pid]
    # tssOSC<sec>ENV<stage>
    m = re.match(r'tssOSC([PFA])ENV(\w+)$', pid)
    if m: return f"{SEC[m.group(1)]} {ENV[m.group(2)]}"
    m = re.match(r'tssFLTFENV(\w+)$', pid)
    if m: return f"Filter {ENV[m.group(1)]}"
    return pid  # fallback

# ---------------------------------------------------------------------------
# 4. Build the param entries
# ---------------------------------------------------------------------------
def build_params():
    out = []
    for b in BLOCK_ORDER:
        ct, bn, params = blocks[b]
        count = bn if bn > 0 else 1
        labels = INSTANCE_LABELS[b]
        for rec in params:
            pid, vt = rec["pid"], rec["vt"]
            ov = OVR.get(pid, {})
            # range
            if pid in ONOFF:
                rng = {"min": 0, "max": 1}; default = 0; ui = "toggle"
            elif "rng" in ov:
                rng = {"min": ov["rng"][0], "max": ov["rng"][1]}
                default = ov.get("default", 0 if vt in ("cf","cF","pk","tn","wf") else None)
                ui = ov.get("ui", "slider")
            else:
                lo, hi = VT_RANGE[vt]
                if vt == "wf":
                    hi = len(pcm_waves) - 1   # overall bound; real per-osc max in perOsc.maxPerOsc
                rng = {"min": lo, "max": hi}
                default = 0 if vt in ("cf","cF","pk","tn") else (0 if vt == "wf" else None)
                ui = "combo" if vt == "wf" else "slider"
            entry = {
                "id": pid,
                "name": name_for(pid),
                "block": BLOCK_GROUP[b],
                "group": group_for(pid),
                "address": {"ct": ct, "id": rec["id"], "ai": ov.get("ai", rec["ai"]), "an": rec["an"]},
                "vt": vt,
                "range": rng,
                "default": default,
                "unit": ov.get("unit", ""),
                "ui": {"control": ui},
            }
            # instances descriptor
            entry["instances"] = {
                "count": count,
                "blkByteOffset": 6,          # position within the 8-byte blk selector
                "addressByteIndex": 10,      # absolute index within the 18-byte address
                "idSuffix": True,            # modulator id gets '-1'..'-count'
                "labels": labels,
            }
            # wf special handling
            if vt == "wf":
                entry["perOsc"] = {
                    "waveBaseOffset": {"1":1,"2":1,"3":326,"4":326,"5":0,"6":312},
                    "enumPerOsc": {"1":"soloSynthWaves","2":"soloSynthWaves",
                                    "3":"soloPcmWaves","4":"soloPcmWaves","5":None,"6":None},
                    "maxPerOsc": {"1":len(syn_waves)-1,"2":len(syn_waves)-1,
                                   "3":len(pcm_waves)-1,"4":len(pcm_waves)-1,"5":0,"6":13},
                }
                entry["ui"] = {"control": "combo", "enumPerOsc": True}
                entry["note"] = ("Wire wave# = UIvalue + waveBaseOffset[osc]; encoder returns 3 bytes "
                                 "(0,msb,lsb) sent LSB-first. P1 offsets differ from G1 (011_initTables.lua:26-27). "
                                 "OSC1/2 use soloSynthWaves, OSC3/4 use soloPcmWaves; OSC5(EXT) has no wave; "
                                 "OSC6(Noise) selects noise waves (wire 312-325).")
            # enum ref for combos
            if ov.get("enum"):
                entry["ui"]["enum"] = ov["enum"]
            # notes (discrepancies)
            if ov.get("note") and "note" not in entry:
                entry["note"] = ov["note"]
            elif ov.get("note"):
                entry["note"] += " | " + ov["note"]
            out.append(entry)
    return out

solo_params = build_params()

# Address collision detection (same 18-byte address for >1 param within same instance)
def addr_key(e):
    return (e["address"]["ct"], e["address"]["id"], e["address"]["ai"], e["address"]["an"])
from collections import defaultdict
byaddr = defaultdict(list)
for e in solo_params:
    byaddr[addr_key(e)].append(e["id"])
collisions = {k: v for k, v in byaddr.items() if len(v) > 1}
for k, ids in collisions.items():
    for e in solo_params:
        if e["id"] in ids:
            n = (f"Address collision: shares 18-byte address (id=0x{k[1]:02x},ai={k[2]},an={k[3]}) with {[x for x in ids if x!=e['id']]}. "
                 "Likely Lua source typo (compare NRPN table where these have distinct ids). Prefer Lua per policy; codec RX cannot disambiguate.")
            e["note"] = (e.get("note","") + " | " + n).lstrip(" |")

print(f"Solo-synth logical params: {len(solo_params)}")
print(f"Total expanded addresses: {sum(e['instances']['count'] for e in solo_params)}")
print(f"Address collisions: {collisions}")

# ---------------------------------------------------------------------------
# 5. Enums
# ---------------------------------------------------------------------------
enums = {
    "soloSynthWaves": syn_waves,
    "soloPcmWaves": pcm_waves,
    "filterType": [{"value":0,"label":"LPF"},{"value":1,"label":"BPF"},{"value":2,"label":"HPF"}],
    "filterGain": [{"value":0,"label":"Flat"},{"value":1,"label":"-3dB"},{"value":2,"label":"-6dB"},
                    {"value":3,"label":"-12dB"},{"value":4,"label":"-18dB"}],
    "lfoWave": [{"value":0,"label":"Sine"},{"value":1,"label":"Triangle"},{"value":2,"label":"Saw Up"},
                 {"value":3,"label":"Saw Down"},{"value":4,"label":"Pulse 1:3"},{"value":5,"label":"Pulse 2:2"},
                 {"value":6,"label":"Pulse 3:1"},{"value":7,"label":"Random"}],
    # Envelope Clock Trigger (manual E-24/E-25): 19 values, wire 0-18. "U" suffix = reset synced
    # to the back beat (upbeat). Backs tssOSCPEclk/tssOSCFEclk/tssOSCAEclk/tssFLTFEclk.
    "clockTrigger": [{"value": i, "label": lbl} for i, lbl in enumerate(
        ["Off", "1/4", "1/3", "1/2", "2/3", "1", "3/2", "2", "3", "4",
         "1/4U", "1/3U", "1/2U", "2/3U", "1U", "3/2U", "2U", "3U", "4U"])],
    # LFO Clock Trigger (manual E-27 "Clk.Sync"): the SAME 18-value list minus the leading "Off"
    # (LFO's separate Sync param already has its own Off/Tempo/LFO1 setting) -- wire 0-17,
    # confirmed against the manual text, not inferred. Backs tssLFOclk only.
    "lfoClockTrigger": [{"value": i, "label": lbl} for i, lbl in enumerate(
        ["1/4", "1/3", "1/2", "2/3", "1", "3/2", "2", "3", "4",
         "1/4U", "1/3U", "1/2U", "2/3U", "1U", "3/2U", "2U", "3U", "4U"])],
}

# ---------------------------------------------------------------------------
# 6. Value types dictionary
# ---------------------------------------------------------------------------
value_types = {
  "nf": {"bytes":1,"signed":False,"wireOrder":"single","desc":"Normal fader 0-127 (wire = value).","luaRef":"011_initTables.lua:37"},
  "cf": {"bytes":1,"signed":True,"wireOrder":"single","desc":"Centered fader -64..+63 (wire = value+64).","luaRef":"011_initTables.lua:38"},
  "cF": {"bytes":2,"signed":True,"wireOrder":"lsb-first","desc":"Double-byte centered fader -128..+128 (wire = value+128, split msb/lsb, sent LSB first).","luaRef":"011_initTables.lua:40"},
  "pk": {"bytes":3,"signed":True,"wireOrder":"lsb-first-plus-sign","desc":"Pitch key -256..+255. wire = 0x30*value; if value<0 sign byte=0x7F and magnitude=0x4000+0x30*value; returns (sign,msb,lsb), sent LSB first (lsb,msb,sign).","luaRef":"011_initTables.lua:47"},
  "tn": {"bytes":2,"signed":True,"wireOrder":"lsb-first","desc":"Tune/detune -256..+255 with x2 scaling: wire = 2*(value+256) (center 0 -> 0x200), split msb/lsb, sent LSB first.","luaRef":"011_initTables.lua:46"},
  "wf": {"bytes":3,"signed":False,"wireOrder":"lsb-first","desc":"Waveform number, 3 bytes. wire = UIvalue + g_tsswf[osc] (per-oscillator base offset, P1 vs G1 differ); returns (0,msb,lsb), sent LSB first (lsb,msb,00). osc index parsed from modulator id suffix.","luaRef":"011_initTables.lua:45"},
}

# ---------------------------------------------------------------------------
# 7. Assemble document
# ---------------------------------------------------------------------------
doc = {
  "$schema": "./xwp1.schema.json",
  "device": "XW-P1",
  "generatedFrom": "CTRLR panel CasioXW-EDITOR-v0101.bpanelz (author: franky), extracted Lua in reference/lua/",
  "sysex": {
    "manufacturerId": "0x44",
    "modelId": ["0x16","0x03"],
    "deviceId": "0x7F",
    "frame": "F0 44 16 03 7F <act> <18-byte address> <value LSB-first> F7",
    "acts": {"request": 0, "set": 1, "reject": 11},
    "checksum": "none (per-parameter); CRC-32 only on bulk memory images, out of scope"
  },
  "addressLayout": {
    "totalBytes": 18,
    "note": "Address begins at the category byte; sendXWSX prepends 'F0 44 16 03 7F <act>' and appends value + F7. On the full wire message these 18 bytes sit at indices 6..23; value bytes begin at index 24.",
    "fields": [
      {"offset":0,"len":1,"name":"ct","desc":"category (0x09 solo synth)"},
      {"offset":1,"len":3,"name":"fixed","value":"00 00 00"},
      {"offset":4,"len":8,"name":"blk","desc":"block selector; per-instance byte at blk offset 6 (address index 10) holds oscillator/LFO number 0..count-1"},
      {"offset":12,"len":1,"name":"id","desc":"parameter id within block"},
      {"offset":13,"len":1,"name":"fixed","value":"00"},
      {"offset":14,"len":1,"name":"ai","desc":"array index (sub-parameter/element)"},
      {"offset":15,"len":1,"name":"fixed","value":"00"},
      {"offset":16,"len":1,"name":"an","desc":"array number"},
      {"offset":17,"len":1,"name":"fixed","value":"00"}
    ],
    "luaRef": "011_initTables.lua:244-265 (createSXtssArray); PROTOCOL.md section 3"
  },
  "valueTypes": value_types,
  "enums": enums,
  "groupOrder": GROUP_ORDER,
  "sections": {
    "soloSynth": {
      "category": "0x09",
      "status": "complete",
      "luaRef": "011_initTables.lua:82-207 (tssOSC/tssPWM/tssETC/tssFLT/tssLFO)",
      "note": "Nine hardware blocks (midi-spec p73): 6 OSC (Synth1/Synth2/PCM1/PCM2/EXT/Noise), LFO1, LFO2, Total Filter. In the Lua these are 5 param groups; OSC=6 instances, PWM=2, LFO=2, Etc/TotalFilter=1. Oscillator/Filter/Amp sub-blocks are ×5 in the manual (Noise excluded) but the Lua generates all 6 uniformly; codec should generate all 6 and the UI may disable inapplicable instances.",
      "params": solo_params
    },
    "hexLayer":    {"category":"0x08","status":"stub","params":[]},
    "pcmMelody":   {"category":"0x05","status":"stub","params":[]},
    "mixer":       {"status":"stub","params":[]},
    "performance": {"category":"0x02","status":"stub","params":[]},
    "dsp":         {"category":"0x13","status":"stub","params":[]}
  }
}

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", encoding="utf-8") as f:
    json.dump(doc, f, indent=2, ensure_ascii=False)
print("wrote", OUT, os.path.getsize(OUT), "bytes")
print("valueTypes:", len(value_types), "enums:", len(enums))
