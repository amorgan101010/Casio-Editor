-- CTRLR method: initTables
-- extracted from CasioXW-EDITOR-v0101.bpanelz (author: franky)



function initTables()
--
-- value tables for ADSSR graphs
--
g_CANVdataInit	= {
["tssOSCPENVcanv"] = {iL=0,  aT=127, aL=0,   dT=127, sL=0,   r1T=127, r1L=0,  r2T=127, r2L=0},
["tssOSCFENVcanv"] = {iL=0,  aT=127, aL=0,   dT=127, sL=0,   r1T=127, r1L=0,  r2T=127, r2L=0},
["tssOSCAENVcanv"] = {iL=0,  aT=0,   aL=127, dT=127, sL=127, r1T=30,  r1L=00, r2T=127, r2L=0 },
["tssFLTFENVcanv"] = {iL=0,  aT=127, aL=0,   dT=127, sL=0,   r1T=127, r1L=0,  r2T=127, r2L=0},
}
g_CANVdata	= {}
for o=1,6 do
for c,v in pairs(g_CANVdataInit) do g_CANVdata[c..'-'..o]={} for f,v in pairs(g_CANVdataInit[c]) do g_CANVdata[c..'-'..o][f]=v  end end
end


--
-- TSS waveform list numbers:
--
g_XWTssWf	 = {P1={}, G1={}}
g_XWTssWf.P1 = { [1] = 1, [2] = 1, [3] = 326, [4] = 326, [5] = 0, [6] = 312 }
g_XWTssWf.G1 = { [1] = 1, [2] = 1, [3] = 781, [4] = 781, [5] = 0, [6] = 767 }
g_tsswf		 = g_XWTssWf.P1


--
-- general casio 'strange midi calculating' functions
--
g_xwModCalc= {}

g_xwModCalc["V2SX"]	= {			-- (sign),msb,lsb (order will be reversed in sendSX)
nf 		= function(v) 	return v     end,														-- normal fader 0-127
cf 		= function(v) 	return v+64  end,														-- centered fader -64 +63
nF		= function(v) 	return bit.rshift(v,7), bit.band(v,0x7f) end,							-- double byte fader
cF		= function(v) 	v=v+128 ; return bit.rshift(v,7),bit.band(v,0x7f) end,					-- -128..+128, msb,lsb	// sysex: lsb, msb, 40 == 04 
-- Casio specials:
sw 		= function(v) 	return v*127 end,														-- switch 0/127
nfx		= function(v,n) return math.floor(v*126/n) end,											-- formula for '< 127' fader
db		= function(v) 	return (8-v) end,														-- drawbars (inverted)
wf		= function(v,o)	v=v+g_tsswf[o];	return 0,bit.rshift(v,7),bit.band(v,0x7f) end,			-- wave-form (3by, starts a 1)
tn		= function(v) 	v=2*(v+256); 	return bit.rshift(v,7), bit.band(v,0x7f) end,			-- tune/detune : -256..+256 x 2 (!)
pk		= function(v) 	local sgn=0;local sx=0x30*v;if v<0 then sgn=0x7f;sx=0x4000+sx;end; return sgn,bit.rshift(sx,7),bit.band(sx,0x7f) end,	-- pitch key: -256..+256 * 0x30 "signed"
dt		= function(v) 	v=v+0x80; return bit.rshift(v,7), bit.band(v,0x7f) end,					-- tss DSP type
}

g_xwModCalc["SX2v"]	= {			-- (sign),msb,lsb (order will be reversed in sendSX)
nf 		= function(v) 		 return v     end,													-- normal fader 0-127
cf 		= function(v) 		 return v-64  end,													-- centered fader -64 +63
nF		= function(v1,v2) 	 return v1 + bit.lshift(v2) end,									-- double byte fader
cF		= function(v1,v2) 	 return v1 + bit.lshift(v2,7) - 128 end,							-- -128..+128, msb,lsb
-- Casio specials:
sw 		= function(v) 	 	 return v%127 end,													-- switch 0/127
nfx		= function(v,n) 	 return math.floor(v*n/127) end,	
db		= function(v)     	 return (v-8) end,													-- drawbars (inverted)
wf		= function(v1,v2,v3,o) return (v1 + bit.lshift(v2,7)) - g_tsswf[o] end,					-- wave-form (3by, starts a 1)
tn		= function(v1,v2)    return (v1 + bit.lshift(v2,7))/2 - 256 end,						-- tune/detune : -256..+256 x 2 (!)
pk		= function(v1,v2,v3) if v3==0 then return (v1+bit.lshift(v2,7))/0x30 else return (v1+bit.lshift(v2,7)-0x4000)/0x30 end end, 	-- pitch key: -256..+256 * 0x30 "signed"
dt		= function(v1,v2)	 return v1 + bit.lshift(v2,7) - 0x80 end,							-- tss DSP type
}

g_xwModCalc["nrpn"]	= {
nf 		= function(v) return v     end,															-- normal fader 0-127
cf 		= function(v) return v+64  end,															-- centered fader -64 +63
sw 		= function(v) return v*127 end,															-- switch 0/127
nF		= function(v) return bit.rshift(v,7), bit.band(v,127) end,								-- double byte fader
db		= function(v) return (8-v)*15 end,														-- drawbars (inverted)
cf256	= function(v) return bit.rshift(v+128,1), bit.lshift(bit.band(v+128,1),6) end,			-- -128..+128  msb, lsb
cf512	= function(v) return bit.rshift(v+256,2), bit.lshift(bit.band(v+256,3),5) end,			-- -256..+256  msb, lsb
nfx		= function(v,n) return math.floor(v*126/n) end,											-- formula for '< 127' fader
}


--#=====================================================================================================================

local tssModSX		= {}

tssModSX["tssOSC"] = {
sx					= {ct=0x09, bn=6},							-- cat, block-no
blk 				= function(b) return "00 00 00 00 00 00 0"..b.." 00" end,	-- block
tssOSCsw			= {id=0x00, ai=0 , an=0, vt='nf'},			-- osc on/off
tssOSCwf			= {id=0x03, ai=0 , an=0, vt='wf'},			-- syn wave number / 'split ui number'
tssOSCPortaSw		= {id=0x04, ai=0 , an=0, vt='nf'},
tssOSCPortaTm		= {id=0x05, ai=0 , an=0, vt='nf'},
tssOSCLegatoSw 		= {id=0x06, ai=0 , an=0, vt='nf'},
-- pitch
tssOSCPENViL		= {id=0x0b, ai=0 , an=0, vt='nf'},			-- ENV
tssOSCPlfo1D		= {id=0x08, ai=0 , an=0, vt='cf'},
tssOSCPlfo2D 		= {id=0x08, ai=1 , an=0, vt='cf'},
tssOSCPoset			= {id=0x09, ai=0 , an=0, vt='pk'},			-- pitch key:	: -256 - 0 - 255  in steps of  ca '10'
tssOSCPdtne			= {id=0x0a, ai=0 , an=0, vt='tn'},			-- detune	: nrnp: -256 - 0 - 255
tssOSCPENViL		= {id=0x0b, ai=0 , an=0, vt='cf'},			-- ENV
tssOSCPENVaT		= {id=0x0c, ai=0 , an=0, vt='nf'},
tssOSCPENVaL		= {id=0x0d, ai=0 , an=0, vt='cf'},
tssOSCPENVdT		= {id=0x0e, ai=0 , an=0, vt='nf'},
tssOSCPENVsL		= {id=0x0f, ai=0 , an=0, vt='cf'},
tssOSCPENVr1T		= {id=0x10, ai=0 , an=0, vt='nf'},
tssOSCPENVr1L		= {id=0x11, ai=0 , an=0, vt='cf'},
tssOSCPENVr2T		= {id=0x12, ai=0 , an=0, vt='nf'},
tssOSCPENVr2L		= {id=0x13, ai=0 , an=0, vt='cf'},
tssOSCPEclk			= {id=0x14, ai=0 , an=0, vt='nf'},			-- Clock Trigger
tssOSCPEdep			= {id=0x15, ai=0 , an=0, vt='cf'},			-- ENV depth
tssOSCPkeyf			= {id=0x17, ai=0 , an=0, vt='cF'},		-- key follow, 2byte
tssOSCPkeyfB		= {id=0x18, ai=0 , an=0, vt='nf'},
-- filter
tssOSCFcoff			= {id=0x19, ai=0 , an=0, vt='nf'},			-- cutoff
tssOSCFgain			= {id=0x1a, ai=0 , an=0, vt='nf'},			-- gain
tssOSCFtch			= {id=0x1b, ai=0 , an=0, vt='cf'},			-- touch sens
tssOSCFkeyf			= {id=0x1c, ai=0 , an=0, vt='cF'},
tssOSCFkeyfB		= {id=0x1d, ai=0 , an=0, vt='nf'},
tssOSCFlfo1D		= {id=0x1e, ai=0 , an=0, vt='cf'},
tssOSCFlfo2D 		= {id=0x1e, ai=1 , an=0, vt='cf'},
tssOSCFENViL		= {id=0x1f, ai=0 , an=0, vt='nf'},			-- ENV
tssOSCFENVaT		= {id=0x20, ai=0 , an=0, vt='nf'},
tssOSCFENVaL		= {id=0x21, ai=0 , an=0, vt='nf'},
tssOSCFENVdT		= {id=0x22, ai=0 , an=0, vt='nf'},
tssOSCFENVsL		= {id=0x23, ai=0 , an=0, vt='nf'},
tssOSCFENVr1T		= {id=0x24, ai=0 , an=0, vt='nf'},
tssOSCFENVr1L		= {id=0x25, ai=0 , an=0, vt='nf'},
tssOSCFENVr2T		= {id=0x26, ai=0 , an=0, vt='nf'},
tssOSCFENVr2L		= {id=0x27, ai=0 , an=0, vt='nf'},
tssOSCFEclk			= {id=0x28, ai=0 , an=0, vt='nf'},
tssOSCFEdep			= {id=0x29, ai=0 , an=0, vt='cf'},			-- Env depth
-- AMP
tssOSCAlvl			= {id=0x2a, ai=0 , an=0, vt='nf'},			-- level
tssOSCAtch			= {id=0x2c, ai=0 , an=0, vt='cf'},			-- touch sens
tssOSCAkeyf			= {id=0x2d, ai=0 , an=0, vt='cF'},
tssOSCAkeyfB		= {id=0x2e, ai=0 , an=0, vt='nf'},
tssOSCAlfo1D		= {id=0x2f, ai=0 , an=0, vt='cf'},
tssOSCAlfo2D 		= {id=0x2f, ai=0 , an=0, vt='cf'},
tssOSCAENViL		= {id=0x30, ai=0 , an=0, vt='nf'},			-- ENV
tssOSCAENVaT		= {id=0x31, ai=0 , an=0, vt='nf'},
tssOSCAENVaL		= {id=0x32, ai=0 , an=0, vt='nf'},
tssOSCAENVdT		= {id=0x33, ai=0 , an=0, vt='nf'},
tssOSCAENVsL		= {id=0x34, ai=0 , an=0, vt='nf'},
tssOSCAENVr1T		= {id=0x35, ai=0 , an=0, vt='nf'},
tssOSCAENVr1L		= {id=0x36, ai=0 , an=0, vt='nf'},
tssOSCAENVr2T		= {id=0x37, ai=0 , an=0, vt='nf'},
tssOSCAENVr2L		= {id=0x38, ai=0 , an=0, vt='nf'},
tssOSCAEclk			= {id=0x39, ai=0 , an=0, vt='nf'},
}

tssModSX["tssPWM"] = {
sx					= {ct=0x09, bn=2},							-- cat, block-no
blk 				= function(b) return "00 00 00 00 00 00 0"..b.." 00" end,	-- block
tssOSCPWMpw			= {id=0x3a, ai=0 , an=0, vt='nf'},			-- PWM
tssOSCPWMlfo1D		= {id=0x3c, ai=0 , an=0, vt='cf'},
tssOSCPWMlfo2D		= {id=0x3c, ai=1 , an=0, vt='cf'},
}

tssModSX["tssETC"] = {
sx					= {ct=0x09, bn=1},							-- cat, block-no
blk 				= function(b) return "00 00 00 00 00 00 00 00" end,	-- block
tssOSC2sync			= {id=0x3d, ai=0 , an=0, vt='nf'},			-- Osc Sync (only syn2)
tssOSCXokey			= {id=0x3e, ai=0 , an=0, vt='nf'},			-- EXT P
tssOSCXPxtrg		= {id=0x3f, ai=0 , an=0, vt='nf'},			--     trig P
tssOSCXFxtrg		= {id=0x40, ai=0 , an=0, vt='nf'},			--     trig F
tssOSCXAxtrg		= {id=0x41, ai=0 , an=0, vt='nf'},			--     trig A
tssOSCXTFxtrg		= {id=0x42, ai=0 , an=0, vt='nf'},			--     EXT trig Tot. Filter => placed in block 'Total Filter'
tssOSCXinlvl		= {id=0x43, ai=0 , an=0, vt='nf'},			--     mic instr level P
tssOSCXngth			= {id=0x44, ai=0 , an=0, vt='nf'},			--     trig. thresh P
tssOSCXngrel		= {id=0x45, ai=0 , an=0, vt='nf'},			--     trig. rel. P
tssOSCXPshmode		= {id=0x46, ai=0 , an=0, vt='nf'},			--     P
tssOSCXPshmix		= {id=0x47, ai=0 , an=0, vt='nf'},			--     P
}

tssModSX["tssFLT"] = {
sx					= {ct=0x09, bn=1},							-- cat, block-no
blk 				= function(b) return "00 00 00 00 00 00 00 00" end,	-- block
tssFLTFtype			= {id=0x48, ai=0 , an=0, vt='nf'},
tssFLTFcoff			= {id=0x49, ai=0 , an=0, vt='nf'},			-- cutoff
tssFLTFreso			= {id=0x4a, ai=0 , an=0, vt='nf'},			-- release
tssFLTFtch			= {id=0x4b, ai=0 , an=0, vt='cf'},			-- touch sens
tssFLTFkeyf			= {id=0x4c, ai=0 , an=0, vt='cF'},
tssFLTFkeyfB		= {id=0x4d, ai=0 , an=0, vt='nf'},
tssFLTFlfo1D		= {id=0x4e, ai=0 , an=0, vt='cf'},
tssFLTFlfo2D 		= {id=0x4e, ai=0 , an=0, vt='cf'},
tssFLTFENViL		= {id=0x4f, ai=0 , an=0, vt='nf'},			-- ENV
tssFLTFENVaT		= {id=0x50, ai=0 , an=0, vt='nf'},
tssFLTFENVaL		= {id=0x51, ai=0 , an=0, vt='nf'},
tssFLTFENVdT		= {id=0x52, ai=0 , an=0, vt='nf'},
tssFLTFENVsL		= {id=0x53, ai=0 , an=0, vt='nf'},
tssFLTFENVr1T		= {id=0x54, ai=0 , an=0, vt='nf'},
tssFLTFENVr1L		= {id=0x55, ai=0 , an=0, vt='nf'},
tssFLTFENVr2T		= {id=0x56, ai=0 , an=0, vt='nf'},
tssFLTFENVr2L		= {id=0x57, ai=0 , an=0, vt='nf'},
tssFLTFEclk			= {id=0x58, ai=0 , an=0, vt='nf'},
tssFLTFEdep			= {id=0x59, ai=0 , an=0, vt='cf'},
tssFLTFErtrg		= {id=0x5a, ai=0 , an=0, vt='nf'},
}

tssModSX["tssLFO"] = {
sx					= {ct=0x09, bn=2},								-- cat, block-no
blk 				= function(b) return "00 00 00 00 00 00 0"..b.." 00" end,	-- block
tssLFOwf			= {id=0x5b, ai=0 , an=0, vt='nf'},
tssLFOsync			= {id=0x5c, ai=0 , an=0, vt='nf'},
tssLFOrate			= {id=0x5d, ai=0 , an=0, vt='nf'},
tssLFOdep			= {id=0x5e, ai=0 , an=0, vt='nf'},
tssLFOdelay			= {id=0x5f, ai=0 , an=0, vt='nf'},
tssLFOrise			= {id=0x60, ai=0 , an=0, vt='nf'},
tssLFOclk			= {id=0x61, ai=0 , an=0, vt='nf'},
tssLFOmdep			= {id=0x62, ai=0 , an=0, vt='nf'},
}

tssModSX["tssDSP"] = {
sx					= {ct=0x13, bn=0},							-- cat, block-no
blk 				= function(b) return "00 00 00 00 00 00 00 00" end,
tssDSPTab			= {id=0x02, ai=0 , an=0, vt='dt'},
tssDSPPANwf			= {id=0x03, ai=0 , an=0, vt='sw'},			-- tss Pan
tssDSPPANrate		= {id=0x03, ai=1 , an=0, vt='nf'},
tssDSPPANdep		= {id=0x03, ai=2 , an=0, vt='nf'},
tssDSPPANman		= {id=0x03, ai=3 , an=0, vt='cf'},
tssDSPDSTgain		= {id=0x03, ai=0 , an=0, vt='nf'},			-- tss Distortion
tssDSPDSTlvl		= {id=0x03, ai=1 , an=0, vt='nf'},
tssDSPFLGwf			= {id=0x03, ai=0 , an=0, vt='nf-2'},		-- tss Flanger
tssDSPFLGrate		= {id=0x03, ai=1 , an=0, vt='nf'},
tssDSPFLGdep		= {id=0x03, ai=2 , an=0, vt='nf'},
tssDSPCHOwf			= {id=0x03, ai=0 , an=0, vt='sw'},			-- tss Chorus
tssDSPCHOrate		= {id=0x03, ai=1 , an=0, vt='nf'},
tssDSPCHOdep		= {id=0x03, ai=2 , an=0, vt='nf'},
tssDSPDELtime		= {id=0x03, ai=0 , an=0, vt='nf'},			-- tss Delay
tssDSPDELfb			= {id=0x03, ai=1 , an=0, vt='nf'},
tssDSPDELdamp		= {id=0x03, ai=2 , an=0, vt='nf-3'},
tssDSPDELwet		= {id=0x03, ai=3 , an=0, vt='nf-5'},
tssDSPDELsync		= {id=0x03, ai=4 , an=0, vt='nf-10'},
tssDSPRMDfreq		= {id=0x03, ai=0 , an=0, vt='nf'},			-- tss RingMod
tssDSPRMDdry		= {id=0x03, ai=1 , an=0, vt='nf'},
tssDSPRMDwet		= {id=0x03, ai=2 , an=0, vt='nf'},
}



--
-- generated complete 'modulator-sysex-value' assignment-tables for send and receive: 
--
g_tssModSXrx = {}
g_tssModSXtx = {}


local function createSXtssArray(cat)
	local sysex, _a
	_a	= tssModSX[cat]

	for m, _m in pairs(_a) do
		if  m:match("tss") then
			if	_a.sx.bn == 0 then
				--        f0 44 16 03 7f 00 cat  me pset. bk prm.... idx..   arr.... f7
	 			sysex	=    string.format("%.2x 00 00 00 %s %.2x 00 %.2x 00 %.2x 00", _a.sx.ct, _a.blk(0), _m.id, _m.ai, _m.an )		-- ggf. umstellen auf blk(b), b=0...a.sx.bn
				g_tssModSXrx[sysex] 	= { id=m	,     vt=_m.vt }			-- <sysex> = mod-id, value
				g_tssModSXtx[m]			= { sx=sysex,     vt=_m.vt } 			-- <modid> = sysex, value
			else
			for b=1,_a.sx.bn do
				--        f0 44 16 03 7f 00 cat  me pset. bk prm.... idx..   arr.... f7
	 			sysex	=    string.format("%.2x 00 00 00 %s %.2x 00 %.2x 00 %.2x 00", _a.sx.ct, _a.blk(b-1), _m.id, _m.ai, _m.an )		-- ggf. umstellen auf blk(b), b=0...a.sx.bn
				g_tssModSXrx[sysex] 	= { id=m..'-'..b, vt=_m.vt }			-- <sysex> = mod-id, value
				g_tssModSXtx[m..'-'..b]	= { sx=sysex,     vt=_m.vt } 			-- <modid> = sysex, value
			end
			end
		end
	end
end


createSXtssArray("tssOSC")
createSXtssArray("tssPWM")
createSXtssArray("tssETC")
createSXtssArray("tssFLT")
createSXtssArray("tssLFO")
-- createSXtssArray("tssDSP")


--#==============================================================================================================


local dspModSX	= {}

dspModSX["tssDSP"] = {
sx					= {ct=0x13, bn=0},									-- cat, block-no
blk 				= function(b) return "00 00 00 00 00 00 00 00" end,	-- block
tssDSPTab			= {tid=0x80, id=0x02, ai=0 , an=0, vt='dt'},		-- tss Type
--tssDSPoff			= {tid=0x80, id=0x03, ai=0 , an=0, vt='sw'},		-- tss off pseudo-modulator
tssDSPPANwf			= {tid=0x81, id=0x03, ai=0 , an=0, vt='sw'},		-- tss Pan
tssDSPPANrate		= {tid=0x81, id=0x03, ai=1 , an=0, vt='nf'},
tssDSPPANdep		= {tid=0x81, id=0x03, ai=2 , an=0, vt='nf'},
tssDSPPANman		= {tid=0x81, id=0x03, ai=3 , an=0, vt='cf'},
tssDSPDSTgain		= {tid=0x82, id=0x03, ai=0 , an=0, vt='nf'},		-- tss Distortion
tssDSPDSTlvl		= {tid=0x82, id=0x03, ai=1 , an=0, vt='nf'},
tssDSPFLGwf			= {tid=0x83, id=0x03, ai=0 , an=0, vt='nf-2'},		-- tss Flanger
tssDSPFLGrate		= {tid=0x83, id=0x03, ai=1 , an=0, vt='nf'},
tssDSPFLGdep		= {tid=0x83, id=0x03, ai=2 , an=0, vt='nf'},
tssDSPCHOwf			= {tid=0x84, id=0x03, ai=0 , an=0, vt='sw'},		-- tss Chorus
tssDSPCHOrate		= {tid=0x84, id=0x03, ai=1 , an=0, vt='nf'},
tssDSPCHOdep		= {tid=0x84, id=0x03, ai=2 , an=0, vt='nf'},
tssDSPDELtime		= {tid=0x85, id=0x03, ai=0 , an=0, vt='nf'},		-- tss Delay
tssDSPDELfb			= {tid=0x85, id=0x03, ai=1 , an=0, vt='nf'},
tssDSPDELdamp		= {tid=0x85, id=0x03, ai=2 , an=0, vt='nf-3'},
tssDSPDELwet		= {tid=0x85, id=0x03, ai=3 , an=0, vt='nf-5'},
tssDSPDELsync		= {tid=0x85, id=0x03, ai=4 , an=0, vt='nf-10'},
tssDSPRMDfreq		= {tid=0x86, id=0x03, ai=0 , an=0, vt='nf'},		-- tss RingMod
tssDSPRMDdry		= {tid=0x86, id=0x03, ai=1 , an=0, vt='nf'},
tssDSPRMDwet		= {tid=0x86, id=0x03, ai=2 , an=0, vt='nf'},
}


--
-- generated complete 'modulator-sysex-value' assignment-tables for send and receive: 
--
g_dspModSXrx = {}
g_dspModSXtx = {}

local function createSXdspArray(cat)
	local sysex, _a
	_a	= dspModSX[cat]

	for m, _m in pairs(_a) do
		if  m:match("tss") then
			--        f0 44 16 03 7f 00 cat  me pset. bk prm.... idx..   arr.... f7
			sysex	=    string.format("%.2x 00 00 00 %s %.2x 00 %.2x 00 %.2x 00", _a.sx.ct, _a.blk(0), _m.id, _m.ai, _m.an)
			g_dspModSXrx[string.format("%s-%.2x",sysex,_m.tid)] 	= { id=m	,     vt=_m.vt }			-- <sysex> = mod-id, value
			g_dspModSXtx[m]											= { sx=sysex,     vt=_m.vt } 			-- <modid> = sysex, value
		end
	end	
end

createSXdspArray("tssDSP")




--#=====================================================================================================================
--
-- NRPN:
--
g_tssModMidi		= {}

g_tssModMidi["tssOSC"] = {
tssMIDI 			= 'nrnp',						-- type
tssMSBid 			= {0x30, 0x31, 0x32, 0x33, 0x34, 0x35},			-- msb
-- general			     lsb	value-type
tssOSCsw			= {id=0x00, vt='sw'},
tssOSCwf			= {id=0x01, vt='nF'},			-- syn wave number / 'split ui number'
tssOSCPortaSw		= {id=0x02, vt='sw'},
tssOSCPortaTm		= {id=0x03, vt='nf'},
tssOSCLegatoSw 		= {id=0x04, vt='sw'},
-- pitch
tssOSCPlfo1D		= {id=0x05, vt='cf'},
tssOSCPlfo2D 		= {id=0x06, vt='cf'},
tssOSCPoset			= {id=0x07, vt='cf512'},		-- pitch	: nrnp: -256 - 0 - 255 
tssOSCPdtne			= {id=0x08, vt='cf512'},		-- detune	: nrnp: -256 - 0 - 255
tssOSCPENViL		= {id=0x09, vt='cf'},			-- ENV
tssOSCPENVaT		= {id=0x0a, vt='nf'},
tssOSCPENVaL		= {id=0x0b, vt='cf'},
tssOSCPENVdT		= {id=0x0c, vt='nf'},
tssOSCPENVsL		= {id=0x0d, vt='cf'},
tssOSCPENVr1T		= {id=0x0e, vt='nf'},
tssOSCPENVr1L		= {id=0x0f, vt='cf'},
tssOSCPENVr2T		= {id=0x10, vt='nf'},
tssOSCPENVr2L		= {id=0x11, vt='cf'},
tssOSCPEclk			= {id=0x12, vt='nf-18'},		-- Clock Trigger
tssOSCPEdep			= {id=0x13, vt='cf'},			-- ENV depth
tssOSCPkeyf			= {id=0x15, vt='cf256'},		-- key follow
tssOSCPkeyfB		= {id=0x16, vt='nf'},
-- filter
tssOSCFcoff			= {id=0x17, vt='nf-15'},		-- cutoff
tssOSCFgain			= {id=0x18, vt='nf-4'},			-- gain
tssOSCFtch			= {id=0x19, vt='cf'},			-- touch sens
tssOSCFkeyf			= {id=0x1a, vt='cf256'},
tssOSCFkeyfB		= {id=0x1b, vt='nf'},
tssOSCFlfo1D		= {id=0x1c, vt='cf'},
tssOSCFlfo2D 		= {id=0x1d, vt='cf'},
tssOSCFENViL		= {id=0x1e, vt='nf'},			-- ENV
tssOSCFENVaT		= {id=0x1f, vt='nf'},
tssOSCFENVaL		= {id=0x20, vt='nf'},
tssOSCFENVdT		= {id=0x21, vt='nf'},
tssOSCFENVsL		= {id=0x22, vt='nf'},
tssOSCFENVr1T		= {id=0x23, vt='nf'},
tssOSCFENVr1L		= {id=0x24, vt='nf'},
tssOSCFENVr2T		= {id=0x25, vt='nf'},
tssOSCFENVr2L		= {id=0x26, vt='nf'},
tssOSCFEclk			= {id=0x27, vt='nf-18'},
tssOSCFEdep			= {id=0x28, vt='cf'},			-- Env depth
-- AMP
tssOSCAlvl			= {id=0x29, vt='nf'},			-- level
tssOSCAtch			= {id=0x2b, vt='cf'},			-- touch sens
tssOSCAkeyf			= {id=0x2c, vt='cf256'},
tssOSCAkeyfB		= {id=0x2d, vt='nf'},
tssOSCAlfo1D		= {id=0x2e, vt='cf'},
tssOSCAlfo2D 		= {id=0x2f, vt='cf'},
tssOSCAENViL		= {id=0x30, vt='nf'},			-- ENV
tssOSCAENVaT		= {id=0x31, vt='nf'},
tssOSCAENVaL		= {id=0x32, vt='nf'},
tssOSCAENVdT		= {id=0x33, vt='nf'},
tssOSCAENVsL		= {id=0x34, vt='nf'},
tssOSCAENVr1T		= {id=0x35, vt='nf'},
tssOSCAENVr1L		= {id=0x36, vt='nf'},
tssOSCAENVr2T		= {id=0x37, vt='nf'},
tssOSCAENVr2L		= {id=0x38, vt='nf'},
tssOSCAEclk			= {id=0x39, vt='nf-18'},
-- div
tssOSCPWMpw			= {id=0x3a, vt='nf'},			-- PWM
tssOSCPWMlfo1D		= {id=0x3c, vt='cf'},
tssOSCPWMlfo2D		= {id=0x3d, vt='cf'},
tssOSC2sync			= {id=0x3e, vt='sw'},			-- Osc Sync (only syn2)
tssOSCXokey			= {id=0x3f, vt='nf'},			-- EXT P
tssOSCXPxtrg		= {id=0x40, vt='sw'},			--     trig P
tssOSCXFxtrg		= {id=0x41, vt='sw'},			--     trig F
tssOSCXAxtrg		= {id=0x42, vt='sw'},			--     trig A
tssOSCXTFxtrg		= {id=0x43, vt='sw'},			--     EXT trig Tot. Filter => placed in block 'Total Filter'
tssOSCXinlvl		= {id=0x44, vt='nf'},			--     mic instr level P
tssOSCXngth			= {id=0x45, vt='nf'},			--     trig. thresh P
tssOSCXngrel		= {id=0x46, vt='nf'},			--     trig. rel. P
tssOSCXPshmode		= {id=0x47, vt='nf-3'},			--     P
tssOSCXPshmix		= {id=0x48, vt='nf-15'},		--     P
}
g_tssModMidi["tssLFO"] = {
tssMIDI 			= 'nrnp',						-- type
tssMSBid 			= {0x36, 0x37},					-- msb
tssLFOwf			= {id=0x00, vt='nf-7'},
tssLFOsync			= {id=0x01, vt='sw'},
tssLFOrate			= {id=0x02, vt='nf'},
tssLFOdep			= {id=0x03, vt='nf'},
tssLFOdelay			= {id=0x04, vt='nf'},
tssLFOrise			= {id=0x05, vt='nf'},
tssLFOclk			= {id=0x06, vt='nf-17'},
tssLFOmdep			= {id=0x07, vt='nf'},
}
g_tssModMidi["tssFLT"] = {
tssMIDI 			= 'nrnp',						-- type
tssMSBid 			= {0x38},						-- msb
tssFLTFtype			= {id=0x00, vt='nf-2'},
tssFLTFcoff			= {id=0x01, vt='nf'},			-- cutoff
tssFLTFreso			= {id=0x02, vt='nf'},			-- release
tssFLTFtch			= {id=0x03, vt='cf'},			-- touch sens
tssFLTFkeyf			= {id=0x04, vt='cf256'},
tssFLTFkeyfB		= {id=0x05, vt='nf'},
tssFLTFlfo1D		= {id=0x06, vt='cf'},
tssFLTFlfo2D 		= {id=0x07, vt='cf'},
tssFLTFENViL		= {id=0x08, vt='nf'},			-- ENV
tssFLTFENVaT		= {id=0x09, vt='nf'},
tssFLTFENVaL		= {id=0x0a, vt='nf'},
tssFLTFENVdT		= {id=0x0b, vt='nf'},
tssFLTFENVsL		= {id=0x0c, vt='nf'},
tssFLTFENVr1T		= {id=0x0d, vt='nf'},
tssFLTFENVr1L		= {id=0x0e, vt='nf'},
tssFLTFENVr2T		= {id=0x0f, vt='nf'},
tssFLTFENVr2L		= {id=0x10, vt='nf'},
tssFLTFEclk			= {id=0x11, vt='nf-18'},
tssFLTFEdep			= {id=0x12, vt='cf'},
tssFLTFErtrg		= {id=0x13, vt='sw'},
}
g_tssModMidi["tssDSP"] = {
tssMIDI 			= 'cc',							-- type
tssMSBid 			= {0x00},						-- not used
tssDSPPANwf			= {id=0x10, vt='sw'},			-- Pan
tssDSPPANrate		= {id=0x11, vt='nf'},
tssDSPPANdep		= {id=0x12, vt='nf'},
tssDSPPANman		= {id=0x13, vt='cf'},
tssDSPDSTgain		= {id=0x10, vt='nf'},			-- Distortion
tssDSPDSTlvl		= {id=0x11, vt='nf'},
tssDSPFLGwf			= {id=0x10, vt='nf-2'},			-- Flanger
tssDSPFLGrate		= {id=0x11, vt='nf'},
tssDSPFLGdep		= {id=0x12, vt='nf'},
tssDSPCHOwf			= {id=0x10, vt='sw'},			-- Chorus
tssDSPCHOrate		= {id=0x11, vt='nf'},
tssDSPCHOdep		= {id=0x12, vt='nf'},
tssDSPDELtime		= {id=0x10, vt='nf'},			-- Chorus
tssDSPDELfb			= {id=0x11, vt='nf'},
tssDSPDELdamp		= {id=0x12, vt='nf-3'},
tssDSPDELwet		= {id=0x13, vt='nf-5'},
tssDSPDELsync		= {id=0x50, vt='nf-10'},
tssDSPRMDfreq		= {id=0x10, vt='nf'},			-- RingMod
tssDSPRMDdry		= {id=0x11, vt='nf'},
tssDSPRMDwet		= {id=0x12, vt='nf'},
}
g_tssModMidi["tssCOM"] = {
tssMIDI 			= 'cc',							-- type
tssMSBid 			= {0x00},						-- not used
tssCOMvol			= {id=0x07, vt='nf'},
tssCOMrevb			= {id=0x5b, vt='nf'},
}



--
-- MIXER
--
g_mixModMidi = {}

g_mixModMidi['mixHEX'] = {
mixMIDI 			= 'nrnp',						-- type
mixMSBid 			= {0x3e},						-- msb
mixHEX1lvl			= {id=0x10, vt='cf256'},		-- level layer 1
mixHEX2lvl			= {id=0x11, vt='cf256'},
mixHEX3lvl			= {id=0x12, vt='cf256'},
mixHEX4lvl			= {id=0x13, vt='cf256'},
mixHEX5lvl			= {id=0x14, vt='cf256'},
mixHEX6lvl			= {id=0x15, vt='cf256'},
mixHEXAcoff			= {id=0x16, vt='cf256'},
mixHEXAdtne			= {id=0x17, vt='nf-32'},
mixHEXAatk			= {id=0x18, vt='cf256'},
mixHEXArel			= {id=0x19, vt='cf256'},
}
g_mixModMidi["mixDSP"] = {
mixMIDI 			= 'nrnp',						-- type
mixMSBid 			= {0x22},						-- msb
mixPARTsw			= {id=0x00, vt='sw'},			-- DSP switch
mixDSPsw			= {id=0x01, vt='sw'},			-- DSP switch
}



--
-- ORGAN
--
g_orgModMidi = {}

g_orgModMidi["orgTW"] = {
orgMIDI 			= 'nrnp',						-- type
orgMSBid 			= {0x40},						-- msb
orgTWdbar16			= {id=0x00, vt='db'},
orgTWdbar513		= {id=0x01, vt='db'},
orgTWdbar8			= {id=0x02, vt='db'},
orgTWdbar4			= {id=0x03, vt='db'},
orgTWdbar223		= {id=0x04, vt='db'},
orgTWdbar2			= {id=0x05, vt='db'},
orgTWdbar135		= {id=0x06, vt='db'},
orgTWdbar113		= {id=0x07, vt='db'},
orgTWdbar1			= {id=0x08, vt='db'},	
orgTWclckon			= {id=0x09, vt='sw'},
orgTWperc2			= {id=0x0a, vt='sw'},
orgTWperc3			= {id=0x0b, vt='sw'},
orgTWpercdec		= {id=0x0c, vt='nf'},
orgTWtype			= {id=0x0d, vt='sw'},
orgTWclckoff		= {id=0x0e, vt='sw'},
}
g_orgModMidi["orgVC"]  = {
orgMIDI 			= 'cc',							-- type
orgMSBid 			= {0x00},						-- not used
orgVCrate			= {id=0x59, vt='nf'},
orgVCdepth			= {id=0x5a, vt='nf'},
}
g_orgModMidi["orgROT"]  = {
orgMIDI 			= 'cc',							-- type
orgMSBid 			= {0x00},						-- not used
orgROTODgain		= {id=0x10, vt='nf-4'},			-- Gain
orgROTODlvl			= {id=0x11, vt='nf'},
orgROTspd			= {id=0x12, vt='sw'},			-- slow fast
orgROTbrk			= {id=0x13, vt='sw'},			-- brake
orgROTfacc			= {id=0x50, vt='nf'},
orgROTracc			= {id=0x51, vt='nf'},
orgROTsrate			= {id=0x52, vt='nf'},
orgROTfrate			= {id=0x53, vt='nf'},
}
g_orgModMidi["orgGEN"]  = {
orgMIDI 			= 'cc',							-- type
orgMSBid 			= {0x00},						-- not used
orgGENexpr			= {id=0x0b, vt='nf'},
orgGENrevb			= {id=0x5b, vt='nf'},
}


--
-- PCM
--
g_mixDrumKits = {
{ name="StandardSet1", prg=0, msb=120 }, 
{ name="StandardSet2", prg=1, msb=120 }, 
{ name="StandardSet3", prg=2, msb=120 }, 
{ name="StandardSet4", prg=3, msb=120 }, 
{ name="Room Set", prg=8, msb=120 }, 
{ name="Hip-Hop Set", prg=9, msb=120 }, 
{ name="Rock Set", prg=17, msb=120 }, 
{ name="Elec.Set", prg=24, msb=120 }, 
{ name="Synth Set 1", prg=25, msb=120 }, 
{ name="Synth Set 2", prg=30, msb=120 }, 
{ name="Trance Set", prg=31, msb=120 }, 
{ name="Dance Set 1", prg=29, msb=120 }, 
{ name="Dance Set 2", prg=28, msb=120 }, 
{ name="Dance Set 3", prg=27, msb=120 }, 
{ name="Jazz Set", prg=32, msb=120 }, 
{ name="Brush Set", prg=40, msb=120 }, 
{ name="OrchestraSet", prg=48, msb=120 }, 
{ name="Ethnic Set 1", prg=49, msb=120 }, 
{ name="Ethnic Set 2", prg=50, msb=120 }, 
}


end
