-- CTRLR method: XWMixer
-- extracted from CasioXW-EDITOR-v0101.bpanelz (author: franky)

--
-- XWMixer: controller for MIXER layer
--

local sendCC 		= sendCC
local sendNRNP		= sendNRNP
local mixDrumKits	= g_mixDrumKits


--
-- // in development //
--

local function sendMIXParam(mid, val, prt, zch)
	local mdef

	if 		mid:match("mixPRTdsp") 		then mdef	= g_mixModMidi.DSP				-- DSP
	elseif 	mid:match("mixPRTtone")		then sendPC(8, mixDrumKits[val+1].msb, 0, mixDrumKits[val+1].prg)
	elseif 	mid:match("mixPRTtss")		then sendPC(1, 98, 0, 2)
	elseif 	mid:match("tssFactPreset")	then sendPC(g_XWChannels.rx[prt], 98, 0, val) ; setModNameMVal("MIDIsync", 0, true)
	elseif 	mid:match("tssUserPreset")	then sendPC(g_XWChannels.rx[prt], 98, 0, val) ; setModNameMVal("MIDIsync", 0, true)
	elseif 	mid:match("tssCOMvol")		then sendCC(g_XWChannels.rx[prt], 0x07, val)
	elseif 	mid:match("tssCOMrevb")		then sendCC(g_XWChannels.rx[prt], 0x5b, val)
	end

	if not mdef or not mdef[mid] then do return end ; end
end


--[[
	-- SOUND LOAD

	-- get all sound definitions from modulator content box:

 	snddefs		= mod:getComponent():getProperty("uiComboContent")
	if 	snddefs == nil then toConsole(thisfunc.." snddefs = nil"); do return ; end ; end

	-- transform the sound definitions list from string to array:

	loadstring( "snddefs_t = {"..snddefs:gsub("^","\""):gsub("$","\""):gsub("\n","\",\"").."}" )()

	-- for selected sound definition, split the definition into array and get name and prgchange

	loadstring ("snd_t = {"..snddefs_t[idx]:gsub("^","\""):gsub("$","\""):gsub("[ ]*=[ ]*","\",\""):gsub("[Â°â ]","").."}")()
	sndname  	= snd_t[1]
	sndprgch 	= snd_t[2]

	-- split sndprgch into pc/msb/lsb:

	_prg	= string.sub(sndprgch,1,2) ; 	s.prg		= tonumber(_prg, 16)
	_msb	= string.sub(sndprgch,3,4) ; 	s.msb		= tonumber(_msb, 16)
	_lsb	= string.sub(sndprgch,5,6) ; 	s.lsb		= tonumber(_lsb, 16)

	-- modulators:

	if thisMod == "VKbRyDrums" then			-- special treatment for 'GM2 rhythm drum set' 
		local ccmsg
		local midiChannel = 9

		ccmsg={ (0xb0+midiChannel),0x00, s.msb} ; 	sendMidiMsg(ccmsg)
		ccmsg={ (0xb0+midiChannel),0x20, s.lsb} ; 	sendMidiMsg(ccmsg)
		ccmsg={ (0xc0+midiChannel),      s.prg} ; 	sendMidiMsg(ccmsg)

		do return end
	end
--]]




--
-- MAIN method
--
XWMixer = function(mod, value)
	if not isPanelReady() 	then do return end ; end
	if not value 			then do return end ; end

	local thisMod	= L(mod:getName())

	local modid	= thisMod:gsub("-.*","")
	local prtid	= tonumber(thisMod:gsub(".*-",""),10) or 1					-- part 1-16

	sendMIXParam(modid, value, prtid, g_XWChannels.rx[prtid])
end
