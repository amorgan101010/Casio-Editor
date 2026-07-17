-- gen_golden.lua — authoritative golden-vector generator for the XW-P1 SysEx codec.
--
-- Runs franky's REAL reverse-engineered Lua (reference/lua/) with minimal stubbing:
--   * the ONLY stub is the MIDI sink `sendMidiMsg`, replaced to CAPTURE the assembled
--     hex string `sxmsg` instead of transmitting it.
--   * `bit` is the real luajit bitop library (require("bit")) — same semantics CTRLR used.
--
-- Everything load-bearing is the genuine article:
--   * value encoders  = g_xwModCalc["V2SX"]           (011_initTables.lua)
--   * 18-byte address = g_tssModSXtx[modid].sx        (createSXtssArray, 011_initTables.lua)
--   * frame builder   = sendXWSX                      (014_globalFunctions.lua)
--
-- The only glue we supply is the 3-line TX dispatch, copied verbatim from
-- sendTSSParamSX (019_ToneSoloSynth.lua). That function is a `local` wrapped in
-- UI-coupled code (panel:getComponent, canvas repaint) so it can't be loaded
-- cleanly; the bytes it produces are 100% from the real encoders/address/frame.
--
-- Run under luajit:   luajit tools/gen_golden.lua > tests/golden/xwp1_golden.json
--
-- Emits {version, generatedBy, rows:[{paramId, instance, value, expectedFrameHex}]}.

local bit = require("bit")            -- real luajit bitop, exposed as the global the encoders expect
_G.bit = bit

-- ---------------------------------------------------------------------------
-- Locate reference/lua relative to this script.
-- ---------------------------------------------------------------------------
local function scriptDir()
    local src = debug.getinfo(1, "S").source:sub(2)   -- strip leading '@'
    return src:match("^(.*[/\\])") or "./"
end
local REF = scriptDir() .. "../reference/lua/"

-- ---------------------------------------------------------------------------
-- Capture sink: sendXWSX -> sendMidiMsg(sxmsg). We override sendMidiMsg AFTER
-- loading globalFunctions so the real sendXWSX calls our capture.
-- ---------------------------------------------------------------------------
local CAPTURED = nil

dofile(REF .. "014_globalFunctions.lua")   -- defines real sendXWSX + sendMidiMsg
_G.sendMidiMsg = function(msg) CAPTURED = msg end   -- the ONE stub

dofile(REF .. "011_initTables.lua")        -- defines initTables()
initTables()                               -- builds g_xwModCalc, g_tsswf(=P1), g_tssModSXtx/rx

assert(g_tsswf and g_tsswf[3] == 326, "expected P1 wave-offset model (g_tsswf[3]==326)")
assert(g_tssModSXtx, "g_tssModSXtx not built")

-- ---------------------------------------------------------------------------
-- The genuine TX dispatch, verbatim from sendTSSParamSX (019_ToneSoloSynth.lua),
-- minus the ENV-canvas UI side effect. Returns the captured full-frame hex.
-- ---------------------------------------------------------------------------
local mcalc = g_xwModCalc["V2SX"]

local function driveTSS(modid, val)
    local mdef = g_tssModSXtx
    if not mdef[modid] then error("unknown modid: " .. modid) end

    local v1, v2, v3, n
    v1 = 0
    local sxid = mdef[modid].sx
    if mdef[modid].vt:match("wf") then n = tonumber(modid:sub(-1, -1), 10) or 1 end
    if mcalc[mdef[modid].vt]      then v1, v2, v3 = mcalc[mdef[modid].vt](val, n) end

    CAPTURED = nil
    sendXWSX(1, sxid, v1, v2, v3)          -- act=1 (set)
    return CAPTURED
end

-- ---------------------------------------------------------------------------
-- Self-check the bit path on the two nastiest pk edges before trusting the grid.
-- ---------------------------------------------------------------------------
local function frameValueBytes(hex)
    -- strip 'f0 44 16 03 7f 01' + 18 addr bytes = 24 leading, drop trailing 'f7'
    local toks = {}
    for t in hex:gmatch("%x%x") do toks[#toks + 1] = t end
    local vals = {}
    for i = 25, #toks - 1 do vals[#vals + 1] = toks[i] end
    return table.concat(vals, " ")
end
assert(frameValueBytes(driveTSS("tssOSCPoset-1", -256)) == "00 20 7f",
       "pk(-256) self-check failed: " .. frameValueBytes(driveTSS("tssOSCPoset-1", -256)))
assert(frameValueBytes(driveTSS("tssOSCPoset-1", 255)) == "50 5f 00",
       "pk(255) self-check failed: " .. frameValueBytes(driveTSS("tssOSCPoset-1", 255)))

-- ---------------------------------------------------------------------------
-- The (param, instances, values) grid. Covers every solo-synth vt, edge values,
-- all 6 wf oscillators, and the multi-instance PWM/LFO addressing.
-- ---------------------------------------------------------------------------
local grid = {
    -- nf  (1-byte fader) ------------------------------------------------------
    { p = "tssOSCsw",     inst = {1,2,3,4,5,6}, vals = {0, 1} },          -- toggle, all 6 osc
    { p = "tssOSCPortaTm", inst = {1, 6},        vals = {0, 63, 127} },    -- full 0..127 fader
    { p = "tssFLTFcoff",  inst = {1},            vals = {0, 64, 127} },    -- count=1 block
    { p = "tssOSC2sync",  inst = {1},            vals = {0, 1} },          -- Etc count=1 block
    { p = "tssLFOrate",   inst = {1, 2},         vals = {0, 64, 127} },    -- LFO count=2 addressing
    { p = "tssLFOmdep",   inst = {1, 2},         vals = {0, 127} },
    -- cf  (1-byte centered fader -64..+63) -----------------------------------
    { p = "tssOSCPENViL", inst = {1, 6},         vals = {-64, 0, 63} },
    { p = "tssOSCPWMlfo1D", inst = {1, 2},       vals = {-64, 0, 63} },    -- PWM count=2, ai=0
    { p = "tssOSCPWMlfo2D", inst = {1, 2},       vals = {-64, 0, 63} },    -- PWM count=2, ai=1
    -- cF  (2-byte centered fader -128..+128, LSB-first) ----------------------
    { p = "tssOSCPkeyf",  inst = {1, 6},         vals = {-128, -64, 0, 64, 128} },
    { p = "tssFLTFkeyf",  inst = {1},            vals = {-128, 0, 128} },
    -- tn  (2-byte tune/detune x2, -256..+255) --------------------------------
    { p = "tssOSCPdtne",  inst = {1, 3, 6},      vals = {-256, -1, 0, 1, 255} },
    -- pk  (3-byte pitch-key, signed 0x4000, -256..+255) ----------------------
    { p = "tssOSCPoset",  inst = {1, 3, 6},      vals = {-256, -16, -1, 0, 16, 255} },
    -- wf  (3-byte waveform, per-osc base offset) — ALL 6 oscillators ----------
    { p = "tssOSCwf",     inst = {1}, vals = {0, 1, 310} },      -- osc1 syn, base 1,  max 310
    { p = "tssOSCwf",     inst = {2}, vals = {0, 155, 310} },    -- osc2 syn, base 1
    { p = "tssOSCwf",     inst = {3}, vals = {0, 1000, 2157} },  -- osc3 pcm, base 326
    { p = "tssOSCwf",     inst = {4}, vals = {0, 500, 2157} },   -- osc4 pcm, base 326
    { p = "tssOSCwf",     inst = {5}, vals = {0} },              -- osc5 ext, base 0,  max 0
    { p = "tssOSCwf",     inst = {6}, vals = {0, 7, 13} },       -- osc6 noise, base 312, max 13
    -- Collision pairs (Lua ai typo): identical frames -> RX-ambiguous ---------
    { p = "tssOSCAlfo1D", inst = {1}, vals = {-64, 0, 63} },
    { p = "tssOSCAlfo2D", inst = {1}, vals = {-64, 0, 63} },
    { p = "tssFLTFlfo1D", inst = {1}, vals = {-64, 0, 63} },
    { p = "tssFLTFlfo2D", inst = {1}, vals = {-64, 0, 63} },
}

-- ---------------------------------------------------------------------------
-- Emit JSON.
-- ---------------------------------------------------------------------------
local function normHex(hex)
    local out = {}
    for t in hex:gmatch("%S+") do out[#out + 1] = t:lower() end
    return table.concat(out, " ")
end

local rows = {}
for _, g in ipairs(grid) do
    for _, inst in ipairs(g.inst) do
        for _, v in ipairs(g.vals) do
            local modid = g.p .. "-" .. inst
            local frame = normHex(driveTSS(modid, v))
            rows[#rows + 1] = string.format(
                '  { "paramId": %q, "instance": %d, "value": %d, "expectedFrameHex": %q }',
                g.p, inst, v, frame)
        end
    end
end

io.write('{\n')
io.write('  "version": 1,\n')
io.write('  "generatedBy": "tools/gen_golden.lua (real franky Lua: V2SX encoders + createSXtssArray addresses + sendXWSX frame; luajit bitop; sendMidiMsg stubbed to capture)",\n')
io.write('  "model": "XW-P1",\n')
io.write('  "note": "instance is 1-based (Lua modid suffix -1..-N; block byte = instance-1). expectedFrameHex is the FULL wire frame f0..f7, value bytes LSB-first.",\n')
io.write('  "rows": [\n')
io.write(table.concat(rows, ",\n"))
io.write('\n  ]\n}\n')

io.stderr:write(string.format("gen_golden: %d rows emitted\n", #rows))
