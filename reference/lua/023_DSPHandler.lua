-- CTRLR method: DSPHandler
-- extracted from CasioXW-EDITOR-v0101.bpanelz (author: franky)

--
-- DSPHandler: handler for TSS and general DSPs
--


local sendXWSX		= sendXWSX



--
-- setDSPtssParam: set DSP for TSS
--
local function setDSPtssParam(mid, val)
	local v1,v2,v3
	local mdef		= {}
	local mcalc		= g_xwModCalc["V2SX"]
	local sxid

 	mdef	= g_dspModSXtx

	if not mdef or not mdef[mid] then do return end ; end

	-- set sysex fragment:
	sxid	= mdef[mid].sx
	-- calculate casio midi values
	if 		mcalc[mdef[mid].vt] 		then v1,v2 		= mcalc[mdef[mid].vt](val)
	elseif 	mdef[mid].vt:match("nf-")	then v1,v2		= mcalc['nfx'](val, mdef[mid].vt:sub(4))
	end

	-- send Midi:
	sendXWSX(1, sxid, v1, v2, v3)
end


--
-- MAIN method
--
DSPHandler = function(mod, value)
	if not isPanelReady() 	then do return end ; end
	if not value 			then do return end ; end

	local thisMod	= L(mod:getName())

	if 		thisMod:match("tssDSPTab") 	then setDSPtssParam(thisMod, value)
	elseif 	thisMod:match("tssDSP")		then setDSPtssParam(thisMod, value)
	end

end

