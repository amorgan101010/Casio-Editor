-- CTRLR method: midiReceive
-- extracted from CasioXW-EDITOR-v0101.bpanelz (author: franky)

--
-- midiReceive: handler for midi receive
--

local setModNameVal			= setModNameVal
local getModNameVal			= getModNameVal
local setModNameIntProperty	= setModNameIntProperty
local getModNameIntProperty	= getModNameIntProperty


--
-- receive and process messages for TSS
--
local function rxTSS(midi)
	local sysex	= {sx, mid, db, mval, vt, _f, osc}
	local v		= {}

	sysex.sx	= midi.msg:getData():getRange(6,18):toHexString(1)

	if g_tssModSXrx[sysex.sx] then 
		sysex.mid		= g_tssModSXrx[sysex.sx].id
		sysex.vt		= g_tssModSXrx[sysex.sx].vt 
		sysex.db		= midi.size-25												-- number of data bytes
		sysex._f		= g_xwModCalc["SX2v"][sysex.vt]

		-- calculate modulator values:
		for b=1,sysex.db do 	v[b] 	= midi.msg:getData():getByte(23+b) end						-- calculate modulator value
		if sysex.mid:match("tssOSCwf") then	v[4] = tonumber(sysex.mid:sub(-1,-1),10) or 1 end 		-- special case oscillator waveforms

		sysex.mval	= sysex._f(v[1],v[2],v[3],v[4])

		-- set modulator:
		setModNameVal(sysex.mid, sysex.mval)

		-- draw ADSRR graph:
		if sysex.mid:match('tss.*ENV') then
			local ENVCanvas
			local gvar	= 	sysex.mid:gsub('.*ENV',''):gsub("-.*","")		-- eg 'r1T'
			local gmod	=	sysex.mid:gsub(gvar,'canv')						-- eg 'tssOSCPENVcanv-1'
			g_CANVdata[gmod][gvar] = sysex.mval								-- assign slider value to global data
			ENVCanvas = panel:getComponent(gmod) ; if ENVCanvas then ENVCanvas:repaint() end
		end
	end
end

--
-- receive and process messages for DSP
--
local function rxDSP(midi)
	local sysex	= {sxr, sx, tid, mid, db, mval, vt, _f}
	local v		= {}
	local mcalc	= g_xwModCalc["SX2v"]

	sysex.sxr	= midi.msg:getData():getRange(6,18):toHexString(1)
	sysex.tid 	= getModNameIntProperty("tssDSPTab", "uiTabsCurrentTab") + 0x80

	for i,t in ipairs({0x80, sysex.tid}) do 
		sysex.sx	= string.format("%s-%.2x",sysex.sxr,t)

		if g_dspModSXrx[sysex.sx] then
			sysex.mid		= g_dspModSXrx[sysex.sx].id
			sysex.vt		= g_dspModSXrx[sysex.sx].vt
			sysex.db		= midi.size-25												-- number of data bytes

			for b=1,sysex.db do 	v[b] 	= midi.msg:getData():getByte(23+b) end		-- calculate modulator value

			if 		mcalc[sysex.vt] 		then sysex._f	= mcalc[sysex.vt]
			elseif 	sysex.vt:match("nf-")	then sysex._f	= mcalc['nfx'] ; v[2] = sysex.vt:sub(4)
			end
			sysex.mval	= sysex._f(v[1],v[2],v[3])

			-- set DSP Tab:
			if  sysex.mid 	== "tssDSPTab" then
				setModNameIntProperty("tssDSPTab", "uiTabsCurrentTab", sysex.mval)
			else
				setModNameVal(sysex.mid, sysex.mval)
			end
		end
	end
end




--[[
#### BASE FUNCTIONS 'PROCESS MIDI SIGNAL'  #################################################################################
--]]


--
-- process sysEx Messages
--
local function procSysEx(midi)
	if midi==nil then do return end ; end

	-- data dump: 1 data-byte: sysex = 26by
	local	midicat	 = midi.msg:getData():getRange(0,7):toHexString(1)



	if 		midicat	 == g_XWSysEx.syn then 				rxTSS(midi)			-- solo synth
	elseif 	midicat	 == g_XWSysEx.dsp then 				rxDSP(midi)			-- DSP
	else
		-- console("SysEx received but not for me")
	end
end


--
-- process control change messages
--
local function procCC(midi, midiMSG)
	midi.Ch		= midiMSG:getData():getByte(0) - 0xb0
	midi.CC 	= midiMSG:getData():getByte(1)
	midi.val 	= midiMSG:getData():getByte(2)

	-- 1) always discard those CC: 

	if   midi.CC==0x64 or midi.CC==0x65 or midi.CC==0x06 or midi.CC==0x26 then do return end ; end 		-- discard RPN 

	-- 2) these CC are always captured:

	if		midi.CC == 0x40 and getModNameVal("KPedDmpBass") == 1 then									-- sustain foot pedal to 'bass'
		local ccmsg={0xb1, midi.CC, midi.val} ; 	sendMidiMsg(ccmsg)
	end

	if		midi.CC == 0x40 and midi.Ch == 3 and getModNameVal("KVpFPed") == 1 and getModNameVal("KVpSw") == 1 then		-- sustain foot pedal + V-Piano (sends CC64 for low and upp)
		if		midi.val == 0	then  	KVpPSustainPedal(0) ; setModNameVal('KVpPSustain',0) 
		elseif  midi.val == 127	then	KVpPSustainPedal(1) ; setModNameVal('KVpPSustain',1) 
		end
	elseif  midi.CC == 0x00 or midi.CC == 0x20 then 													-- (registration) bank select msb/lsb
		if		midi.CC == 0x00  then 	g_regPCH[midi.Ch].msb = midi.val
		elseif	midi.CC == 0x20  then 	g_regPCH[midi.Ch].lsb = midi.val
		end
	end

	-- 3) these CC are captured if CC 'rules' are active:

	if getModNameVal("KMMCCrcv") == 0 then do return end ; end

	-- apply the midi mapper rules:

	for i,rule in pairs(g_midiMapTmp.cc) do
		if (midi.CC == rule.cc and midi.Ch == rule.ch) and rule.sx and rule.act then
 			if 		rule.id == 'vrsx' 				then MMaction[rule.act](rule.sx, midi.val)
			elseif	string.sub(rule.id,1,2)	== 'vo' then MMaction[rule.act](rule.sx, midi.val)
			end
		end
	end
end



--
-- PC receive timer
--
function procPCrcv_t(tid)
	timer:stopTimer(tid) ; setModNameVal("STGMMPcLED", 0)
end

--
-- process registration program change messages
-- NOTE: VR sends 'program change' messages only for VR registrations: a captured PC is always from a registr.
--
local function procPC(midi, midiMSG)
	local pcrcv, VRMode, VPMode

	if getModNameVal("KMMPCrcv") == 0 then do return end ; end

	midi.Ch		= midiMSG:getData():getByte(0) - 0xc0					-- capture channel
	g_regPCH[midi.Ch].prg = midiMSG:getData():getByte(1)				-- capture prg to temp. variable

	-- check if bank-select + program is complete, otherwise return:

	if	g_regPCH[midi.Ch].msb and g_regPCH[midi.Ch].lsb and g_regPCH[midi.Ch].prg then
		pcrcv	= bit.lshift(g_regPCH[midi.Ch].msb,16) + bit.lshift(g_regPCH[midi.Ch].lsb,8) + g_regPCH[midi.Ch].prg
	else 
		do return end
	end
			
	-- init the VR (reset to 'defaults'):

	VPMode	= getModNameVal("KVpSw")
	if 		VPMode == 1 then setModNameVal("KVpSw", 0, true) end	-- 'Reset' V-Piano

	-- now apply the midi mapper rules:

	for i,rule in pairs(g_midiMapTmp.pc) do
		if (pcrcv == rule.pc and midi.Ch == rule.ch)  and rule.act then
			if 		rule.id	== 'vpsw' then MMaction[rule.act](rule.rv, rule.rp, 1, rule.rv2)-- V-Piano ON
			elseif	rule.id == 'girl' then MMaction[rule.act](rule.rv, rule.rg, rule.sn)	-- GM2 sound load to KBD
			elseif	rule.id == 'vsrl' then MMaction[rule.act](rule.rv, rule.rg, rule.sn)	-- (V-SYNTH REG patch load)
			elseif	rule.id == 'gmrl' then MMaction[rule.act](rule.rv, rule.rg, rule.sn)	-- (GM2 REG  patch load)
			end
		end
	end

	-- blink LED & clean
	
	setModNameVal("STGMMPcLED", 1) ; timer:setCallback(13001, procPCrcv_t) ; timer:stopTimer(13001) ; timer:startTimer(13001, 300) 	-- blink receive LED

	g_regPCH[midi.Ch]	= {}						-- empty the temporary bank-select-program storage triple
end



--[[
##############################################################################################################
####  MAIN  
##############################################################################################################
--]]
--
-- MAIN method for received midi messages
--
midiReceive = function(midiMSG)
	if not isPanelReady() then do return end ; end
	if midiMSG==nil then do return end ; end

	local midiHBy1		= bit.rshift( midiMSG:getData():getByte(0), 4 )	; if midiHBy1 == 8 or midiHBy1 == 9	then return end		-- exit for 'notes'

	-- eventually kick 'request reject' answer: f0 44 16 03 7f 0b 00 00 00 00 f7


	local midi 		  	= {}											-- midi message array: address Highest, High, Mid, Low; msg-size, msg-value
	midi.msg			= midiMSG
	midi.size 			= midiMSG:getSize() 							-- Size of the midi dump received
	midi.byte			= midiMSG:getData():getByte(0)					-- first byte

	-- MONITOR  console('midi msg in: '..midiMSG:getData():toHexString(1))	-- uncomment for testing or monitoring

	if 		midi.size >= 20 and midi.byte == 0xf0						then procSysEx(midi)
--	elseif	midi.size == 3 and bit.rshift(midi.byte,4) == 0xB  			then procCC(midi)
--	elseif	midi.size == 2 and bit.rshift(midi.byte,4) == 0xC			then procPC(midi)
	else	do return end
	end
end
