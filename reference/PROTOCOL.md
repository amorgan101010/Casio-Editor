# Casio XW-P1 SysEx Protocol

Reverse-engineered from the CTRLR panel `CasioXW_EDITOR/CasioXW-EDITOR-v0101.bpanelz`
(author: **franky**). All facts below are quoted from the embedded Lua, extracted to
`reference/lua/`. File/line references point at those extracted `.lua` files.

> This is a distillation for implementers. When in doubt, the Lua is the source of truth —
> `reference/lua/_all_lua.lua` is a single concatenated copy for grepping.

---

## 1. Message frame / manufacturer ID

Every XW parameter SysEx message the editor sends is built by **one** function,
`sendXWSX` (`014_globalFunctions.lua:320`):

```lua
function sendXWSX(act, _msgid, v1, v2, v3)
    -- act: 0 = request data, 1 = send data
    local _val = ""
    if       not v1 then _val = ""                                  -- request (no value)
    elseif   not v2 then _val = string.format("%.2x "          ,v1)
    elseif   not v3 then _val = string.format("%.2x %.2x "     ,v2, v1)   -- invert msb/lsb to lsb-msb
    else                 _val = string.format("%.2x %.2x %.2x ",v3, v2, v1)
    end
    sxmsg = string.format("f0 44 16 03 7f %.2x %s %sf7", act, _msgid, _val)
    sendMidiMsg(sxmsg)
end
```

So the wire frame is:

```
F0 44 16 03 7F <act> <_msgid ...> <value bytes, LSB first> F7
```

| Byte(s)      | Value       | Meaning                                                        |
|--------------|-------------|----------------------------------------------------------------|
| `F0`         | 0xF0        | SysEx start                                                     |
| `44`         | **0x44**    | **Casio manufacturer ID**                                      |
| `16`         | 0x16        | Casio model / format id (constant for XW)                      |
| `03`         | 0x03        | constant (sub-model / category-class)                          |
| `7F`         | 0x7F        | device id (broadcast / "all")                                  |
| `<act>`      | see below   | action / direction byte                                        |
| `<_msgid>`   | 18 bytes    | parameter address block (see §3)                               |
| `<value>`    | 0–3 bytes   | parameter value, **LSB byte(s) first** (§4)                    |
| `F7`         | 0xF7        | SysEx end                                                      |

**`act` (action) byte values seen:**
- `0x00` — **request** current value of the addressed parameter (no value bytes follow).
- `0x01` — **send / set** the addressed parameter (value bytes follow).
- `0x0B` — **request reject** answer from the device. `015_midiReceive.lua:224` comment:
  `-- eventually kick 'request reject' answer: f0 44 16 03 7f 0b 00 00 00 00 f7`

There is **no checksum** — the message ends directly with `F7`. (`sendMidiMsg`'s docstring
mentions a Roland checksum path, but the XW path passes a fully-formed hex string and adds nothing.)

---

## 2. `g_XWSysEx` — tone-type headers

Defined in `010_initPanel.lua:167`:

```lua
g_XWSysEx = {
    syn = "f0 44 16 03 7f 01 09",   -- tone solo synth
    hex = "f0 44 16 03 7f 01 08",   -- tone hex layer
    dsp = "f0 44 16 03 7f 01 13",   -- dsp for tss and general
}
```

Decoded, the differentiating bytes are `<act=01> <category>`:

| Name  | Header bytes               | act  | **category (ct)** | Meaning                          |
|-------|----------------------------|------|-------------------|----------------------------------|
| `syn` | `F0 44 16 03 7F 01 09`     | 0x01 | **0x09**          | Solo Synth tone (OSC/PWM/ETC/FLT/LFO) |
| `hex` | `F0 44 16 03 7F 01 08`     | 0x01 | **0x08**          | Hex Layer tone                   |
| `dsp` | `F0 44 16 03 7F 01 13`     | 0x01 | **0x13**          | DSP (per-tone effects + general) |

The category byte is byte index 6 of the message. Note `syn` covers **all** solo-synth
sub-blocks (OSC, PWM, ETC, FLT, LFO) — they all carry `ct = 0x09`
(`011_initTables.lua:83,148,156,172,197`). DSP blocks carry `ct = 0x13`
(`011_initTables.lua:210,282`).

---

## 3. Parameter address (`_msgid`) — the 18-byte block

The address is generated once at init by `createSXtssArray` / `createSXdspArray`
(`011_initTables.lua:244,315`) with this exact format string and layout comment:

```lua
--        f0 44 16 03 7f 00 cat  me pset. bk prm.... idx..   arr.... f7
sysex = string.format("%.2x 00 00 00 %s %.2x 00 %.2x 00 %.2x 00",
                        _a.sx.ct,  _a.blk(b-1),  _m.id,  _m.ai,  _m.an)
--        ct        00 00 00  <blk: 8 bytes>   id  00   ai  00   an  00
```

The stored `_msgid` is **18 bytes** and starts at the category byte (`ct`); `sendXWSX`
prepends `F0 44 16 03 7F <act>` and appends value + `F7`.

Layout of the 18-byte address:

| Offset (in address) | Field   | Bytes | Source          | Meaning                                             |
|---------------------|---------|-------|-----------------|-----------------------------------------------------|
| 0                   | `ct`    | 1     | `sx.ct`         | category (0x09 syn / 0x08 hex / 0x13 dsp)           |
| 1–3                 | —       | 3     | `00 00 00`      | fixed                                                |
| 4–11                | `blk`   | 8     | `blk(b)`        | block selector (see below)                           |
| 12                  | `id`    | 1     | `_m.id`         | **parameter id** within the block                   |
| 13                  | —       | 1     | `00`            | fixed                                                |
| 14                  | `ai`    | 1     | `_m.ai`         | array index (sub-parameter / element index)         |
| 15                  | —       | 1     | `00`            | fixed                                                |
| 16                  | `an`    | 1     | `_m.an`         | array number                                        |
| 17                  | —       | 1     | `00`            | fixed                                                |

In the full on-wire message these sit at byte indices **6..23**; the receiver keys on them
with `midi.msg:getData():getRange(6,18)` (`015_midiReceive.lua:21`).

**`blk` — the 8-byte block selector.** For per-oscillator blocks it encodes the oscillator
number in the 7th block byte:

```lua
blk = function(b) return "00 00 00 00 00 00 0"..b.." 00" end   -- 011_initTables.lua:84
```

Blocks with `sx.bn > 0` are expanded per instance `b = 1..bn`, producing modulator ids
suffixed `-1..-bn` (e.g. `tssOSCsw-1` … `tssOSCsw-6`); blocks with `sx.bn == 0` are a single
instance (`011_initTables.lua:250-262`). `bn` per block: OSC=6, PWM=2, ETC=1, FLT=1, LFO=2
(the 6 solo-synth oscillators, etc.).

### Example: reconstruct one parameter address

`tssOSCsw` (OSC on/off) is defined (`011_initTables.lua:85`):
```lua
tssOSCsw = {id=0x00, ai=0, an=0, vt='nf'}   -- inside tssModSX["tssOSC"], sx={ct=0x09, bn=6}
```
For oscillator 1 (`b=1`, so block byte `0`+(b-1)=`00`), the 18-byte address is:
```
09 00 00 00  00 00 00 00 00 00 00 00  00 00  00 00  00 00
```
Full **set** message = `F0 44 16 03 7F 01` + that + `<value LSB..>` + `F7`.
Full **request** = `F0 44 16 03 7F 00` + that + `F7`.

Another: `tssOSCwf` (synth wave number) `= {id=0x03, ai=0, an=0, vt='wf'}` — `id` byte at
offset 12 becomes `03`.

---

## 4. Value encoding (`vt` value-types)

Each parameter carries a value-type `vt`. Two lookup tables convert between the UI value and
the Casio wire bytes (`011_initTables.lua:36` and `:51`):

- `g_xwModCalc["V2SX"][vt](val)` — UI → wire, returns bytes in **(sign/mmsb, msb, lsb)** order.
- `g_xwModCalc["SX2v"][vt](v1,v2,...)` — wire → UI (inverse).

`sendXWSX` then **reverses** those bytes onto the wire so the value is transmitted **LSB first**
(see the `%.2x %.2x` ordering in §1).

Key `V2SX` encoders (`011_initTables.lua:36-49`):

```lua
nf  = function(v)  return v      end,                                 -- normal fader 0..127 (1 byte)
cf  = function(v)  return v+64   end,                                 -- centered fader -64..+63 (1 byte)
nF  = function(v)  return bit.rshift(v,7), bit.band(v,0x7f) end,      -- double-byte fader -> msb,lsb
cF  = function(v)  v=v+128 ; return bit.rshift(v,7),bit.band(v,0x7f) end, -- -128..+128  // sysex: lsb,msb, 40 == 04
sw  = function(v)  return v*127  end,                                 -- switch 0/127
nfx = function(v,n) return math.floor(v*126/n) end,                  -- fader with reduced max (< 127); n from vt 'nf-<n>'
db  = function(v)  return (8-v)  end,                                 -- drawbars (inverted)
wf  = function(v,o) v=v+g_tsswf[o]; return 0,bit.rshift(v,7),bit.band(v,0x7f) end, -- wave number, 3 bytes, base offset per osc
tn  = function(v)  v=2*(v+256); return bit.rshift(v,7), bit.band(v,0x7f) end,      -- tune/detune -256..+256 (x2!)
pk  = function(v)  local sgn=0;local sx=0x30*v;if v<0 then sgn=0x7f;sx=0x4000+sx;end;
                   return sgn,bit.rshift(sx,7),bit.band(sx,0x7f) end, -- pitch key -256..+256 * 0x30, signed, 3 bytes
dt  = function(v)  v=v+0x80; return bit.rshift(v,7), bit.band(v,0x7f) end,          -- tss DSP type
```

Corresponding `SX2v` decoders (`011_initTables.lua:51-64`) invert them, e.g.
`cF = function(v1,v2) return v1 + bit.lshift(v2,7) - 128 end`,
`tn = function(v1,v2) return (v1 + bit.lshift(v2,7))/2 - 256 end`.

**`vt` naming convention:** a `vt` like `"nf-3"` means encoder `nfx` with `n=3` (reduced max).
Handlers detect this with `vt:match("nf-")` and pass `tonumber(vt:sub(4))` as `n`
(`019_ToneSoloSynth.lua:79`, `015_midiReceive.lua:71`).

So a single-byte 0..127 fader is `vt='nf'`; 2-byte values (`cF`,`nF`,`tn`,`dt`) send **lsb then
msb**; 3-byte values (`wf`,`pk`) send **lsb, msb, mmsb/sign**.

---

## 5. Sending a single parameter edit (TX path)

`019_ToneSoloSynth.lua` is the canonical example. On any `tss*` modulator change,
`ToneSoloSynth(mod, value)` → `sendTSSParamSX(mid, val)` (`:102`):

```lua
mdef = g_tssModSXtx                       -- modid -> { sx=<18-byte address>, vt=... }
sxid = mdef[mid].sx
if mdef[mid].vt:match("wf") then n = tonumber(mid:sub(-1,-1),10) or 1 end   -- osc suffix for wave
if mcalc[mdef[mid].vt] then v1,v2,v3 = mcalc[mdef[mid].vt](val,n) end       -- V2SX encode -> mmsb,msb,lsb
sendXWSX(1, sxid, v1, v2, v3)             -- act=1 (set)
```

`g_tssModSXtx` is the generated map **modulator-id → {sx=address, vt}**; `g_tssModSXrx` is its
inverse **address → {id, vt}** (`011_initTables.lua:253-260`). The DSP equivalents are
`g_dspModSXtx` / `g_dspModSXrx` (`:312-324`).

(There is also a legacy **NRPN/CC** path — `sendCC`, `sendNRNP`, `sendPC` in
`014_globalFunctions.lua:336-366`, and the commented-out `sendTSSParam` in
`019_ToneSoloSynth.lua:56`. The **active** editor path for tone edits is SysEx via `sendXWSX`.
NRPN uses CC 0x63/0x62 (param msb/lsb) + CC 0x06/0x26 (value msb/lsb); PC uses CC0/CC32 bank +
program change.)

---

## 6. Receiving / parsing (RX path)

`015_midiReceive.lua`. Entry `midiReceive(midiMSG)` filters to SysEx of size ≥ 20 starting with
`0xF0`, then `procSysEx` dispatches on the **first 7 bytes** (`getRange(0,7)`):

```lua
local midicat = midi.msg:getData():getRange(0,7):toHexString(1)
if     midicat == g_XWSysEx.syn then rxTSS(midi)   -- f0 44 16 03 7f 01 09  solo synth
elseif midicat == g_XWSysEx.dsp then rxDSP(midi)   -- f0 44 16 03 7f 01 13  DSP
end
```

`rxTSS` (`:17`) reads the address key and value bytes:

```lua
sysex.sx = midi.msg:getData():getRange(6,18):toHexString(1)   -- 18-byte address key
if g_tssModSXrx[sysex.sx] then
    sysex.mid = g_tssModSXrx[sysex.sx].id                     -- modulator id
    sysex.vt  = g_tssModSXrx[sysex.sx].vt
    sysex.db  = midi.size-25                                  -- number of value bytes
    sysex._f  = g_xwModCalc["SX2v"][sysex.vt]                 -- decoder
    for b=1,sysex.db do v[b] = midi.msg:getData():getByte(23+b) end   -- value bytes start at index 24
    sysex.mval = sysex._f(v[1],v[2],v[3],v[4])                -- decode -> UI value
    setModNameVal(sysex.mid, sysex.mval)                      -- update UI
end
```

Byte accounting: header+address occupy indices 0..23 (6 header + 18 address); value bytes begin
at index 24 (`getByte(23+b)`, b≥1); `db = size - 25` (subtracting the 24 leading bytes + trailing
`F7`). `rxDSP` (`:52`) is the same but additionally folds in the current DSP tab id
(`tssDSPTab` + 0x80) to disambiguate DSP type sub-tables.

---

## 7. Bulk / tone sync ("dump")

The XW-P1 has **no single whole-tone dump message** in this map. The editor "syncs" a tone by
firing an individual **request** (`act=0x00`) for every parameter address and letting the RX path
repopulate the UI. See `syncXW` (`018_SYSControllers.lua:22`):

```lua
mdef = g_dspModSXtx ; sendXWSX(0, mdef["tssDSPTab"].sx)           -- first request DSP type
mdef = g_tssModSXtx ; for m,av in pairs(mdef) do sendXWSX(0, av.sx) end  -- request every solo-synth param
```

Each request `F0 44 16 03 7F 00 <18-byte address> F7` is answered by the device with a `01`
(set) message carrying the current value, decoded by `rxTSS`/`rxDSP`. A parameter the device
does not know returns the reject frame (`act=0x0B`, §1).

---

## 8. Parameter address tables (where the map lives)

The complete per-parameter map is in `011_initTables.lua`:

- `tssModSX["tssOSC"]` (`:82`, ct=0x09, bn=6) — oscillator params (`tssOSCsw`, `tssOSCwf`,
  `tssOSCPortaSw/Tm`, `tssOSCLegatoSw`, pitch/ENV rows, …). `tssMSBid = {0x30..0x35}` per osc
  (`:342`) is used by the NRPN path.
- `tssModSX["tssPWM"]` (`:147`, bn=2), `tssModSX["tssETC"]` (`:155`, bn=1),
  `tssModSX["tssFLT"]` (`:171`, bn=1), `tssModSX["tssLFO"]` (`:196`, bn=2).
- `dspModSX["tssDSP"]` (`:281`, ct=0x13, bn=0) — per-effect DSP params with an extra `tid`
  (type/tab id) field: PAN `0x81`, DST `0x82`, FLG `0x83`, CHO `0x84`, DEL `0x85`, RMD `0x86`;
  the type selector itself `tssDSPTab = {tid=0x80, id=0x02, vt='dt'}`.

Each entry is `{id=<byte>, ai=<byte>, an=<byte>, vt=<value-type>}` (DSP entries add `tid`).
These four fields + the block's `ct`/`blk` fully determine the 18-byte address (§3).

Other tone-type handlers (mix / organ / hex layer) live in
`021_XWMixer.lua`, `022_XWOrgan.lua`, `020_ToneHexLayer.lua`, `023_DSPHandler.lua` and reuse the
same frame; the Mixer/registration path additionally uses bank-select + program-change
(`sendPC`, `021_XWMixer.lua:63-65`). `012_initWaves.lua` (86 KB) holds the wave-number lists
referenced by the `wf` value-type (`g_tsswf`, base offsets per oscillator).

---

## Notes for Chunk 3 (JSON param map)

- The authoritative parameter list = the `tssModSX` / `dspModSX` tables in
  `011_initTables.lua`. Each JSON param needs: `ct`, `blk`-pattern (esp. the per-osc block byte),
  `id`, `ai`, `an`, and `vt`. The full 18-byte address is derivable from those.
- **`vt` drives value scaling.** Port the `V2SX`/`SX2v` pairs faithfully — several are
  non-obvious: `tn` multiplies by 2 (±256 → ±512 range), `pk` uses a signed 0x4000 offset with a
  separate sign byte, `cF`/`nF` are 2-byte LSB-first, `wf` is 3-byte with a per-oscillator base
  offset from `g_tsswf` (P1 vs G1 differ — `011_initTables.lua:26-27`).
- **Byte order on the wire is LSB-first**, opposite to the `(msb,lsb)` order the encoders
  return; `sendXWSX` reverses. Do not double-reverse.
- **`vt='nf-<n>'`** is not a literal type; it is `nfx` with parameter `n`. Parse the suffix.
- **Oscillator suffixing:** solo-synth params exist per oscillator (`-1..-6`), encoded in block
  byte index 6 of the 8-byte `blk`. Wave params (`wf`) also key their base offset on the osc index.
- **DSP disambiguation:** DSP RX must combine the address with the active DSP `tid` (tab + 0x80)
  — the same 18-byte address maps to different params depending on selected DSP type
  (`015_midiReceive.lua:57-72`).
- P1 vs G1 model differences exist (wave counts, enabled sections) — `g_XWModel`,
  `g_XWTssWf.P1/.G1`. This editor targets **P1**.
- No checksum, no whole-tone dump — sync is param-by-param request/response.
