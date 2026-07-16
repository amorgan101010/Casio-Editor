-- CTRLR method: ToneHexLayer
-- extracted from CasioXW-EDITOR-v0101.bpanelz (author: franky)

--
-- ToneHexLayer: tone controller / mixer for HexLayers
--

local sendCC 	= sendCC
local sendNRNP	= sendNRNP


local function sendHEXParam(mid, val, lay, zch)
	local nmsb, nlsb, vmsb=0, vlsb
	local mdef		= {}
	local mdefcalc	= g_xwModCalc["nrpn"]

	if 		mid:match("mixHEX") then mdef	= g_mixModMidi["mixHEX"]			-- hexlayer
	end

	if not mdef or not mdef[mid] then do return end ; end

	nmsb	= mdef["mixMSBid"][lay]
	nlsb	= mdef[mid].id

	-- calculate casio midi values
	if 		mdefcalc[mdef[mid].vt] 		then vmsb,vlsb = mdefcalc[mdef[mid].vt](val, 23)
	elseif 	mdef[mid].vt:match("nf-")	then vmsb,vlsb = mdefcalc['nfx'](val, mdef[mid].vt:sub(4))
	end

	-- send Midi:
	if mdef["mixMIDI"]	== 'nrnp' 	then sendNRNP(zch,nmsb,nlsb,vmsb, vlsb)	end
end


--
-- MAIN method
--
ToneHexLayer = function(mod, value)
	if not isPanelReady() 	then do return end ; end
	if not value 			then do return end ; end

	local thisMod	= L(mod:getName())

	local modid	= thisMod:gsub("-.*","")
	local layid	= tonumber(thisMod:gsub(".*-",""),10) or 1

	sendHEXParam(modid, value, layid, g_XWChannels.rx[1])				-- HEXLAYER is always zone 1
end

