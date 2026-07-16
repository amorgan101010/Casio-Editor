# Casio XW-P1 — MIDI / SysEx Implementation Spec

Implementer-facing distillation of Casio's official **"XW-P1/XW-G1 MIDI Implementation"**
document (`XWP1_midi_EN.pdf`, 111 pages, © CASIO 2012). Page numbers in this file refer to the
**printed page numbers of that PDF** (they coincide with the physical PDF page numbers).

This spec covers **both** the XW-P1 and XW-G1. Rows/areas marked *XW-P1 only* or *XW-G1 only*
apply to only one model. **For this project (XW-P1)**: Drawbar and Hex Layer are relevant; the
XW-G1-only **User Wave** area is not present on our hardware and can be ignored by the codec.

> **Extraction provenance:** primary text from `pdftotext -layout` (cleanest, most consistent for
> these fixed-column tables); cross-checked against `uvx --from 'markitdown[pdf]' markitdown`. The
> two agree on every numeric fact spot-checked (category IDs, manufacturer/model ID, act codes,
> parameter-set cat/mem/pset table, Part/Tone tables) — **no numeric discrepancies found**.
> markitdown's tables were actually messier (frequent bad column splits), so `-layout` was used as
> the transcription source throughout. The one flagged ambiguity (MOD `02H` vs `03H`, §0) is a
> source-document inconsistency present identically in both extractions, not an extraction artifact.

> Cross-check: the CTRLR Lua header `F0 44 16 03 7F 01 09` decodes exactly as
> `SX(F0) MAN(44) MOD-MSB(16) MOD-LSB(03) dev(7F) act(01=IPS) cat(09=Solo Synth)` — confirming
> the frame layout documented below.

---

## 0. Quick reference — the numbers you need constantly

| Field | Value | Source |
|---|---|---|
| Manufacturer ID (MAN) | `44H` (Casio Computer Co. Ltd) | p40, p44 |
| Model ID (MOD) | `16H 03H` (MSB, LSB) — XW-P1/XW-G1 | p44 |
| SysEx status / EOX | `F0H` … `F7H` | p44, p51 |
| Instrument-specific SysEx prefix | `F0 44 16 02 …` (overview) / `F0 44 16 03 …` (actual frames) | p42 / p44 |
| Default device ID | configurable; `7FH` = broadcast/"All" (always received) | p40, p44; Spec `Device ID` p83 |
| CRC | CRC-32 (ISO 8802-3 / IEEE 802.3), only on `OBS`/`HBS` img packets | p51 |
| Max single message size | 256 bytes (handshake bulk), 48 bytes (all other cases) — adjustable | p50 |

> **Note — MOD "02H" vs "03H":** The overview section (p42) writes the instrument-specific prefix
> as `F0 44 16 02 …`, but the formal field definition (§16.3.3, p44) and every real frame use
> **`16H 03H`** as the two model-ID bytes. Treat `16 03` as the model ID. The `02` in the p42
> example appears to be a documentation slip; **verify against hardware round-trip** (main agent) if
> it ever matters. (p42 vs p44 — flagged as ambiguous in source.)

---

## 1. Global / Channel MIDI

### 1.1 Product configuration (p9–10)
Three sections: **System** (status + user data / bulk dump), **Performance Controller** (keyboard,
knobs, auto-play → generates channel messages), **Sound Generator** (a channel-independent *common
block* = system FX + master, plus **16 instrument parts**).

- All 16 parts: MIDI receive Ch and send Ch are each independently settable **01–16** (p10).
- Parts **01–04** form the **Zones** (zone editor). Parts 08–16 map to Step Sequencer tracks
  (Drum1-5, Bass, Solo1/2, Chord); part 01 also = Step Seq Solo1. (Full table p10.)
- No MIDI is sent/received while "Please Wait ..." is on the display (p10).
- **Basic Channel** and **Device ID** are set via the *Spec* parameter area (IDs `0045H`, `0044H` — see §6.16).

### 1.2 Note On / Note Off (p11)
- Note Off: `8n kk vv` (send vv=40H) **or** `9n kk 00` (receive only).
- Note On: `9n kk vv`. Key number is shifted by Transpose / Octave Shift on send.

### 1.3 Control Change (p12–23)
Format `Bn cc vv`. **Assignable knobs** can send any CC `00H–65H`; **multi-function key**
(XW-G1 only) any `00H–77H`; **virtual controller** (Solo Synth tone) can map `00H–61H` + aftertouch.

Standard CC used by the synth:

| CC (hex) | Function | Notes |
|---|---|---|
| `00`/`20` | Bank Select MSB / LSB | LSB ignored on receive; tone not changed until Program Change (§1.5) |
| `01` | Modulation | depth add; effect tone-dependent |
| `05` | Portamento Time | **Solo Synth tone only** |
| `06`/`26` | Data Entry MSB / LSB | carries RPN/NRPN values |
| `07` | Volume | mixer part volume |
| `0A` | Pan | see Pan value table §7.3 |
| `0B` | Expression | |
| `10–13`, `50–53` | General Purpose 1–8 → **DSP Parameter7[1]–[8]** | value 0–127 auto-scaled to param range (formula below) |
| `40` | Hold1 (sustain) | per Timbre Type; ignored for Drum |
| `41` | Portamento On/Off | Solo Synth tone only |
| `42` | Sostenuto | |
| `43` | Soft | |
| `47` | Filter Resonance | (non-drawbar) |
| `48` | Release Time | (non-drawbar), rel. −64..+63 |
| `49` | Attack Time | (non-drawbar) |
| `4A` | Filter Cut Off | (non-drawbar) |
| `4C` | Vibrato Rate | (non-drawbar) |
| `4D` | Vibrato Depth | (non-drawbar) |
| `4E` | Vibrato Delay | (non-drawbar) |
| `5B` | Reverb Send | |
| `5D` | Chorus Send | |
| `62`/`63` | NRPN LSB / MSB | see §1.4 |
| `64`/`65` | RPN LSB / MSB | see §1.4 |
| `78` | All Sound Off | |
| `79` | Reset All Controllers | |
| `7B` | All Notes Off | |
| `7C`/`7D`/`7E`/`7F` | Omni Off / Omni On / Mono / Poly | all behave as All Notes Off |

**DSP Parameter7 value scaling (p15):**
`ParamValue = ParamMin + (ParamMax − ParamMin) × (ReceivedValue / 127)`

**Drawbar CC remap (XW-P1, when a Drawbar tone is selected — p12):** CCs `46–4F` and `54–5A` are
reinterpreted as drawbar foot positions / organ controls instead of their normal meaning:

| CC | Non-drawbar | Drawbar tone |
|---|---|---|
| `46` | – | Drawbar Position 16' |
| `47` | Filter Resonance | Drawbar 5 1/3' |
| `48` | Release Time | Drawbar 8' |
| `49` | Attack Time | Drawbar 4' |
| `4A` | Filter Cut Off | Drawbar 2 2/3' |
| `4B` | – | Drawbar 2' |
| `4C` | Vibrato Rate | Drawbar 1 3/5' |
| `4D` | Vibrato Depth | Drawbar 1 1/3' |
| `4E` | Vibrato Delay | Drawbar 1' |
| `4F` | – | Drawbar Organ Type |
| `54` | – | Organ 2nd Percussion |
| `55` | – | Organ 3rd Percussion |
| `56` | – | Organ Percussion Decay Time |
| `57` | – | Organ Key On Click |
| `58` | – | Organ Key Off Click |
| `59` | – | Vibrato Rate |
| `5A` | – | Vibrato Depth |

### 1.4 NRPN and RPN (p23–35)
Order: NRPN `Bn 62 LSB` then `Bn 63 MSB`; RPN `Bn 64 LSB` then `Bn 65 MSB`; value via Data Entry
`Bn 06 MSB` / `Bn 26 LSB`. `RPN 7F7F` = Null (deselect).

**RPN (MSB always 00H) — p33-35:**

| RPN (LSB) | Parameter | MSB range |
|---|---|---|
| `00` | Pitch Bend Sensitivity | `00–18H` (0–24 semitones) |
| `01` | Fine Tune | full |
| `02` | Coarse Tune | `28–58H` (−24..+24) |
| `7F` | Null | — |

**NRPN — "command" style (MSB=`22H`, `24H`, `25H`, `26H`, `27H`, `40H`) — p23-29.** These select
performances / sequencers / drawbar controls rather than sound params. Key ones:

| NRPN (LSB, MSB) | Function | Value (mm) |
|---|---|---|
| `00 22` | Part Enable (mixer part on/off) | Off/On |
| `01 22` | DSP Enable | Off/On |
| `00 24` | Performance Number Select | 00–63H; LSB 00=Preset,40=User |
| `00 25` | Step Sequencer Number Select | 00–63H; LSB 00=Preset,40=User |
| `01 25` | Step Seq Pattern Number | |
| `02 25` | Step Seq Start/Stop | |
| `00 26` | Phrase Seq Number Select | |
| `01 26` | Phrase Seq Start/Stop | |
| `00 27` | Arpeggio Number Select | |
| `ff 40` | Drawbar Position (ff = foot 00=16'…08=1') | see Drawbar value table §7.5 |
| `09 40` | Drawbar Key On Click | Off/On |
| `0A 40` | Drawbar 2nd Percussion | Off/On |
| `0B 40` | Drawbar 3rd Percussion | Off/On |
| `0C 40` | Percussion Decay Time | |
| `0D 40` | Drawbar Organ Type | Sine/Vintage |
| `0E 40` | Drawbar Key Off Click | Sine/Vintage table |

These command-NRPNs are gated by the *Setting Performance/StepSeq/Phrase/Arpeggio NRPN* enable
flags in the Spec area (§6.16). When enabled, bank/program number switching is ignored.

**NRPN — Solo Synth sound parameters (p29–32).** Each Solo Synth parameter is addressable per OSC
block. MSB selects block, LSB selects parameter:

- Blocks (MSB): **Synth1 OSC=`30H`, Synth2 OSC=`31H`, PCM1 OSC=`32H`, PCM2 OSC=`33H`,
  EXT OSC=`34H`, Noise OSC=`35H`, LFO1=`36H`, LFO2=`37H`, TOTAL(filter)=`38H`.**
- "Solo Synth Osc Edit" LSBs `00H–48H` cover onoff, split ui number, portamento, legato,
  pitch/filter/amp LFO depths 1&2, full pitch/filter/amp envelopes (init/attack/decay/sustain/
  release1/release2 + clock trigger + depth), key follow/base, filter cutoff/gain/touch, PWM
  (`3A/3C/3D` — Synth OSC only), and EXT-OSC-only params (`3F–48H`: original key, EG triggers,
  mic level, noise gate, pitch shifter mode/mix). Full LSB list is on p29-31 of the PDF.
- LFO block (`36H`/`37H`) LSBs `00–07H`: wave, sync, rate, depth, delay, rise, clock trigger,
  modulation depth (p32).
- TOTAL block (`38H`) LSBs `00–13H`: type, cutoff, resonance, touch, key follow/base, lfo depth
  1&2, full envelope, clock trigger, depth, eg retrigger (p32).

**NRPN — Tone parameters (p33):**
- *Tone Etc Edit* MSB=`3EH`: `00`=synth-all-osc amp env attack, `01`=…release;
  `10–15`=hex layer1-6 level (XW-P1), `16`=hex all-layer cutoff, `17`=hex detune,
  `18/19`=hex all-layer attack/release; `20/21/22`=pcm melody cutoff/attack/release.
- *Tone Common Edit* MSB=`3FH`: `00`=level, `01`=reverb send, `02`=chorus send.

### 1.5 Program Change & Bank Select (p12, p36–37)
- Program Change `Cn pp`. Selected tone = f(previously received Bank Select, this program #).
  Program change may also change the part's **Timbre Type**.
- **Timbre Types**: Melody, Piano, Drum, Drawbar(P1), Hex Layer(P1), Solo Synth, User Wave(G1).
  Drum ignores Hold1/Channel Coarse Tune/Master Coarse Tune; all others do sustain on/off.
- **Performance / Step-Seq switching via Bank Select MSB then Program Change (p37):**

| Bank Select MSB | Change target |
|---|---|
| `70H` | Preset Performance |
| `71H` | User Performance |
| `72H` | Preset Step Sequencer |
| `73H` | User Step Sequencer |

  (LSB ignored. Ignored entirely when Perform/S.Seq NRPN enable flags are on.)
- Tone bank/program → tone mapping itself is in the printed **Tone List** (not in this PDF).

### 1.6 Channel Aftertouch / Pitch Bend (p37–38)
- Aftertouch `Dn vv` → adds modulation (like CC01).
- Pitch Bend `En ll mm` → range set by part Bend Range.

### 1.7 System Realtime & Universal SysEx (p39–42)
- Timing Clock / Start / Stop / Active Sensing supported (p39).
- **Universal Realtime** `F0 7F dd …`: Master Volume `04 01 ll mm`, Master Pan `04 02 ll mm`,
  Master Fine Tune `04 03 ll mm`, Master Coarse Tune `04 04 ll mm` (mm 28–58H),
  Reverb Time `04 05 01 01 01 01 01 01 vv`, Chorus Rate `04 05 01 01 01 01 02 01 vv`,
  Send-To-Reverb `04 05 01 01 01 01 02 04 vv` (p40-41).
- **Universal Non-Realtime** `F0 7E dd 09 0x`: `01`=GM On, `02`=GM Off, `03`=GM2 On (treated as GM On) (p42).

---

## 2. Instrument-Specific System Exclusive — Frame Format (p43–51)

All parameter transfer uses one general frame. Fields present depend on the **action (`act`)**.

### 2.1 Full field order
```
SX  MAN  MOD    dev  act  cat  mem  pset  blk  prm  idx  len  data  img  crc  EOX
F0  44   16 03  dd   aa   cc   mm   LL MM ...  ...  ...  ...  ....  ...  ...  F7
```

### 2.2 Which fields each action carries (p44, table)
`Y` = present, left-to-right.

| act | SX | MAN | MOD | dev | act | cat | mem | pset | blk | prm | idx | len | data | img | crc | EOX |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| IPR | Y|Y|Y|Y|Y|Y|Y|Y|Y|Y|Y|Y|–|–|–|Y |
| IPS | Y|Y|Y|Y|Y|Y|Y|Y|Y|Y|Y|Y|Y|–|–|Y |
| OBR | Y|Y|Y|Y|Y|Y|Y|Y|–|–|–|–|–|–|–|Y |
| OBS | Y|Y|Y|Y|Y|Y|Y|Y|–|–|–|Y|–|Y|Y|Y |
| HBR | Y|Y|Y|Y|Y|Y|Y|Y|–|–|–|–|–|–|–|Y |
| HBS | Y|Y|Y|Y|Y|Y|Y|Y|–|–|–|Y|–|Y|Y|Y |
| EXI | Y|Y|Y|Y|Y|–|–|–|–|–|–|–|–|–|–|Y |
| SBS | Y|Y|Y|Y|Y|–|–|–|–|–|–|–|Y|–|–|Y |
| ACK | Y|Y|Y|Y|Y|Y|Y|Y|–|–|–|–|–|–|–|Y |
| RJC | Y|Y|Y|Y|Y|Y|Y|Y|–|–|–|–|–|–|–|Y |
| ESS | Y|Y|Y|Y|Y|Y|Y|Y|–|–|–|–|–|–|–|Y |
| EBS | Y|Y|Y|Y|Y|Y|Y|Y|–|–|–|–|–|–|–|Y |
| ERR | Y|Y|Y|Y|Y|–|–|–|–|–|–|–|Y|–|–|Y |

### 2.3 Field definitions (§16.3, p44–51)

- **SX** `F0H` — SysEx status.
- **MAN** `44H` — Casio.
- **MOD** `16H 03H` (MSB,LSB) — XW-P1/XW-G1 model ID.
- **dev** `0ddddddd` (`00–7FH`) — device ID; must match instrument's, or `7FH` = always accepted.
- **act** `0aaaaaaa` — action (table below).
- **cat** `0ccccccc` — category (§2.5).
- **mem** `0mmmmmmm` — memory area: `0`=User (R/W), `1`=Preset (read-only), `2`=Store (R/W).
- **pset** 2 bytes `LSB MSB` (each 7-bit) — parameter set number = `MSB<<7 | LSB`.
- **blk** — block number; up to 4 nested dimensions, each 14-bit (2×7-bit LSB/MSB), high dimension
  first. For an N-dim block you send index(N-1)…index0 pairs; unused higher dims are `0000H`.
  The **Block field** column in the parameter tables shows the bit layout (e.g. `2-0:Oscillator
  Number(0-5)` = bits 2..0 hold the oscillator index). (p47-48)
- **prm** 2 bytes `LSB MSB` (7-bit each) — parameter ID (the "ID" column in the parameter tables).
- **idx** 2 bytes `LSB MSB` — starting array index for the transfer.
- **len** 2 bytes `LSB MSB` —
  - *Individual*: size of the value in `data` (for array/string params = arrayLength − 1).
  - *Bulk*: number of `img` bytes in this packet (0 = no data).
- **data** — parameter value(s), see §2.4.
- **img** — bulk memory image, see §2.6.
- **crc** — CRC-32, see §2.7.
- **EOX** `F7H`.

### 2.4 `act` values (§16.3.5, p45)

| act | Name | Meaning |
|---|---|---|
| `00H` | **IPR** | Individual Parameter Request (→ device replies IPS) |
| `01H` | **IPS** | Individual Parameter Send (writes the value) |
| `02H` | **OBR** | One-way Bulk Parameter Set Request (→ OBS) |
| `03H` | **OBS** | One-way Bulk Parameter Set Send |
| `04H` | **HBR** | Handshake Bulk Parameter Set Request (→ HBS) |
| `05H` | **HBS** | Handshake Bulk Parameter Set Send |
| `08H` | **SBS** | Start of Bulk Dump Session |
| `09H` | **EXI** | Extend Interval (pause; resets wait timer) |
| `0AH` | **ACK** | Acknowledge |
| `0BH` | **RJC** | Reject / abandon session |
| `0DH` | **ESS** | End of Sub-session (one parameter set done) |
| `0EH` | **EBS** | End of Bulk Dump Session |
| `0FH` | **ERR** | Error (see data codes §2.8) |

### 2.5 `cat` categories (§16.3.6, p46–47)
"A" = available for that transfer type, "F" = file-info only (name/size, not data), "–" = N/A.

| cat | Parameter Set | Individual | One-way Bulk | Handshake Bulk |
|---|---|---|---|---|
| `00H` | System | A | – | – |
| `02H` | Patch | A | A | A |
| `03H` | Tone | A | A | A |
| `05H` | Melody | A | A | A |
| `06H` | Drum | A | A | A |
| `07H` | Drawbar (XW-P1 only) | A | A | A |
| `08H` | Hex Layer (XW-P1 only) | A | A | A |
| `09H` | Solo Synth | A | A | A |
| `0AH` | User Wave (XW-G1 only) | A | A | A |
| `13H` | DSP | A | A | A |
| `1FH` | All | F | A | A |
| `26H` | Step Sequencer | F | A | A |
| `27H` | Step Sequencer Chain | F | A | A |
| `28H` | Arpeggio | F | A | A |
| `29H` | Phrase | F | A | A |
| `2AH` | Spec | A | A | A |

### 2.6 Individual parameter `data` encoding (§16.3.13, p49–50)
Value is packed 7-bit-per-byte, **little-endian, LSB first**. Byte count from bit width (Size):

| Size (bits) | Data bytes |
|---|---|
| 1–7 | 1 |
| 8–14 | 2 |
| 15–21 | 3 |
| 22–28 | 4 |
| 29–32 | 5 |

For array parameters, `len+1` such value-blocks are concatenated. Each block's byte 0 holds bits
0–6, byte 1 bits 7–13, etc.; unused high bits of the last byte are 0.

**Single-message size limit (p50):** ≤256 bytes for handshake bulk, ≤48 bytes otherwise. A large
array can be split across IPS/IPR messages by adjusting `idx`+`len`. These limits are themselves
adjustable via the System Exclusive Protocol parameters (§6.2).

### 2.7 Bulk `img` encoding (§16.3.14, p51)
The raw parameter-set memory image (8-bit bytes) is re-packed into 7-bit bytes: read the source
bit-stream from bit 0 upward, emit 7 bits per MIDI byte. 33 source bytes → 38 transfer bytes.
Trailing unused bits are 0. A set may be split into multiple packets on any byte boundary, but must
be sent from the start in order and one packet may **not** contain more than one parameter set.

### 2.8 `crc` (§16.3.15, p51)
CRC-32 (ISO 8802-3 / IEEE 802.3) computed over the byte string **from MAN through the last `img`
byte**, stored as 5×7-bit little-endian bytes (`LSB … MSB`). Present only on OBS/HBS. Receiver
requests resend on mismatch.

### 2.9 SBS / ERR data codes (p49–50)
- **SBS data**: `0`=start OBR, `1`=start OBS, `2`=start HBR, `3`=start HBS session.
- **ERR data**: `0`=Time Out, `1`=Format Error, `2`=CRC Error.

---

## 3. Individual Parameter Operations (§17, p52)
Two ops: **Individual Parameter Send (IPS)** — writes value / issues command; **Individual
Parameter Request (IPR)** — device replies with IPS carrying the value / status. To read a single
parameter: send IPR with the target `cat/mem/pset/blk/prm/idx/len`; instrument returns IPS.

---

## 4. Bulk Dump / Parameter Set Transfer (§18, p52–62)

### 4.1 Two modes
- **One-way** (`OBR`/`OBS`): sender streams packets at fixed intervals, no per-packet ack. Best for
  sequencer-style dumps.
- **Handshake** (`HBR`/`HBS`): each packet is `ACK`-ed before the next; faster, with error recovery.

### 4.2 Session structure
- **Sub-session** = one parameter set (possibly split into multiple packets), terminated by `ESS`.
- **Session** = one user operation = one or more sub-sessions, terminated by `EBS`. A bulk dump is
  always a session, never a bare sub-session.

### 4.3 One-way flow (device → external, on request) (p53)
```
Ext → SBS(OBR)              start (data=0)
Ext → OBR                  request (start sub-session)
Dev → OBS  (×N, interval)  packets
Dev → ESS                  end sub-session
        … more sub-sessions …
Ext → EBS                  end session
```
One-way external → device (p54): `SBS(OBS)`, then `OBS ×N`, `ESS`, device replies `ACK`, `EBS`.

### 4.4 Handshake flow (device → external, on request) (p56)
```
Ext → SBS(HBR)     ; Dev → ACK
Ext → HBR          ; Dev → HBS  ; Ext → ACK  (repeat per packet)
Dev → ESS ; Ext → EBS
```
Handshake external → device (p57): `SBS(HBS)`→`ACK`, then per-packet `HBS`→`ACK`, `ESS`, `EBS`.

### 4.5 Error handling (p55–62)
Timeout / Format / CRC errors → `ERR(code)`, sender resends last packet. After *Handshake Retry
Number* consecutive failures → `RJC` terminates the session. `EXI` pauses a session (unlimited
extensions); resume with `ACK`, or abort with `RJC`. All intervals/retries are governed by the
System Exclusive Protocol parameters (§6.2).

### 4.6 Parameter-Set address table for bulk (`cat`/`mem`/`pset`) — **XW-P1** (§36.1, p85)
`mem` is `02H` (User area) for all bulk sets; `pset` numbers are 0-based User-area indices (they are
**not** the numbers shown on the panel).

| Set (cat) | mem | pset range | Contents |
|---|---|---|---|
| Patch `02H` | 02H | `0000–0063H` | User Patch 0–99 |
| Tone `03H` | 02H | `0000–0063H` | User Solo Synth 0–99 |
| | | `0064–0095H` | User Hex Layer 0–49 |
| | | `0096–00C7H` | User Drawbar 0–49 |
| | | `00C8–00DBH` | User Piano Melody 0–19 |
| | | `00DC–00EFH` | User Strings Melody 0–19 |
| | | `00F0–0103H` | User Guitar Melody 0–19 |
| | | `0104–0117H` | User Lead Melody 0–19 |
| | | `0118–0121H` | User Drum Melody 0–9 |
| | | `0122–0135H` | User Various Melody 0–19 |
| Melody `05H` | 02H | `0000–0013H` … `0050–0063H` | Piano/Strings/Guitar/Lead/Various Melody, 0–19 each |
| Drum `06H` | 02H | `0000–000AH` | User Drum Melody 0–9 |
| Drawbar `07H` | 02H | `0000–0032H` | User Drawbar 0–49 |
| Hex Layer `08H` | 02H | `0000–0032H` | User Hex Layer 0–49 |
| Solo Synth `09H` | 02H | `0000–0063H` | User Solo Synth 0–99 |
| DSP `13H` | 02H | `0000–0063H` | User DSP 0–99 |
| All `1FH` | 02H | `0000–0006H` | All Data |
| Step Sequencer `26H` | 02H | `0000–0063H` | User Step Seq 0–99 |
| Step Seq Chain `27H` | 02H | `0000–0063H` | User Step Seq Chain 0–99 |
| Arpeggio `28H` | 02H | `0000–0063H` | User Arpeggio 0–99 |
| Phrase `29H` | 02H | `0000–0063H` | User Phrase 0–99 |
| Spec `2AH` | 02H | `0000H` | Various global settings |

(XW-G1 variant differs — §36.2, p86 — not needed for this project.)

---

## 5. Parameter Address Map

**How to read (§19, p63):** each table has **Parameter | ID (hex) | R/W | Block | Size (bits) |
Array (hex) | Min-Def-Max (hex) | Description**. The **ID** is the `prm` field; **Block** shows how
the block-number bits are allocated (`↑` = same as the row above); **Size** = bit width (drives the
data-byte count, §2.6); **Array** = array length; Min/Def/Max are hex.

Values below are transcribed faithfully from the PDF. `↑` means "same Block as the row above."

### 5.1 System — category `00H` (§20, p63–65)

**System Information (§20.1, p63)**
| Parameter | ID | R/W | Block | Size | Array | Min-Def-Max | Description |
|---|---|---|---|---|---|---|---|
| Model Name | 0000 | R | 00000000 | 7 | 08 | 00-20-7F | ASCII; "XW-P1" / "XW-G1" |
| General Register | 000D | R/W | ↑ | 8 | 01 | 00-00-FF | general-purpose comms-test register |

**System Exclusive Protocol (§20.2, p64)** — controls bulk timing/size limits.
| Parameter | ID | R/W | Size | Min-Def-Max | Description |
|---|---|---|---|---|---|
| Oneway Min Interval | 000E | R | 14 | 0000-0014-3FFF | min ms between packets (one-way recv) |
| Oneway Max Interval | 000F | R/W | 14 | 0000-0800-3FFF | max wait ms (one-way recv) |
| Oneway Current Interval | 0010 | R/W | 14 | 0000-0014-3FFF | current ms between packets (one-way send) |
| Oneway Max Data Length | 0011 | R | 14 | 0000-0080-3FFF | max bytes/packet (one-way) |
| Oneway Current Data Length | 0012 | R/W | 14 | 0000-0080-3FFF | current bytes/packet (one-way send) |
| Handshake Max Interval | 0013 | R/W | 14 | 0000-0800-3FFF | max wait ms (handshake recv) |
| Handshake Max Data Length | 0014 | R | 14 | 0000-0080-3FFF | max bytes/packet (handshake) |
| Handshake Current Data Length | 0015 | R/W | 14 | 0000-0080-3FFF | current bytes/packet (handshake send) |
| Handshake Retry Number | 0016 | R/W | 7 | 00-03-7F | retries after error |

**Data Management (§20.3, p64–65)** — for the "Data Manager" PC app; IDs `0019H`–`0028H`
(Ps Category/Memory/Number, Ps Data Type, Current Ps Existence/Protect/Size, Current Sub Ps Size,
Current Ps Name[×10], Max Ps Size, Max Ps Number, Area Size, Available Size, Free Size, Delete Ps,
Bulksession Enabled). Full table p64-65. Mostly read-only status queries; likely not needed for tone editing.

### 5.2 Patch — category `02H` (§21, p66–68)
The Patch set = sound-source common settings + per-part mixer. Sub-blocks:

**Analog Input Tune (§21.1, p66)** IDs `0074–007C`: Part Enable, Line Select (0=SysChorus/1=DSP),
Level(00-64-7F), Pan(00-40-7F), Rev Send, Cho/Dsp Send, Noise Gate Threshold, Noise Gate Release,
Auto Level Control(0-3).

**Card Audio (§21.2, p66):** Level `0081` 7-bit 00-7F-7F.

**DSP Output (§21.3, p66)** IDs `007D–0080`: Part Enable, Level(00-64-7F), Pan(00-40-7F), Rev Send(00-20-7F).

**DSP Setup (§21.4, p66):** Disable `0082` (0=enable DSP,1=disable); Number `0083` 8-bit 00-00-C8
(0=Tone Dsp, 1-100=Preset Dsp, 101-200=User Dsp).

**Master EQ (§21.5, p66–67)** IDs `008D–0095`: Low/LowMid/HighMid/High Gain (each 8-bit 00-0C-18 =
−12..+12), Low Freq(0=200/1=400/2=800Hz), LowMid Freq(0-7: 1.0–5.0kHz), HighMid Freq(0-7 same),
High Freq(0=6/1=8/2=10kHz), On/Off `0095`.

**Master Tune (§21.6, p67):** Master Fine Tune `0000` 10-bit `0000-0200-03FF` (−100/512..+100/512
cent); Master Coarse Tune `0001` 7-bit `28-40-58` (−24..+24 semitone).

**Master Mixer (§21.7, p67):** Master Volume `0002` (00-7F-7F); Master Pan `0003` (00-40-7F,
−64..+63); Master Line Select `0004` (0=SysChorus/1=DSP).

**Part (§21.8, p67–68)** — Block `4-0:Part #` (part number in bits 4..0 → 16 parts). IDs:
| ID | Parameter | Size | Min-Def-Max | Notes |
|---|---|---|---|---|
| 0068 | Part Enable | 1 | 00-01-01 | Off/On |
| 0069 | Tone Num | 14 | 0000-0000-3FFF | 0–16383 |
| 006A | Fine Tune | 10 | 0000-0200-03FF | −100/512..+100/512 cent |
| 006B | Coarse Tune | 7 | 28-40-58 | −24..+24 semitone |
| 006C | Volume | 7 | 00-64-7F | 0–127 |
| 006E | Pan | 7 | 00-40-7F | −64..+63 |
| 006F | Cho Send | 7 | 00-00-7F | 0–127 |
| 0070 | Rev Send | 7 | 00-28-7F | 0–127 |
| 0071 | Bend Range | 7 | 00-02-18 | 0–24 |
| 0072 | Line Select | 1 | 00-00-01 | 0=SysChorus/1=DSP |

**System Chorus (§21.9, p68):** Level `008A`, Rate `008B`, SendToRev `008C`.
**System Reverb (§21.10, p68):** Type `0086` (0=Rectangle/1=Round), Level `0087`, Time `0088`.

**Patch Etc (§21.11, p68–69)** — performance-level settings. IDs `0096–00CC`. Highlights:
Performance Name[16] `0096`; Knob Assign Parameter `0098` (Block `2-0:Knob(0-3)`); RPN/NRPN MSB
`009A`, LSB `009B`, Data Entry MSB/LSB `009C`; Pedal Assign `009D` (0=Sustain/1=Soft/2=Sostenuto/
3=SS); Touch Curve `009E`; Touch Off Velocity `009F`; Tempo `00A0` (8-bit 1E-78-FF = 30-255);
StepSeq Number `00A1`; Solo1 Ch `00A2`; StepSeq Change Timing `00A3`; StepSeq Key Shift `00A4`;
StepSeq Pattern Number `00A5`; Arpeggio Key Shift/Range Low/Hi/Number `00A6-00A9`; Arpeggio Hold
`00AA`; Arpeggio StepSeq Sync `00AB`; Phrase Key Play/Number/Range `00AC-00AF`; Looper Number
`00B1`; Enable/Lowkey/Target/Index1-3 `00B2-00B7` (Block `4-0:TargetType`, `5-0:KeySetting`);
**Zone params** `00B8-00C7` (Block `2-0:Zone(0-3)`): Zone Enable, Key Range Low/Hi, Bend Range
Low/Hi, Octave Shift(3E-40-42), Transpose(34-40-4C), Knob1-4/Bender/Wheel/Pedal/Arpeggio/Phrase
Enable; **MIDI routing** `00C8-00CC` (Block `4-0:Part(0-15)`): MIDI Out Ch, MIDI In Ch(Off,0-15),
MIDI Generator Out, MIDI MIDI Out, MIDI USB Out. (Full table p68-69.)

### 5.3 Tone (common) — category `03H` (§22, p69)
| Parameter | ID | Size | Min-Def-Max | Description |
|---|---|---|---|---|
| Timbre Num | 0002 | 14 | 0000-0000-3FFF | 0–16383 |
| Line Select | 0004 | 1 | 00-00-01 | 0=SysChorus/1=DSP |
| Timbre Type | 0006 | 4 | 00-00-0F | 0=Melody,1=Piano,2=Drum,3=Drawbar(P1),4=Hex Layer(P1),5=Solo Synth,6=User Wave(G1) |
| Name | 0007 | 7 | 00-20-7F | ASCII ×10 |
| Level | 0008 | 7 | 00-7F-7F | 0–127 |

### 5.4 Melody — category `05H` (§23, p70)
IDs `0017–0020`: Attack Time, Release Time, Cutoff Freq (each 00-40-7F, −64..+63); Vibrato Type
(0=Sine/1=Tri/2=Saw/3=Square), Vibrato Depth/Speed/Delay (00-40-7F); Octave Shift (3E-40-42,
−2..+2); Volume (00-7F-7F); Touch Sense (00-7F-7F, −64..+63).

### 5.5 Drum — category `06H` (§24, p70–71)
**Instrument (×128) (§24.1)** Block `6-0:Inst`, IDs `0000-0006`: Assign Group(0-15), Rx Noteoff,
Volume(8-bit 00-80-FF, 0×–1.99×), Pan(−64..+63), Reverb Send.
**Velocity Split (×4 split ×128 inst) (§24.2)** Block `13-7:Inst / 2-0:Split`: Range Top `0007`
(velocity upper limit), Number `0008` (14-bit, inst 0-376).
**LFO (§24.3)** IDs `0009-0016`: Pitch/Amp LFO Wave Type(0-6: Sin/Tri/SawUp/SawDown/Pulse1:3/2:2/
3:1), Rate, Auto Delay, Auto Rise, Auto Depth(8-bit ±128), Mod Depth, After Depth.

### 5.6 Drawbar — category `07H`, **XW-P1 only** (§25, p71–72)
Block `3-0:Select Bar` for Position; rest `00000000`.
| Parameter | ID | Size | Min-Def-Max | Description |
|---|---|---|---|---|
| Position | 0000 | 4 | 00-00-08 | drawbar 0–8 (per selected bar) |
| Percussion | 0001 | 2 | 00-00-03 | 0=off,1=2nd,2=3rd,3=2nd+3rd |
| Percussion Decay Time | 0002 | 7 | 00-00-7F | 0–127 |
| Keyon Click | 0003 | 1 | 00-00-01 | off/on |
| Keyoff Click | 0004 | 1 | 00-00-01 | off/on |
| Type | 0005 | 1 | 00-00-01 | 0=Normal/1=Vintage |
| Vibrato Rate | 0006 | 7 | 00-00-7F | 0–127 |
| Vibrato Depth | 0007 | 7 | 00-00-7F | 0–127 |

### 5.7 Hex Layer — category `08H`, **XW-P1 only** (§26, p72–73)
**Hex Layer (×6 layer) (§26.1)** Block `2-0:Layer Number`. IDs `0000-0014`:
| ID | Parameter | Size | Min-Def-Max | Notes |
|---|---|---|---|---|
| 0000 | Onoff | 1 | 00-01-01 | off/on |
| 0002 | Split Ui Number | 16 | 0000-0000-FFFF | PCM Wave # 0326–1114 |
| 0003 | Pan Offset | 7 | 00-40-7F | −64..+63 |
| 0004 | Pitch Key | 7 | 00-40-7F | added to key #, 0x40 center |
| 0005 | Pitch Cent | 16 | 0000-0000-FFFF | sign+cent, 100/512-cent resolution (see PDF p72) |
| 0006 | Amp Attack Rate Offset | 8 | 00-80-FF | −128..127 |
| 0007 | Amp Decay Rate Offset | 8 | 00-80-FF | −128..127 |
| 0008 | Amp Sustain Level Offset | 8 | 00-80-FF | −128..127 |
| 0009 | Amp Release Rate Offset | 8 | 00-80-FF | −128..127 |
| 000A | Volume Offset | 8 | 00-80-FF | −128..127 |
| 000B | Cutoff Offset | 8 | 00-80-FF | −128..127 |
| 000C | Touch Sense Offset | 8 | 00-BF-FF | −128..127 |
| 000D | Reverb Send Offset | 8 | 00-80-FF | −128..127 |
| 000E | Chorus Send Offset | 8 | 00-80-FF | −128..127 |
| 000F | Key Range Low | 7 | 00-00-7F | 0–127 |
| 0010 | Key Range High | 7 | 00-7F-7F | 0–127 |
| 0011 | Velocity Range Low | 7 | 00-00-7F | 0–127 |
| 0012 | Velocity Range High | 7 | 00-7F-7F | 0–127 |
| 0013 | Detune Number | 5 | 00-00-1F | 0–31 (Block `00000000`) |
| 0014 | Pitch Lock | 1 | 00-00-01 | Array 03; 0=Unlocked/1=Locked |

**Hex Layer LFO (§26.2, p72–73)** IDs `0015-0022` (Block `00000000`): Pitch LFO Wave Type(0-6),
Pitch LFO Rate, Pitch Auto Delay/Rise/Depth(±128), Pitch Mod Depth, Pitch After Depth, Amp LFO Wave
Type, Amp LFO Rate, Amp Auto Delay/Rise/Depth, Amp Mod Depth, Amp After Depth. (Same shape as Drum LFO.)

### 5.8 Solo Synth — category `09H` (§27, p73–79) — **the main synth engine**
Nine blocks per tone: 6 OSC blocks (Synth1, Synth2, PCM1, PCM2, EXT, Noise), LFO1, LFO2, Total Filter.

**OSC Block Basic (×6 oscillator) (§27.1, p73)** Block `2-0:Oscillator Number(0-5)`, IDs `0000-0006`:
| ID | Parameter | Size | Min-Def-Max | Notes |
|---|---|---|---|---|
| 0000 | Onoff | 1 | 00-00-01 | off/on |
| 0003 | Wave Number | 16 | 0000-0000-FFFF | XW-P1: 0001-0311 Synth OSC1/2; 0312-0325 Noise; 0326-2483 PCM OSC1/2 |
| 0004 | Portamento Onoff | 1 | 00-00-01 | off/on (except Noise) |
| 0005 | Portamento Time | 7 | 00-00-7F | (except Noise) |
| 0006 | Legato Onoff | 1 | 00-00-01 | off/on |

**OSC Block Oscillator (×5) (§27.2, p73–74)** Block ↑, IDs `0008-0018`: LFO Depth(Array 02),
Pitch Key Cent(16-bit sign+semitone+cent — see p74 encoding), Detune(10-bit 0000-0200-03FF,
−256..255), Init Level, Attack Time/Level, Decay Time, Sustain Level, Release1 Time/Level,
Release2 Time/Level, Clock Trigger(0-18, off + beat divisions), Envelope Depth(±64),
Key Follow(8-bit ±128), Key Follow Base.

**OSC Block Filter (×5) (§27.3, p74)** IDs `0019-0029`: Cutoff(0-15), Gain(0=Flat/1=−3/2=−6/3=−12/
4=−18dB), Touch Sensitivity(±64), Key Follow(±128), Key Follow Base, LFO Depth(Array 02), full
envelope (Init/Attack/Decay/Sustain/Release1/Release2), Clock Trigger(0-18), Envelope Depth(±64).

**OSC Block Amp (×5) (§27.4, p75)** IDs `002A-0039`: Level, Touch Sensitivity(±64), Key Follow(±128),
Key Follow Base, LFO Depth(Array 02), Init Level, full envelope, Clock Trigger(0-18).

**Solo Synth Etc (§27.5, p76)** Block `00000000`, IDs `003D-0047`: Sync Osc(0=Async/1=Sync OSC2→OSC1),
Ext OSC Original Key, Pitch/Filter/Amp/Total-Filter Eg Trigger, Mic Inst Level, Ext Trigger
Threshold/Release, Pitch Shifter Mode(0-3), Pitch Shifter Mix(0-15).

**Solo Synth Controller (×8) (§27.6, p76–78)** Block `2-0:Controller Number`, IDs `0063-0066`:
Source, Destination Parameter, Destination Index, Depth(8-bit ±128). The Source & Destination
values are enumerated tables:
- **Source** (`0063`): `00`=Off; `01-62H`=CC# 0-61H; `63`=Note Key; `64`=Note Velocity;
  `65`=Aftertouch; `66`=Pitch Bend Up; `67`=Pitch Bend Down; `68`=Modulation; `69`=LFO1; `6A`=LFO2.
- **Destination Parameter** (`0064`, with matching Destination Index `0065`): `00-5E H` — a full
  routing map (OSC Portamento/Pitch/Detune/Key Follow/envelope stages, PWM, EXT params,
  Filter block, Amp block, Total Filter, LFO Rate/Depth/Delay/Rise/Mod, DSP Parameter1-8,
  and "OSC S1-P2" envelope group). The complete `00H`–`5EH` list with its Destination Index blocks
  is reproduced on **PDF p76-78** — transcribe directly from there for the codec's modulation matrix.

**Solo Synth LFO (×2) (§27.7, p78–79)** Block `0:Lfo Number`, IDs `005B-0062`: Wave(0-7: Sin/Tri/
SawUp/SawDown/Pulse1:3/2:2/3:1/Random), Clock Sync(0=NoSync/1=Sync LFO1[LFO2 only]/2=Sync Tempo),
Rate, Depth, Delay, Rise, Clock Trigger(0-17), Modulation Depth.

**Solo Synth PWM (§27.8, p79)** Block `0:Oscillator Number`: Pulse Width `003A` (0-127), LFO Depth
`003C` (Array 02, ±64).

**Solo Synth Total Filter (§27.9, p79)** Block `00000000`, IDs `0048-005A`: Filter Type(0=LPF/1=BPF/
2=HPF), Cutoff, Resonance, Touch Sensitivity(±64), Key Follow(±128), Key Follow Base, LFO Depth
(Array 02), full envelope (Init/Attack/Decay/Sustain/Release1/Release2), Clock Trigger(0-18),
Envelope Depth(±64), Eg Retrigger(off/on).

### 5.9 User Wave — category `0AH`, **XW-G1 only** (§28, p80–81) — *not on XW-P1*
Key Splits (×10), LFO, Looper. Documented p80-81; skip for this project.

### 5.10 DSP — category `13H` (§29, p81)
DSP Basic (Block `00000000` / `0:Button Selection`): Name `0000` (ASCII×10), Algorithm `0002`
(14-bit `0000-000A-3FFF`, ID → DSP Type per §7.1), Parameter7 `0003` (7-bit, **Array 08** = the 8
DSP knob params), Rotary Sw Onoff `0004`, Parameter Index `0005` (0=No Assign,1-8=Param), On Value
`0006`, Off Value `0007`. The meaning of each Parameter7[n] depends on the selected DSP type (§7).

### 5.11 Directory-info sets (name/size only — category `1F/26/27/28/29H`)
All (§30, p81), Step Sequencer (§31, p82), Step Seq Chain (§32, p82), Arpeggio (§33, p82),
Phrase (§34, p82): each exposes only **Name** (`0000`, ASCII×10) and **Size** (`0001`/`0002`,
32-bit). The actual sequence/arp/phrase payload is transferred as an opaque bulk `img`, not as
individual named parameters.

### 5.12 Spec — category `2AH` (§35, p82–84) — global/system settings
Block `00000000`, IDs include: Perform Number `0000`; Perform Filter `0001` (16-bit bitmask: bit0
Step Seq, bit1 Arpeggio, bit2 Phrase, bit3 Tempo, bit4 Sys Reverb, bit5 Sys Chorus, bit6 Master
EQ); Setting Start Up Select `0002`; Chain Number `0003`; Setting Fine Tune `0004` (10-bit
010B-0200-0303, −245..259 = 415.5–465.9Hz); Setting Coarse Tune `0005`; Panel Transpose `000C`
(−12..+12); Panel Octave Shift `000D` (−3..+3); Setting Local Control `000F`; Setting LCD Contrast
`0012` (1-17); Setting APO Mode `0013`; Setting MIDI Out Select `003B` (0=Keyboard/1=MIDI IN Thru/
2=USB); Setting USB Out Select `003C`; Setting MIDI In `003D`; Setting USB In `003E`; Setting Sync
Mode `003F` (0=Off/1=Master/2=Slave); **Setting Performance NRPN `0040`, StepSeq NRPN `0041`,
Phrase NRPN `0042`, Arpeggio NRPN `0043`** (the enable flags for the command-NRPNs in §1.4);
**Setting Device ID `0044`** (0-127, 127=All); **Setting Basic Ch `0045`** (0-15); Phrase Guide/
Precount/Beat/End Quantize/Note Quantize `0048-004C`; Looper Precount/Threshold/Reverse Rec/Channel/
Fs/Auto Overdub `004D-0052`. (Full table p82-84.)

---

## 6. DSP Type List & DSP Parameter Sets (§37–38, p87–101)

### 6.1 DSP Type IDs (§37, p87)
The `Algorithm` value in the DSP Basic set maps to these type IDs.

**Solo Synth DSP (§37.1):** `80H` Bypass, `81H` Auto Pan, `82H` Distortion, `83H` Flanger,
`84H` Chorus, `85H` Delay, `86H` Ring Modulator.

**Normal — Single (§37.2.1):** `01H` Wah, `02H` Compressor, `03H` Distortion, `04H` Enhancer,
`05H` Auto Pan, `06H` Tremolo, `07H` Phaser, `08H` Flanger, `09H` Chorus, `0AH` Delay,
`0BH` Reflection, `0CH` Rotary, `0DH` Ring Modulator, `0EH` Lo-Fi.

**Normal — Dual (§37.2.2):** `41H` Wah-Comp, `42H` Wah-Dist, `43H` Wah-Cho, `44H` Wah-Flan,
`45H` Wah-Ref, `46H` Wah-Trem, `47H` Wah-Pan, `48H` Comp-Wah, `49H` Comp-Dist, `4AH` Comp-Cho,
`4BH` Comp-Flan, `4CH` Comp-Ref, `4DH` Comp-Trem, `4EH` Comp-Pan, `50H` Dist-Wah, `51H` Dist-Comp,
`53H` Dist-Cho, `54H` Dist-Flan, `55H` Dist-Ref, `56H` Dist-Trem, `57H` Dist-Pan, `5DH` Cho-Ref,
`5FH` Cho-Pan, `65H` Flan-Ref, `67H` Flan-Pan, `6AH` Ref-Dist, `6BH` Ref-Cho, `6FH` Ref-Pan,
`72H` Trem-Dist, `73H` Trem-Cho, `74H` Trem-Flan, `75H` Trem-Ref.

> Note: the PDF's "Normal DSP Number" column has a couple of obvious typos (two rows numbered "20",
> two "30"); the **DSP ID** column is authoritative. (p87.)

### 6.2 DSP parameter sets (§38, p88–101)
For each DSP type, the up-to-8 `Parameter7[n]` knobs are named, all with value range `00–7F`, some
carrying a "setting value table" note (§7). Examples:
- **Solo Synth Auto Pan** (`81H`): [1]LFOWaveform [2]LFO Rate [3]LFO Depth [4]Manual.
- **Solo Synth Distortion** (`82H`): [1]Gain [2]Level.
- **Solo Synth Delay** (`85H`): [1]Delay Time [2]Feedback [3]Damp [4]Wet Level [5]Tempo Sync.
- **Wah** (`01H`): [1]Resonance [2]Manual [3]LFO Rate [4]LFO Depth [5]LFOWaveform.
- **Compressor** (`02H`): [1]Attack [2]Release [3]Level [4]Threshold.
- **Rotary** (`0CH`): [1]Od Gain [2]Od Level [3]Speed [4]Brake [5]Fall Accel [6]Rise Accel
  [7]Slow Rate [8]Fast Rate.
- **LoFi** (`0EH`): [1]W&F Rate [2]W&F Depth [3]Noise1Level [4]Noise2Level [5]Density [6]Bit.

Full per-type parameter lists (all 7 Solo Synth types, all 14 single + 32 dual Normal types) are on
**PDF p88-101**; transcribe directly per DSP type when building the effect editor.

---

## 7. Setting-Value / Send-Receive Tables (§39, p102–109)

Many parameters use a non-linear mapping between the transmitted/received MIDI value and the actual
setting. When a parameter's row cites a "Note → §39.x table", apply the corresponding conversion.
These matter for correct decode/encode.

| §39.x | Table | Encoding summary |
|---|---|---|
| 39.1 | Off/On | send 00/7F; recv 00-3F=Off, 40-7F=On |
| 39.2 | −64..0..+63 | linear, 40H = 0 |
| 39.3 | Pan | 00=Left, 40=Center, 7F=Right |
| 39.4 | Fine Tune | (LSB,MSB) pairs → 415.5–465.9 Hz (p102) |
| 39.5 | Drawbar Position | 9 steps 0-8 (00,14,28,32,3C,50,5A,6E,7F) |
| 39.6 | Sine/Vintage | 00=Sine, 7F=Vintage |
| 39.7 | Tempo Sync | Off,1/4,1/3,3/8,1/2,2/3,3/4,1,4/3,3/2,2 |
| 39.8 | 0-3 | 4 steps |
| 39.9 | 0-5 | 6 steps |
| 39.10 | Chorus Mode | mono/stereo/tri |
| 39.11 | Delay Level | 6 steps 0-5 |
| 39.12 | Delay Type | 2 modes |
| 39.13 | LFO Wave Form1 | off/sin/tri/random |
| 39.14 | LFO Wave Form2 | sin/tri/random |
| 39.15 | LFO Wave Form3 | sin/tri |
| 39.16 | LoFi Noise Level | 6 steps |
| 39.17 | Reflection | 8 steps 1-8 |
| 39.18 | Ring Type | 3 modes |
| 39.19 | Rotate/Brake | rotate/stop |
| 39.20 | Slow/Fast | slow/fast |
| 39.21 | −128..0..+127 | (LSB,MSB) 14-bit pairs, (00,40)=0 |
| 39.22 | −256..0..+255 | (LSB,MSB) 14-bit pairs, (00,40)=0 |
| 39.23 | Envelope Clock Trigger | off + 18 beat divisions (MSB ranges) |
| 39.24 | Filter Cutoff | 0-15 (MSB ranges) |
| 39.25 | Filter Gain | Flat/−3/−6/−12/−18dB |
| 39.26 | Synth Ext Osc Pitch Shifter Mode | off,1,2,3 |
| 39.27 | Synth Ext Osc Pitch Shifter Mix | 0-15 |
| 39.28 | Synth LFO Wave | Sin/Tri/SawUp/SawDown/Pulse1:3/2:2/3:1/Random |
| 39.29 | Synth LFO Sync | off/Tempo |
| 39.30 | Synth LFO Clock Sync | 18 beat divisions |
| 39.31 | Synth Total Filter Type | LPF/BPF/HPF |
| 39.32 | Hex Layer Detune | 0-31 |

Full numeric breakpoints for each table are on **PDF p102-109**. The `−128..+127` and `−256..+255`
tables (39.21/39.22) and the Fine Tune table (39.4) are 14-bit `(LSB,MSB)` maps — reproduce exactly
when round-tripping those parameters.

---

## 8. Coverage check vs. task requirements

| Required section | Status | Source |
|---|---|---|
| Global/channel MIDI (CC/NRPN/RPN, program/bank) | ✅ captured | §1 (p9-38) |
| SysEx frame (MAN 0x44, model, checksum, field layout) | ✅ captured | §2 (p43-51) |
| Solo Synth param map | ✅ captured | §5.8 (p73-79) + NRPN §1.4 (p29-32) |
| Hex Layer param map | ✅ captured | §5.7 (p72-73) |
| PCM Melody param map | ✅ captured (Melody = §5.4; PCM waves also via Solo Synth PCM OSC & Hex Layer) | §5.4 (p70) |
| Step Sequencer | ⚠️ directory-info only (name/size); payload is opaque bulk `img` | §5.11 (p82) |
| Arpeggio | ⚠️ directory-info only; payload opaque | §5.11 (p82) |
| DSP / Effects | ✅ captured (type IDs + per-type Parameter7 sets) | §6 (p87-101) |
| Performance / Mixer / System | ✅ captured (Patch/Part/Spec) | §5.2, §5.12 (p66-69, 82-84) |
| Bulk dump / data request | ✅ captured (protocol + cat/mem/pset table) | §4 (p52-62, 85) |

### Gaps Chunk 3 (JSON param map from the Lua) will need to fill
1. **Solo Synth Controller "Destination Parameter" enum (`00H-5EH`)** and the full **Source** enum —
   listed in §5.8 but the exhaustive `Destination Index` mapping should be transcribed from PDF
   p76-78 or cross-referenced with the Lua `g_XWSysEx`.
2. **Per-DSP-type Parameter7 name lists** — §6.2 summarizes; the complete 53-type breakdown
   (PDF p88-101) should be pulled verbatim (Lua likely has it structured).
3. **Setting-value table breakpoints** (§7 / PDF p102-109) — only summarized here; the exact
   MIDI↔value maps for decode/encode should be transcribed fully.
4. **Step Sequencer / Arpeggio / Phrase / Step-Seq-Chain payload formats** — the MIDI manual treats
   these as opaque bulk memory images (only Name+Size exposed). Their internal byte layout is **not
   in this PDF**; if the editor needs to parse/edit them, it must come from the Lua or reverse-eng.
5. **Tone Num / bank→tone mapping** — the numeric tone/bank/program → preset-name mapping lives in
   the separate printed **Tone List**, not this document.
6. **MOD-ID `02H` vs `03H` ambiguity** (see §0 note) — resolve against live hardware.
7. **Pitch Cent / Pitch Key Cent 16-bit sign+semitone+cent encodings** (Hex Layer `0005`, Solo Synth
   `0009`) — the bit-field layout is described in prose on PDF p72/p74; verify exact packing.
</content>
</invoke>
