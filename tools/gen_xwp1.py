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
#
# Chunk 7e item 1: the "X Envelope" sub-groups and "Portamento / Legato" were
# merged into their parent group (Pitch Envelope -> Pitch, Filter Envelope ->
# Filter [both OSC's and TotalFilter's], Amp Envelope -> Amp, Portamento / Legato
# -> General) per owner feedback that 8 OSC groups was too many. Distinguishing
# an envelope-STAGE param (the 9 graph points) from a plain modulation param
# within the now-merged group is a per-param check now
# (casioxw::envelopeStageIds(id).isValid()), not a per-group one — see
# core/include/casioxw/ParamModel.h (isEnvelopeGroup() was deleted; no group is
# ever named "X Envelope" any more).
# ---------------------------------------------------------------------------
GROUP_ORDER = ["Drawbars", "Percussion", "General", "Range", "Pitch", "Filter", "Amp", "Effects", "PWM",
               "External Input", "External Trigger", "Pitch Shifter", "LFO",
               "Envelope", "Vibrato"]

def group_for(pid):
    if "ENV" in pid:
        if pid.startswith("tssOSCP"): return "Pitch"
        if pid.startswith("tssOSCF"): return "Filter"
        if pid.startswith("tssOSCA"): return "Amp"
        if pid.startswith("tssFLTF"): return "Filter"
    if re.match(r'tssOSCP(Eclk|Edep)$', pid): return "Pitch"
    if re.match(r'tssOSCF(Eclk|Edep)$', pid): return "Filter"
    if re.match(r'tssOSCA(Eclk)$', pid): return "Amp"
    if re.match(r'tssFLTF(Eclk|Edep|Ertrg)$', pid): return "Filter"
    if pid in ("tssOSCPortaSw", "tssOSCPortaTm", "tssOSCLegatoSw"): return "General"
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

# ---------------------------------------------------------------------------
# 4b. Melody (PCM engine) params -- HAND-AUTHORED, not mined from Lua.
#
# franky's CTRLR panel has no handler for this domain (no "Melody"/"PCM" Lua
# file under reference/lua/ -- only ToneSoloSynth/ToneHexLayer/XWMixer/XWOrgan/
# DSPHandler exist), so there is nothing to parse here the way solo_params is
# built. Every field below is transcribed directly from XWP1_midi_EN.pdf
# section 23 "Melody Parameter" (printed page 70), category 05H -- the
# manual's own vocabulary for what the project/UI calls the PCM engine (see
# midi-spec.md:749 "PCM Melody"). UNVERIFIED against real hardware (unlike
# soloSynth, which is both Lua-cross-checked and hardware-verified) -- treat
# addr/vt as high-confidence (matches the general SX frame field layout in
# midi-spec.md section 2) but budget a hardware read/write check before
# trusting this section the way soloSynth is trusted.
#
# All 10 params share one flat block (manual's "Block 00000000" -- no nested
# blk dimension, unlike soloSynth's per-oscillator instances), ai=0/an=0 for
# every entry (manual's Array column is 01 for all of them; an is franky's
# name for the general frame's "len" field, and every existing scalar solo-
# synth param -- 1, 2, and 3-byte vt alike -- carries an=0, so a single-
# element scalar param taking an=0 here is the established pattern, not a
# guess).
MELODY_PARAMS = [
    # id,              manual name,     hex id, vt,  range,        default, group,    ui,      enum
    ("pcmAttackTime",  "Attack Time",   0x17, "cf", (-64, 63), 0,   "Envelope", "slider", None),
    ("pcmReleaseTime", "Release Time",  0x18, "cf", (-64, 63), 0,   "Envelope", "slider", None),
    ("pcmCutoffFreq",  "Cutoff Freq",   0x19, "cf", (-64, 63), 0,   "Envelope", "slider", None),
    ("pcmVibratoType", "Vibrato Type",  0x1A, "nf", (0, 3),    0,   "Vibrato",  "combo",  "melodyVibratoType"),
    ("pcmVibratoDepth","Vibrato Depth", 0x1B, "cf", (-64, 63), 0,   "Vibrato",  "slider", None),
    ("pcmVibratoSpeed","Vibrato Speed", 0x1C, "cf", (-64, 63), 0,   "Vibrato",  "slider", None),
    ("pcmVibratoDelay","Vibrato Delay", 0x1D, "cf", (-64, 63), 0,   "Vibrato",  "slider", None),
    ("pcmOctaveShift", "Octave Shift",  0x1E, "cf", (-2, 2),   0,   "General",  "slider", None),
    # HARDWARE-VERIFIED override (see VOLUME_NOTE): the real PCM tone Volume is the Tone-category
    # "Level" (ct 0x03 / id 0x08), NOT the Melody-category (0x05) "Volume" at 0x1F that §23 lists.
    ("pcmVolume",      "Volume",        0x08, "nf", (0, 127),  127, "General",  "slider", None),
    ("pcmTouchSense",  "Touch Sense",   0x20, "cf", (-64, 63), 0,   "General",  "slider", None),
]

TOUCH_SENSE_NOTE = (
    "midi-spec.md/PDF p70 sec 23: the manual's own hex Min-Def-Max column for Touch Sense "
    "(00-7F-7F) is IDENTICAL to Volume's row directly above it, but Touch Sense's described "
    "effective range is signed (-64..+63, matching the cf-encoded params above it) while "
    "Volume's is unsigned 0-127 -- the two rows cannot both be literally correct under one "
    "encoding. Encoded here as cf (consistent with every other -64..+63 param in this block); "
    "default taken from the effective-range description (0, centered) rather than the hex "
    "column's 0x7F (which would decode to +63 under cf and makes no sense as a centered "
    "default). Likely a copy-paste artifact in the manual's own table. Needs hardware "
    "read-back to confirm."
)

# pcmVolume lives in the Tone category (0x03), not Melody (0x05) -- hardware-verified per
# VOLUME_NOTE. Every other Melody param stays in 0x05.
MELODY_CT_OVERRIDE = {"pcmVolume": 0x03}

VOLUME_NOTE = (
    "HARDWARE-VERIFIED 2026-07-18 (owner + midi-probe scan on a real XW-P1): the PCM tone "
    "Volume shown/edited in the synth's Tone->PCM view is the Tone-category 'Level' parameter "
    "(ct 0x03, id 0x08, 00-7F-7F, 0-127), NOT the Melody-category (0x05) 'Volume' at id 0x1F "
    "that XWP1_midi_EN.pdf sec 23 lists. Reads/writes to cat05/0x1F succeed (it is a live "
    "register) but do not affect the audible/displayed tone volume; a synth-side change to the "
    "tone Volume showed up at cat03/0x08 instead. Address corrected to ct=0x03/id=0x08; encoding "
    "(nf, 0-127, default 127) was already correct. Was the original 'this section is "
    "hardware-UNVERIFIED' caveat coming due (see the pcmMelody section note)."
)

def build_melody_params():
    out = []
    for pid, name, idhex, vt, rng, default, group, ui, enum in MELODY_PARAMS:
        entry = {
            "id": pid,
            "name": name,
            "block": "Melody",
            "group": group,
            "address": {"ct": MELODY_CT_OVERRIDE.get(pid, 0x05), "id": idhex, "ai": 0, "an": 0},
            "vt": vt,
            "range": {"min": rng[0], "max": rng[1]},
            "default": default,
            "unit": "",
            "ui": {"control": ui},
            "instances": {
                "count": 1,
                "blkByteOffset": 0,
                "addressByteIndex": 10,
                "idSuffix": False,
                "labels": ["Melody"],
            },
        }
        if enum:
            entry["ui"]["enum"] = enum
        if pid == "pcmTouchSense":
            entry["note"] = TOUCH_SENSE_NOTE
        if pid == "pcmVolume":
            entry["note"] = VOLUME_NOTE
        out.append(entry)
    return out

melody_params = build_melody_params()
print(f"PCM/Melody params: {len(melody_params)} (hand-authored, unverified against hardware)")

# ---------------------------------------------------------------------------
# 4c. Drawbar Organ params -- HAND-AUTHORED, not mined from Lua.
#
# franky's CTRLR panel DOES have an organ controller (022_XWOrgan.lua), but unlike
# 019_ToneSoloSynth.lua it never calls sendXWSX -- every organ control there goes out live as
# NRPN/CC (g_orgModMidi in 011_initTables.lua: MSB=0x40 NRPN for drawbars/percussion/click/type,
# plain CC for vibrato/rotary/general). So there is no SysEx tone-edit-buffer source to mine for
# this domain, the same situation pcmMelody was in. Every field below is transcribed directly
# from XWP1_midi_EN.pdf section 25 "Drawbar" (printed p71-72), category 07H, XW-P1-only --
# reference/midi-spec.md section 5.6.
#
# HARDWARE-VERIFIED 2026-07-18 (owner + midi-probe on a real XW-P1, see .wolf/cerebrum.md
# addendum 31 for the full story): category/address (ct=0x07, ai=0/an=0, block byte @ address
# offset 10) and the plain 'nf' (direct, non-inverted) wire encoding are BOTH CONFIRMED CORRECT --
# GET/SET round-trips cleanly on every one of the 9 block-byte positions. What was WRONG was the
# assumed drawbar ORDER: this is NOT the harmonic order (16',5-1/3',8',4',2-2/3',2',1-3/5',
# 1-1/3',1') the Lua's NRPN table uses -- a "ladder test" (owner set each physical drawbar to a
# distinct recognizable value, then every one of the 9 SysEx block bytes was read back and
# matched against the known ladder) proved the SysEx "Select Bar" index instead groups drawbars
# by TYPE: the 5 octave bars first (16',8',4',2',1' at block bytes 0-4), then the 4 non-octave
# "mutation"/quint-tierce bars (5 1/3',2 2/3',1 3/5',1 1/3' at block bytes 5-8). See
# ORGAN_DRAWBAR_LABELS below (now in this confirmed order) and app/OrganPanel.cpp's
# kSysExInstanceToNrpnLsb (translates a SysEx instance to the DIFFERENT NRPN LSB order needed for
# writes, since the two transports number the same 9 drawbars differently).
#
# Also hardware-confirmed: a SysEx SET to organPosition lands and persists in the saved tone (it
# survives save+reload) but does NOT reach the running voice in real time -- the drawbar additive
# table apparently only gets recomputed on tone load, not on a bare parameter write. Every OTHER
# organPosition-adjacent control (Percussion/Click/Rotary Type/Vibrato) DOES apply live through
# this same SysEx path; only Position itself needs the separate NRPN live-fader path
# (app/OrganPanel.cpp's sendDrawbarNrpn) to be audible while editing.
# Order = the SysEx "Select Bar" index (hardware-confirmed 2026-07-18 via ladder test): octave
# bars first (16',8',4',2',1'), then non-octave "mutation" bars (5 1/3',2 2/3',1 3/5',1 1/3') --
# NOT the harmonic order the NRPN performance path uses. See the ORGAN_PARAMS comment above.
ORGAN_DRAWBAR_LABELS = ["16'", "8'", "4'", "2'", "1'", "5 1/3'", "2 2/3'", "1 3/5'", "1 1/3'"]

ORGAN_PARAMS = [
    # id,                  manual name,             hex id, vt,   range,     default, group,        ui,       enum
    ("organPosition",      "Drawbar Position",      0x00, "nf", (0, 8),   0,   "Drawbars",   "slider", None),
    ("organPercussion",    "Percussion",            0x01, "nf", (0, 3),   0,   "Percussion", "combo",  "organPercussionMode"),
    ("organPercDecayTime", "Percussion Decay Time", 0x02, "nf", (0, 127), 0,   "Percussion", "slider", None),
    ("organKeyonClick",    "Key-On Click",          0x03, "nf", (0, 1),   0,   "Percussion", "toggle", None),
    ("organKeyoffClick",   "Key-Off Click",         0x04, "nf", (0, 1),   0,   "Percussion", "toggle", None),
    # OWNER-VERIFIED ON HARDWARE 2026-07-18: this is a rotary-speaker/vibrato character switch
    # (Sine vs Vintage), not a general organ-type selector, and belongs with the Vibrato group --
    # the manual's own "Type"/section-25 placement was misleading here (see the note below).
    ("organRotaryType",    "Rotary Type",           0x05, "nf", (0, 1),   0,   "Vibrato",    "combo",  "organRotaryType"),
    ("organVibratoRate",   "Vibrato Rate",          0x06, "nf", (0, 127), 0,   "Vibrato",    "slider", None),
    ("organVibratoDepth",  "Vibrato Depth",         0x07, "nf", (0, 127), 0,   "Vibrato",    "slider", None),
]

ORGAN_ROTARY_TYPE_NOTE = (
    "OWNER-VERIFIED ON HARDWARE 2026-07-18: the manual's section 25 calls this 'Type' with values "
    "'Normal'/'Vintage' and groups it as a general organ parameter, but on the real XW-P1 it is a "
    "rotary-speaker/vibrato character switch (values Sine/Vintage), grouped with Vibrato -- "
    "renamed organType -> organRotaryType and moved out of General accordingly. Address (ct=0x07, "
    "id=0x05) and encoding (nf, 0-1) were already correct; only the manual's own name/grouping/enum "
    "labels were wrong."
)

ORGAN_POSITION_NOTE = (
    "HARDWARE-VERIFIED 2026-07-18 (owner + midi-probe on a real XW-P1, see .wolf/cerebrum.md "
    "addendum 31). Address (ct=0x07, id=0x00, block byte @ offset 10) and encoding (plain 'nf', "
    "0-8 direct) are both confirmed correct via GET/SET round-trip on every one of the 9 block "
    "bytes. The drawbar ORDER is confirmed via a ladder test (each physical bar set to a distinct "
    "value, then every block byte read back and matched): it groups by type -- 5 octave bars "
    "(16',8',4',2',1') at block bytes 0-4, then 4 non-octave 'mutation' bars (5 1/3',2 2/3', "
    "1 3/5',1 1/3') at block bytes 5-8 -- NOT the harmonic order the NRPN performance path uses "
    "(see ORGAN_DRAWBAR_LABELS above). ALSO CONFIRMED: a SysEx write here lands and persists into "
    "the saved tone (survives save+reload) but does not reach the running voice in real time -- "
    "the drawbar additive table only recomputes on tone load. app/OrganPanel.cpp therefore writes "
    "Position via the separate NRPN live-fader path (sendDrawbarNrpn) for audible feedback while "
    "editing, translating each SysEx instance to its NRPN LSB via kSysExInstanceToNrpnLsb; this "
    "SysEx path remains authoritative for reads (Sync) only."
)

def build_organ_params():
    out = []
    for pid, name, idhex, vt, rng, default, group, ui, enum in ORGAN_PARAMS:
        is_position = pid == "organPosition"
        entry = {
            "id": pid,
            "name": name,
            "block": "DrawbarOrgan",
            "group": group,
            "address": {"ct": 0x07, "id": idhex, "ai": 0, "an": 0},
            "vt": vt,
            "range": {"min": rng[0], "max": rng[1]},
            "default": default,
            "unit": "",
            "ui": {"control": ui},
            "instances": {
                "count": 9 if is_position else 1,
                "blkByteOffset": 6,
                "addressByteIndex": 10,
                "idSuffix": False,
                "labels": ORGAN_DRAWBAR_LABELS if is_position else ["Organ"],
            },
        }
        if enum:
            entry["ui"]["enum"] = enum
        if is_position:
            entry["note"] = ORGAN_POSITION_NOTE
        if pid == "organRotaryType":
            entry["note"] = ORGAN_ROTARY_TYPE_NOTE
        out.append(entry)
    return out

organ_params = build_organ_params()
print(f"Drawbar Organ params: {len(organ_params)} (hand-authored, unverified against hardware)")

# ---------------------------------------------------------------------------
# 4d. Hex Layer params -- HAND-AUTHORED, not mined from Lua.
#
# franky's CTRLR panel DOES have a Hex Layer controller (020_ToneHexLayer.lua) but -- read in
# full this session -- it drives everything live via NRPN (sendHEXParam -> sendNRNP, MSB=0x3e
# fixed per g_mixModMidi["mixHEX"]["mixMSBid"] in 011_initTables.lua:491-505), never SysEx, and
# only for a SMALL "mixer" subset (per-layer level mixHEX1lvl..6lvl + all-layer
# cutoff/detune/attack/release) -- NOT the full per-layer offset/LFO param set this section
# covers. So there is no tone-edit-buffer Lua source for THIS domain either, same situation
# pcmMelody/drawbarOrgan were in. Every field below is transcribed directly from
# XWP1_midi_EN.pdf section 26 "Hex Layer Parameter" (printed p72-73), category 08H, XW-P1-only --
# reference/midi-spec.md section 5.7.
#
# SCOPE: only the 16 per-layer offset params (26.1 IDs 0000/0003/0004/0006-0012) + Detune Number
# (26.1 ID 0013) + all 14 LFO params (26.2 IDs 0015-0022) are included.
# Deliberately EXCLUDED, same "not a wave browser" call PCMEnginePanel made for tone/patch
# selection: Split Ui Number (id 0002, a per-layer PCM wave# picker -- out of scope, would need
# a wave browser UI, not just a fader) and Pitch Cent (id 0005, a genuinely different bit-packed
# sign+11-bit-fraction encoding -- "S------.- -------- S:sign bit -------.c cccccccc c:cent =
# 100/512-cent resolution" per the PDF -- not a linear signed value like every other param here;
# no existing vt fits it and no Lua source exists to cross-check a guess against). Both can be
# added later behind their own vt/UI work; this is a scope call, not an oversight (see the
# hexLayer section note below for the equivalent statement in the generated JSON). Owner also
# identified Pitch Cent (id 0005, still excluded here) as the synth's own "Fine Tune" control --
# wanted for a future pass, see the project memory note captured 2026-07-19.
#
# Pitch Lock (26.1 ID 0014) was ALSO removed 2026-07-19 after being hand-authored and shipped:
# owner hardware-tested it and confirmed no effect on pitch bend/transpose, and could not find a
# corresponding setting anywhere in the synth's own Hex Layer menu. It was already the weakest
# address inference in this section (see the old HEXLAYER_GLOBAL_PARAMS comment, now removed with
# it) -- dropped rather than kept as a guessed, apparently-inert control.
#
# ENCODING NOTES:
# - Most params are plain 'nf' (0-127) or 'cf' (-64..+63, wire=value+64, matches the PDF's
#   "00-40-7F" hex range exactly). The 8-bit "-128..+127" offset params (Amp Attack/Decay/Sustain/
#   Release Rate Offset, Volume/Cutoff/Touch Sense/Reverb Send/Chorus Send Offset, plus the two LFO
#   Auto Depth params) reuse the EXISTING 'cF' vt (already used by soloSynth, golden-tested), NOT a
#   new single-byte vt. [FIXED 2026-07-19, bug-198] A first pass invented a one-byte 'cf256'
#   (wire=value+128 in a single raw byte) -- broken because MIDI SysEx data bytes must have the
#   high bit clear (0-127 only); half of cf256's range (every UI value >= 0) produced an illegal
#   byte >=0x80, corrupting the frame. The PDF's "Size 8" column means 8 BITS OF RANGE (256 values),
#   which categorically cannot fit in one 7-bit-safe MIDI byte -- it needs 2 bytes on the wire, and
#   'cF' already does exactly that (wire=value+128 split lo7/hi7, same formula, MIDI-safe). Owner
#   hardware test caught this: every cf256 param synced back as -128 regardless of what was set
#   (decode's b0 defaulting to 0 when the malformed frame's value bytes never arrived intact).
# - Touch Sense Offset (id 000C) has the SAME manual inconsistency PCM's Touch Sense had (see
#   pcmMelody's TOUCH_SENSE_NOTE): hex Min-Def-Max column shows a non-centered default (00-BF-FF)
#   while the effective range is signed like its siblings. Resolved the same way: default=0
#   (centered), not the literal hex column.
# - Array->ai mapping: the PDF's "Array" column is franky's own vocabulary for the address 'ai'
#   byte (PROTOCOL.md line 111: "ai / array index"), confirmed by 3 independent hardware-verified
#   precedents in this repo (tssOSCPlfo2D/tssOSCAlfo2D/tssFLTFlfo2D, all manual "Array 02" -> ai=1,
#   see OVR above) -- the rule is ai = Array-1, not specific to those collision cases. Every param
#   remaining in this section has manual Array=01 (-> ai=0). (The one exception, Pitch Lock's
#   "Array 03" -> ai=2, was removed 2026-07-19 -- owner hardware-tested it as having no effect on
#   pitch bend/transpose and found no corresponding setting in the synth's own Hex Layer menu; see
#   the SCOPE note above. Its ai=2 was the one case in this section where the Array->ai rule was
#   applied without an independent id-collision to cross-check it against -- unlike this rule's 3
#   other precedents, which all resolve real collisions.)
# - Detune Number (0013) is the only param in section 26.1 whose PDF Block column reads the fixed
#   "00000000" instead of "up-arrow" (= 2-0:Layer Number, same as every param above it) -- i.e. it
#   is HEX-LAYER-WIDE (one value covering all 6 layers), not per-layer. Section 26.2's entire LFO
#   block (0015-0022, 14 params) is ALSO documented as fixed Block 00000000 -- one shared Pitch/
#   Amp LFO pair for the whole Hex Layer engine, not six independent per-layer LFOs. Both readings
#   are taken literally from the PDF table (not inferred) but are surprising enough to flag rather
#   than bury -- see the section note below.
HEXLAYER_LAYER_LABELS = [f"Layer {i}" for i in range(1, 7)]

# Per-layer params (block "2-0:Layer Number", instanceCount=6). id, name, hex id, vt, range,
# default, group, ui, enum.
HEXLAYER_LAYER_PARAMS = [
    ("hexOnoff",         "Layer On/Off",        0x00, "nf",    (0, 1),      0,   "General", "toggle", None),
    ("hexPanOffset",     "Pan Offset",          0x03, "cf",    (-64, 63),   0,   "General", "slider", None),
    ("hexPitchKey",      "Pitch Key",           0x04, "cf",    (-64, 63),   0,   "General", "slider", None),
    ("hexAmpAttackOfs",  "Amp Attack Rate Offset",  0x06, "cF", (-128, 127), 0, "Amp",   "slider", None),
    ("hexAmpDecayOfs",   "Amp Decay Rate Offset",   0x07, "cF", (-128, 127), 0, "Amp",   "slider", None),
    ("hexAmpSustainOfs", "Amp Sustain Level Offset",0x08, "cF", (-128, 127), 0, "Amp",   "slider", None),
    ("hexAmpReleaseOfs", "Amp Release Rate Offset", 0x09, "cF", (-128, 127), 0, "Amp",   "slider", None),
    ("hexVolumeOfs",     "Volume Offset",       0x0A, "cF", (-128, 127), 0,   "Amp",     "slider", None),
    ("hexCutoffOfs",     "Cutoff Offset",       0x0B, "cF", (-128, 127), 0,   "Filter",  "slider", None),
    ("hexTouchSenseOfs", "Touch Sense Offset",  0x0C, "cF", (-128, 127), 0,   "Filter",  "slider", None),
    ("hexReverbSendOfs", "Reverb Send Offset",  0x0D, "cF", (-128, 127), 0,   "Effects", "slider", None),
    ("hexChorusSendOfs", "Chorus Send Offset",  0x0E, "cF", (-128, 127), 0,   "Effects", "slider", None),
    ("hexKeyRangeLow",   "Key Range Low",       0x0F, "nf",    (0, 127),    0,   "Range",   "slider", None),
    ("hexKeyRangeHigh",  "Key Range High",      0x10, "nf",    (0, 127),    127, "Range",   "slider", None),
    ("hexVelRangeLow",   "Velocity Range Low",  0x11, "nf",    (0, 127),    0,   "Range",   "slider", None),
    ("hexVelRangeHigh",  "Velocity Range High", 0x12, "nf",    (0, 127),    127, "Range",   "slider", None),
]

# Hex-Layer-wide params (block fixed 00000000, instanceCount=1). Same tuple shape.
#
# Pitch Lock (id 0x14) was REMOVED 2026-07-19 (owner hardware test): confirmed to have no effect
# on pitch bend or transpose, and the owner could not find a corresponding setting anywhere in the
# synth's own Hex Layer menu. Its address was always the weakest inference in this section (the
# PDF marks it 'Array 03' where every sibling is 'Array 01', and the ai=2 reading applied here was
# pattern-matched from OTHER params' id-collision cases -- Pitch Lock's id isn't shared with
# anything, so there was no independent way to confirm it). Given the owner's negative test result,
# dropped rather than kept as a guessed, apparently-inert control. Revisit if a real corresponding
# synth-menu setting is ever identified.
HEXLAYER_GLOBAL_PARAMS = [
    # id,             name,           hex id, vt,  range,   default, group,    ui,      enum, ai
    ("hexDetuneNumber", "Detune Number", 0x13, "nf", (0, 31), 0, "General", "slider", None, 0),
]

HEXLAYER_LFO_NOTE = (
    "Block is fixed (00000000) for the ENTIRE LFO section (PDF sec 26.2 p72-73, IDs 0015-0022): "
    "one shared Pitch LFO + Amp LFO pair for the whole 6-layer Hex Layer engine, not six "
    "independent per-layer LFOs -- taken literally from the PDF table (same as Detune Number "
    "above), not inferred, but flagged here because it is architecturally surprising and "
    "unverified against real hardware."
)

TOUCH_SENSE_OFS_NOTE = (
    "midi-spec.md/PDF p72 sec 26.1: same manual inconsistency PCM's Touch Sense had (see "
    "pcmMelody's TOUCH_SENSE_NOTE) -- the hex Min-Def-Max column shows a non-centered default "
    "(00-BF-FF) while every sibling offset param in this block is centered (00-80-FF, default "
    "0x80). Encoded as cF (consistent with its siblings) with default 0 (centered), not the "
    "hex column's literal 0xBF. Needs hardware read-back to confirm."
)

def build_hexlayer_params():
    out = []
    for pid, name, idhex, vt, rng, default, group, ui, enum in HEXLAYER_LAYER_PARAMS:
        entry = {
            "id": pid,
            "name": name,
            "block": "Layer",
            "group": group,
            "address": {"ct": 0x08, "id": idhex, "ai": 0, "an": 0},
            "vt": vt,
            "range": {"min": rng[0], "max": rng[1]},
            "default": default,
            "unit": "",
            "ui": {"control": ui},
            "instances": {
                "count": 6,
                "blkByteOffset": 6,
                "addressByteIndex": 10,
                "idSuffix": False,
                "labels": HEXLAYER_LAYER_LABELS,
            },
        }
        if enum:
            entry["ui"]["enum"] = enum
        if pid == "hexTouchSenseOfs":
            entry["note"] = TOUCH_SENSE_OFS_NOTE
        out.append(entry)

    for pid, name, idhex, vt, rng, default, group, ui, enum, ai in HEXLAYER_GLOBAL_PARAMS:
        entry = {
            "id": pid,
            "name": name,
            "block": "Global",
            "group": group,
            "address": {"ct": 0x08, "id": idhex, "ai": ai, "an": 0},
            "vt": vt,
            "range": {"min": rng[0], "max": rng[1]},
            "default": default,
            "unit": "",
            "ui": {"control": ui},
            "instances": {
                "count": 1,
                "blkByteOffset": 0,
                "addressByteIndex": 10,
                "idSuffix": False,
                "labels": ["Hex Layer"],
            },
        }
        if enum:
            entry["ui"]["enum"] = enum
        out.append(entry)

    for pid, name, idhex, vt, rng, default in HEXLAYER_LFO_PARAMS:
        entry = {
            "id": pid,
            "name": name,
            "block": "Global",
            "group": "LFO",
            "address": {"ct": 0x08, "id": idhex, "ai": 0, "an": 0},
            "vt": vt,
            "range": {"min": rng[0], "max": rng[1]},
            "default": default,
            "unit": "",
            "ui": {"control": "combo" if vt == "hexLfoWaveEnum" else "slider"},
            "instances": {
                "count": 1,
                "blkByteOffset": 0,
                "addressByteIndex": 10,
                "idSuffix": False,
                "labels": ["Hex Layer"],
            },
            "note": HEXLAYER_LFO_NOTE,
        }
        if vt == "hexLfoWaveEnum":
            entry["vt"] = "nf"
            entry["ui"]["enum"] = "hexLayerLfoWave"
        out.append(entry)

    return out

# LFO params (26.2): id, name, hex id, vt ('hexLfoWaveEnum' is a marker, resolved to nf+enum
# above, not a real vt), range, default.
HEXLAYER_LFO_PARAMS = [
    ("hexPitchLfoWave",     "Pitch LFO Wave Type",   0x15, "hexLfoWaveEnum", (0, 6),      0),
    ("hexPitchLfoRate",     "Pitch LFO Rate",        0x16, "nf",             (0, 127),    64),
    ("hexPitchAutoDelay",   "Pitch Auto Delay",      0x17, "nf",             (0, 127),    0),
    ("hexPitchAutoRise",    "Pitch Auto Rise",       0x18, "nf",             (0, 127),    0),
    ("hexPitchAutoDepth",   "Pitch Auto Depth",      0x19, "cF",             (-128, 127), 0),
    ("hexPitchModDepth",    "Pitch Mod Depth",       0x1A, "cf",             (-64, 63),   0),
    ("hexPitchAfterDepth",  "Pitch After Depth",     0x1B, "cf",             (-64, 63),   0),
    ("hexAmpLfoWave",       "Amp LFO Wave Type",     0x1C, "hexLfoWaveEnum", (0, 6),      0),
    ("hexAmpLfoRate",       "Amp LFO Rate",          0x1D, "nf",             (0, 127),    64),
    ("hexAmpAutoDelay",     "Amp LFO Auto Delay",    0x1E, "nf",             (0, 127),    0),
    ("hexAmpAutoRise",      "Amp LFO Auto Rise",     0x1F, "nf",             (0, 127),    0),
    ("hexAmpAutoDepth",     "Amp LFO Auto Depth",    0x20, "cF",             (-128, 127), 0),
    ("hexAmpModDepth",      "Amp LFO Mod Depth",     0x21, "cf",             (-64, 63),   0),
    ("hexAmpAfterDepth",    "Amp LFO After Depth",   0x22, "cf",             (-64, 63),   0),
]

hexlayer_params = build_hexlayer_params()
print(f"Hex Layer params: {len(hexlayer_params)} (hand-authored, unverified against hardware)")

HEXLAYER_SECTION_NOTE = (
    "HAND-AUTHORED from XWP1_midi_EN.pdf section 26 'Hex Layer Parameter' (printed p72-73), "
    "category 08H (XW-P1 only) -- franky's CTRLR panel has a Hex Layer controller "
    "(020_ToneHexLayer.lua) but it drives everything live via NRPN (sendHEXParam/g_mixModMidi"
    "['mixHEX'], MSB=0x3e), never SysEx, and only for the small per-layer-level + all-layer-"
    "cutoff/detune/attack/release mixer subset -- NOT this section's full per-layer offset/LFO "
    "param set. So there is no tone-edit-buffer Lua source to mine here, same situation "
    "pcmMelody/drawbarOrgan were in. addr/vt follow the general SX frame field layout "
    "(midi-spec.md section 2) that soloSynth/pcmMelody/drawbarOrgan already match; SysExCodec "
    "needed zero changes, same as every other hand-authored section -- the 8-bit '-128..+127' "
    "offset params reuse the EXISTING 'cF' vt (2-byte, MIDI-safe), NOT a new one. [bug-198, "
    "2026-07-19] A first pass invented a broken single-byte 'cf256' (wire=value+128 in one raw "
    "byte, illegal per MIDI's high-bit-clear rule for SysEx data bytes) -- owner hardware testing "
    "caught it immediately (every affected param synced back as -128 regardless of the real "
    "value); fixed by switching those params to 'cF', which already encodes this exact -128..+128 "
    "shape correctly (2 bytes, lo7/hi7 split). "
    "SCOPE: excludes Split Ui Number (per-layer PCM wave# picker, id 0002 -- out of scope, same "
    "'not a wave browser' call PCMEnginePanel made for tone/patch selection; owner has asked for "
    "wave picking here and in the PCM editor as a follow-up) and Pitch Cent (id 0005 -- a "
    "genuinely different sign+11-bit-fraction bit-packed encoding with no existing vt and no Lua "
    "source to cross-check; owner identified this as the synth's own 'Fine Tune' control and wants "
    "it implemented too). Pitch Lock (id 0014) was hand-authored and shipped, then REMOVED "
    "2026-07-19 after owner hardware testing found no effect on pitch bend/transpose and no "
    "corresponding setting in the synth's own Hex Layer menu -- see HEXLAYER_GLOBAL_PARAMS' "
    "comment above. NOT YET HARDWARE-VERIFIED for the remaining params (owner has not run a full "
    "live round-trip on this section) -- given the organPosition precedent (a SysEx write that "
    "landed and persisted but did not reach the running voice, needing an NRPN live-fader path "
    "instead, see drawbarOrgan's section note), a cat=0x08 write here may face the same issue "
    "since the Lua's own live path is NRPN (mixHEX), not SysEx -- budget a midi-probe read/write/"
    "audible check before trusting this panel's Sync/edit path, and if writes prove SysEx-inert, "
    "the known fallback is the same NRPN-fader pattern app/OrganPanel.cpp already established "
    "(sendDrawbarNrpn) rather than a new mechanism. Detune Number (hex-layer-wide, not per-layer) "
    "and the entire LFO section (also hex-layer-wide, one shared Pitch/Amp LFO pair for all 6 "
    "layers) are taken literally from the PDF's Block column -- see HEXLAYER_LFO_NOTE for the "
    "specific caveat."
)

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
    # Melody (PCM engine) Vibrato Type (PDF p70 sec 23): 0=Sine/1=Triangle/2=Saw/3=Square.
    # Distinct from soloSynth's 8-value lfoWave enum -- Melody's is its own, smaller list.
    "melodyVibratoType": [{"value":0,"label":"Sine"},{"value":1,"label":"Triangle"},
                            {"value":2,"label":"Saw"},{"value":3,"label":"Square"}],
    # Drawbar Organ Percussion mode (midi-spec.md section 5.6 / PDF p71-72 sec 25):
    # 0=off,1=2nd,2=3rd,3=2nd+3rd. Collapses franky's live-path two separate booleans
    # (orgTWperc2/orgTWperc3) into the one enum column the manual's SysEx table documents.
    "organPercussionMode": [{"value":0,"label":"Off"},{"value":1,"label":"2nd"},
                              {"value":2,"label":"3rd"},{"value":3,"label":"2nd + 3rd"}],
    # Drawbar Organ Rotary Type -- OWNER-VERIFIED ON HARDWARE 2026-07-18: a rotary-speaker/vibrato
    # character switch, values Sine/Vintage (NOT "Normal"/"Vintage" as the manual's section 25
    # table names it -- see organRotaryType's own note).
    "organRotaryType": [{"value":0,"label":"Sine"},{"value":1,"label":"Vintage"}],
    # Hex Layer LFO Wave Type (PDF sec 26.2 p72-73): 7 values, 0-6, no "Random" -- distinct from
    # soloSynth's 8-value lfoWave enum (which adds Random at 7). Backs BOTH Pitch LFO Wave Type
    # and Amp LFO Wave Type (same list, per the PDF).
    "hexLayerLfoWave": [{"value":0,"label":"Sine"},{"value":1,"label":"Triangle"},
                          {"value":2,"label":"Saw Up"},{"value":3,"label":"Saw Down"},
                          {"value":4,"label":"Pulse 1:3"},{"value":5,"label":"Pulse 2:2"},
                          {"value":6,"label":"Pulse 3:1"}],
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
    "hexLayer":    {
      "category": "0x08",
      "status": "complete",
      "note": HEXLAYER_SECTION_NOTE,
      "params": hexlayer_params
    },
    "pcmMelody":   {
      "category": "0x05",
      "status": "complete",
      "note": "HAND-AUTHORED from XWP1_midi_EN.pdf section 23 'Melody Parameter' (printed p70) -- "
              "no Lua source exists for this domain (franky's CTRLR panel never implemented a Melody/"
              "PCM tone editor page). addr/vt follow the general SX frame field layout (midi-spec.md "
              "section 2) that soloSynth's franky-derived 18-byte address also happens to match, so "
              "encode()/decode() need no codec changes. PARTIAL HARDWARE ROUND-TRIP 2026-07-18 "
              "(owner + midi-probe on a real XW-P1): the 9 sound-shaping params (Attack/Release/"
              "Cutoff/Vibrato*/Octave Shift/Touch Sense) DO read back correctly from cat=0x05, so "
              "that category/address landing is confirmed for them. The 10th, Volume, did NOT: it "
              "was mistranscribed to cat05/0x1F (a live but ineffective register) -- the real PCM "
              "tone Volume is the Tone-category Level at cat=0x03/id=0x08, now corrected and "
              "hardware-verified (see pcmVolume's own note). Ranges/defaults of the cf params remain "
              "unconfirmed for exact bounds; do not treat the whole section as soloSynth-grade yet.",
      "params": melody_params
    },
    "drawbarOrgan": {
      "category": "0x07",
      "status": "complete",
      "note": "HAND-AUTHORED from XWP1_midi_EN.pdf section 25 'Drawbar' (printed p71-72), "
              "category 07H (XW-P1 only) -- franky's CTRLR panel has an organ controller "
              "(022_XWOrgan.lua) but it drives everything live via NRPN/CC "
              "(g_orgModMidi/011_initTables.lua), never SysEx, so there is no tone-edit-buffer "
              "Lua source to mine here, same situation as pcmMelody. addr/vt follow the general "
              "SX frame field layout (midi-spec.md section 2) that soloSynth and pcmMelody both "
              "already match, so encode()/decode() need no codec changes. HARDWARE-VERIFIED "
              "2026-07-18 (owner + midi-probe on a real XW-P1): every param round-trips "
              "correctly. organPosition (the drawbars) needed one real fix -- see its own "
              "param-level note -- its 9-instance ORDER was wrong (assumed harmonic, actually "
              "grouped by octave-vs-mutation type) and its writes needed rerouting to the NRPN "
              "live-fader path since the SysEx edit-buffer write doesn't reach the running voice "
              "in real time. Every other param in this section applies live via SysEx as-is.",
      "params": organ_params
    },
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
