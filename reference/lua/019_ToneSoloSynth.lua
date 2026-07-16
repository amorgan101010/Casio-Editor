-- CTRLR method: ToneSoloSynth
-- extracted from CasioXW-EDITOR-v0101.bpanelz (author: franky)

--
-- ToneSoloSynth: handler for Solo Synth layer
--

local setModNameVal			= setModNameVal
local setModNameMVal		= setModNameMVal
local getModNameIntProperty	= getModNameIntProperty
local sendCC 				= sendCC
local sendNRNP				= sendNRNP
local sendXWSX				= sendXWSX

--
-- Set/Init TSS params:
--
local function resetTSSParam(mid, osc)
	local osc		= tonumber(mid:gsub(".*-",""),10) or 1
	local oscgrp	= mid:sub(1,7)
	local oscmid, oscmid2, oscmval

	-- set:
	if mid:match("ENVcopy") then
		for m,av in pairs(g_tssModMidi["tssOSC"]) do
			if m:match(oscgrp) then
				oscmid		= m..'-'..(osc-1)
				oscmid2		= m..'-'..osc
				oscmval		= getModNameVal(oscmid) or 0
				setModNameMVal(oscmid2, oscmval, true)
			end
		end
	end

	-- init:
	if mid:match("ENVinit") then
		local oscgrp	= mid:sub(1,7)
		local oscmid, oscmval

		-- init
		for m,av in pairs( tableConcat(g_tssModMidi["tssOSC"], g_tssModMidi["tssFLT"]) ) do
			if m:match(oscgrp) then
				oscmid		= m..'-'..osc
				oscmval 	= getModNameIntProperty(oscmid,"uiSliderDoubleClickValue") or 0
				setModNameMVal(oscmid, oscmval, true)
			end
		end
	end
end


--# NRPN ====================================================================================================
--
-- send Midi values, print graphs
--[[
local function sendTSSParam(mid, val, zch)
	local mtp	= mid:gsub("-.*","")						-- modul. type (root)
	local osc	= tonumber(mid:gsub(".*-",""),10) or 1		-- suffix (-1, -2...)

	local ENVCanvas
	local nmsb, nlsb, vmsb=0, vlsb
	local mdef		= {}
	local mcalc		= g_xwModCalc

	if 		mtp:match("tssOSC") then mdef	= g_tssModMidi["tssOSC"]		-- OSC
	elseif 	mtp:match("tssLFO") then mdef	= g_tssModMidi["tssLFO"]		-- lfo
	elseif 	mtp:match("tssFLT") then mdef	= g_tssModMidi["tssFLT"]		-- total filter
	elseif 	mtp:match("tssDSP") then mdef	= g_tssModMidi["tssDSP"]		-- DSP config
	elseif 	mtp:match("tssCOM") then mdef	= g_tssModMidi["tssCOM"]		-- Common
	end

	if not mdef or not mdef[mtp] then do return end ; end

	nmsb	= mdef["tssMSBid"][osc]
	nlsb	= mdef[mtp].id

	-- calculate casio midi values
	if 		mcalc[mdef[mtp].vt] 		then vmsb,vlsb = mcalc[mdef[mtp].vt](val)
	elseif 	mdef[mtp].vt:match("nf-")	then vmsb,vlsb = mcalc['nfx'](val, mdef[mtp].vt:sub(4))
	end

	-- send Midi:
	if mdef["tssMIDI"]	== 'cc' 	then sendCC(zch,nlsb,vmsb)					end
	if mdef["tssMIDI"]	== 'nrnp' 	then sendNRNP(zch,nmsb,nlsb,vmsb, vlsb)		end

	-- draw ADSRR graph:
	if mtp:match('ENV') then
		local gvar	= 	mtp:gsub('.*ENV','')					-- eg 'r1T'
		local gmod	=	mtp:gsub(gvar,'')..'canv-'..osc			-- eg tssOSCPENVcanv-1

		g_CANVdata[gmod][gvar] = vmsb							-- assign slider value to global data
		ENVCanvas = panel:getComponent(gmod) ; if ENVCanvas then ENVCanvas:repaint() end
	end
end
--]]


--# SYSEX ====================================================================================================
--
-- send Midi values, print graphs
--
local function sendTSSParamSX(mid, val)
	local v1,v2,v3,n
	local mdef		= {}
	local mcalc		= g_xwModCalc["V2SX"]
	local sxid

	-- init/reset buttons
	if mid:match("ENVcopy") or mid:match("ENVinit") then resetTSSParam(mid, osc) ; return ; end

	-- treat modulators
	mdef	= g_tssModSXtx

	if not mdef or not mdef[mid] then do return end ; end
	v1		= 0

	-- set sysex fragment:
	sxid	= mdef[mid].sx

	-- calculate casio midi values
	if 		mdef[mid].vt:match("wf")	then n	= tonumber(mid:sub(-1,-1),10) or 1	end 			-- special case osc waveforms, osc suffix (-1, -2...)
	if 		mcalc[mdef[mid].vt] 		then v1,v2,v3 = mcalc[mdef[mid].vt](val,n) end				-- mmsb,msb,lsb

	-- send Midi:
	sendXWSX(1, sxid, v1, v2, v3)

	-- draw ADSRR graph:
	if mid:match('ENV') then
		local ENVCanvas
		local gvar	= 	mid:gsub('.*ENV',''):gsub("-.*","")		-- eg 'r1T'
		local gmod	=	mid:gsub(gvar,'canv')					-- eg 'tssOSCPENVcanv-1'
		g_CANVdata[gmod][gvar] = val							-- assign slider value to global data
		ENVCanvas = panel:getComponent(gmod) ; if ENVCanvas then ENVCanvas:repaint() end
	end
end


--# SYSEX ====================================================================================================


--
-- MAIN method
--
ToneSoloSynth = function(mod, value)
	if not isPanelReady() 	then do return end ; end
	if not value 			then do return end ; end

	local thisMod	= L(mod:getName())

	if 		thisMod:match("^tss") then 			sendTSSParamSX(thisMod, value)				-- TSS is always zone 1
	end
end

