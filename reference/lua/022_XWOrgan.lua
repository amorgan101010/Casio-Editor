-- CTRLR method: XWOrgan
-- extracted from CasioXW-EDITOR-v0101.bpanelz (author: franky)

--
-- XWOrgan: controller for ORGAN layer
--


local setModNameVal	= setModNameVal
local getModNameVal	= getModNameVal
local sendCC 		= sendCC
local sendNRNP		= sendNRNP

--
-- setOrgVC : Vibrato
--
local function setOrgVC(mid, val, zch)
	local vdid	= g_orgModMidi["orgVC"].orgVCdepth.id
	local vrid	= g_orgModMidi["orgVC"].orgVCrate.id

	local vsw	= getModNameVal("orgVibSw")
	local vt	= getModNameVal("orgVibSwT")
	local cvr	= getModNameVal("orgVCrate")
	local cvd	= getModNameVal("orgVCdepth")
	local vtvr	= 89
	local vtvd	= {9,15,25}

	if vt == 0 then	sendCC(zch, vrid,   cvr) ; sendCC(zch, vdid,  vsw *  cvd)
	else			sendCC(zch, vrid,  vtvr) ; sendCC(zch, vdid,  vsw * vtvd[vt])
	end
end

--
-- setOrgPerc : set organ percussion
--
local function setOrgPC(mid, val, zch)
	local dbval
	local dmod		= { orgPercF={t=15,am="orgPercS"}, orgPercS={t=40,am="orgPercF"} }				-- perc fast/slow switch

 	-- percussion fast/slow:
	if    val == 0 then setModNameVal(dmod[mid].am,0) ; setModNameVal(mid,1) ; do return end ; end	-- ignore '0'-switch

	setModNameVal(dmod[mid].am,0) ; setModNameVal("orgTWpercdec", dmod[mid].t, true)				-- send default values fast/slow
end

--
-- setOrgDB : mute/full/'manual' for Drawbars
--
local function setOrgDB(mid, val, zch)
	local dbval

	if mid 		== "orgDBman"  then
		for dbar,v in pairs(g_orgModMidi["orgTW"]) do
			if dbar:match("dbar") then dbval	= getModNameVal(dbar) ; setModNameVal(dbar, dbval, true) end
		end
	elseif mid 	== "orgDBmute"  then
		for dbar,v in pairs(g_orgModMidi["orgTW"]) do
			if dbar:match("dbar") then setModNameVal(dbar, 8, true) end
		end
	elseif mid 	== "orgDBfull"  then
		for dbar,v in pairs(g_orgModMidi["orgTW"]) do
			if dbar:match("dbar") then setModNameVal(dbar, 0, true) end
		end
	end
end

--
-- setOrgDB : HalfMoon
--
local function setOrgHM(mid, val, zch)
	local hmid, hmval

	if 		val	== 0 then																					-- brake
			hmid = g_orgModMidi["orgROT"]["orgROTbrk"].id ; hmval = 127 ; 	sendCC(zch, hmid, hmval)
	elseif	val == -1 then																					-- slow/chor.
			hmid = g_orgModMidi["orgROT"]["orgROTspd"].id ; hmval = 0   ; 	sendCC(zch, hmid, hmval)
			hmid = g_orgModMidi["orgROT"]["orgROTbrk"].id ; hmval = 0   ; 	sendCC(zch, hmid, hmval)
	elseif	val == 1 then																					-- fast/trem
			hmid = g_orgModMidi["orgROT"]["orgROTspd"].id ; hmval = 127 ; 	sendCC(zch, hmid, hmval)
			hmid = g_orgModMidi["orgROT"]["orgROTbrk"].id ; hmval = 0   ; 	sendCC(zch, hmid, hmval)
	end
end




--
-- sendORGParam : scheduler for organ parameters
--
local function sendORGParam(mid, val, zch)
	local nmsb, nlsb, vmsb=0, vlsb
	local mdef		= {}
	local mdefcalc	= g_xwModCalc["nrpn"]

	if 		mid:match("orgTW") 		then mdef	= g_orgModMidi["orgTW"]					-- 'normal' drawbar organ params
	elseif 	mid:match("orgVC") 		then mdef	= g_orgModMidi["orgVC"]					-- horrible VC
	elseif 	mid:match("orgROT") 	then mdef	= g_orgModMidi["orgROT"]				-- DSP Rot
	elseif 	mid:match("orgGEN") 	then mdef	= g_orgModMidi["orgGEN"]				-- generic
	elseif 	mid:match("orgVib") 	then setOrgVC(mid, val, zch) ; do return end ;
	elseif 	mid:match("orgDB") 		then setOrgDB(mid, val, zch) ; do return end ;
	elseif 	mid:match("orgHMoon") 	then setOrgHM(mid, val, zch) ; do return end ;
	elseif	mid:match("orgPerc") 	then setOrgPC(mid, val, zch) ; do return end ;
	end

	if not mdef or not mdef[mid] then do return end ; end

	nmsb	= mdef["orgMSBid"][1]
	nlsb	= mdef[mid].id

	-- calculate casio midi values
	if 		mdefcalc[mdef[mid].vt] 		then vmsb,vlsb = mdefcalc[mdef[mid].vt](val, 23)
	elseif 	mdef[mid].vt:match("nf-")	then vmsb,vlsb = mdefcalc['nfx'](val, mdef[mid].vt:sub(4))
	end

	-- send midi
	if mdef["orgMIDI"]	== 'nrnp' 	then sendNRNP(zch,nmsb,nlsb,vmsb, vlsb)	end
	if mdef["orgMIDI"]	== 'cc' 	then sendCC(zch,nlsb,vmsb)	end

end



--
-- MAIN method
--
XWOrgan = function(mod, value)
	if not isPanelReady() 	then do return end ; end
	if not value 			then do return end ; end

	local thisMod	= L(mod:getName())

	sendORGParam(thisMod, value, g_XWChannels.rx[1])				-- Organ is always zone 1
end

