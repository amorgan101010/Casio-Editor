-- CTRLR method: SYSControllers
-- extracted from CasioXW-EDITOR-v0101.bpanelz (author: franky)

--
-- SYSContollers: general EDITOR controllers 
--

-- initTables()


--
-- helper functions
--
local setModNameVal				= setModNameVal
local getModNameVal				= getModNameVal
local setModNameEnable 			= setModNameEnable
local setModNameStringProperty 	= setModNameStringProperty

--
-- syncXW : send sync dump messages
--
local function syncXW(dummy)
	local vmsb=0, vlsb
	local mdef		= {}

	mdef	= g_dspModSXtx ; 	sendXWSX(0, mdef["tssDSPTab"].sx)										-- first send DSP type request
	mdef	= g_tssModSXtx ; 	for m,av in pairs(mdef) do  sendXWSX(0, av.sx) end 						-- send Midi sync requests
end


--
-- setXWModel : set Skin for P1 or G1:
--
local function setXWModel(value)
	local XWP1enable

	if value == 0 then
		g_XWModel.id	= 'P1'
		g_gloReg["CFG"]['xwtype'] 	= 0 
		XWP1enable	= true

		setModNameStringProperty("MainGrp", "uiGroupText", "CASIO XW-P1")													-- title
		setModNameEnable("mixHexGrp", true)
	else
		g_XWModel.id	= 'G1'
		g_gloReg["CFG"]['xwtype'] 	= 1
		XWP1enable	= false

		setModNameStringProperty("MainGrp", "uiGroupText", "CASIO XW-G1")													-- title
	end

	-- ## en/disable for P1/G1:

	setModNameEnable("mixHexGrp"	, XWP1enable)
	setModNameEnable("orgROTGrp"	, XWP1enable)
	setModNameEnable("orgCLCKGrp"	, XWP1enable)
	setModNameEnable("orgVIBGrp"	, XWP1enable)
	setModNameEnable("orgHMGrp"		, XWP1enable)
	setModNameEnable("orgPERCGrp"	, XWP1enable)
	setModNameEnable("orgDrawbarGrp", XWP1enable)

	--## set Wave/presets lists and list parameters:

	-- Solo Synth:
	g_tsswf	= g_XWTssWf[g_XWModel.id]																					-- shortcut to wave number offset

	setModNameStringProperty("tssFactPreset-1", "uiComboContent", g_tssFactPreset[g_XWModel.id])						-- preset list
	setModNameStringProperty("tssUserPreset-1", "uiComboContent", g_tssUserPreset[g_XWModel.id])						-- user list

 	panel:setGlobalVariable(1,g_XWModel[g_XWModel.id].tsssynwno)														-- syn wave number
	panel:setGlobalVariable(2,g_XWModel[g_XWModel.id].tsspcmwno)														-- pcm

	setModNameStringProperty("tssOSCwf-1", "uiComboContent", g_tssSYNwave[g_XWModel.id])								-- syn1 wave list
	setModNameStringProperty("tssOSCwf-2", "uiComboContent", g_tssSYNwave[g_XWModel.id]) 								-- syn2
	setModNameStringProperty("tssOSCwf-3", "uiComboContent", g_tssPCMwave[g_XWModel.id]) 								-- pcm1
	setModNameStringProperty("tssOSCwf-4", "uiComboContent", g_tssPCMwave[g_XWModel.id]) 								-- pcm2

	setModNameIntProperty("tssOSCwfm-1", "uiSliderMax", g_XWModel[g_XWModel.id].tsssynwno)								-- syn1 wave number
	setModNameIntProperty("tssOSCwfm-2", "uiSliderMax", g_XWModel[g_XWModel.id].tsssynwno)								-- syn2
	setModNameIntProperty("tssOSCwfm-3", "uiSliderMax", g_XWModel[g_XWModel.id].tsspcmwno)								-- pcm1
	setModNameIntProperty("tssOSCwfm-4", "uiSliderMax", g_XWModel[g_XWModel.id].tsspcmwno)								-- pcm2

	setModNameVal("tssOSCwf-1", 0)																						-- syn1 wave number init
	setModNameVal("tssOSCwf-2", 0)																						-- syn2
	setModNameVal("tssOSCwf-3", 0)																						-- pcm1
	setModNameVal("tssOSCwf-4", 0)																						-- pcm2


	--## change colours:

	-- TSS OSC switches:
		setModNameStringProperty("tssSwGrp", 		"uiGroupBackgroundColour1", g_XWModel[g_XWModel.id].bgcol1) 
		setModNameStringProperty("tssSwGrp", 		"uiGroupBackgroundColour2", g_XWModel[g_XWModel.id].bgcol2 ) 
	-- TSS: Presets
		setModNameStringProperty("tssPresetGrp", 	"uiGroupBackgroundColour1", g_XWModel[g_XWModel.id].bgcol1) 
		setModNameStringProperty("tssPresetGrp", 	"uiGroupBackgroundColour2", g_XWModel[g_XWModel.id].bgcol2 ) 
	-- TSS: OSC 1-6
		for i=1,6 do 
		for j,m in pairs({"M", "P", "F", "A"}) 		do setModNameStringProperty('tssOSC'..m..'Grp-'..i, "uiGroupTextColour", g_XWModel[g_XWModel.id].gtcol ) end
		for m,v in pairs(g_tssModMidi["tssOSC"]) 	do setModNameStringProperty(m..'-'..i, "uiSliderRotaryOutlineColour", g_XWModel[g_XWModel.id].scol ) end
		end
	-- TSS: LFO 1/2
		for i=1,2 do
		for j,m in pairs({"tssLFOGrp"}) 			do setModNameStringProperty(m..'-'..i, "uiGroupTextColour", g_XWModel[g_XWModel.id].gtcol ) end
		for m,v in pairs(g_tssModMidi["tssLFO"]) 	do setModNameStringProperty(m..'-'..i, "uiSliderRotaryOutlineColour", g_XWModel[g_XWModel.id].scol ) end
		end
	-- TSS Total Filter
		setModNameStringProperty('tssFLTFGrp-1', 	"uiGroupTextColour", g_XWModel[g_XWModel.id].gtcol )
		for m,v in pairs(g_tssModMidi["tssFLT"]) 	do setModNameStringProperty(m..'-1', "uiSliderRotaryOutlineColour", g_XWModel[g_XWModel.id].scol ) end
	-- TSS DSP
		setModNameStringProperty("tssDSPGrp", 		"uiGroupTextColour", g_XWModel[g_XWModel.id].gtcol )
		for m,v in pairs(g_tssModMidi["tssDSP"]) 	do setModNameStringProperty(m, "uiSliderRotaryOutlineColour", g_XWModel[g_XWModel.id].scol ) end
	-- TSS Common Grp (main)
		setModNameStringProperty("tssCOMGrp", 		"uiGroupTextColour", g_XWModel[g_XWModel.id].gtcol )
		setModNameStringProperty("tssCOMvol", 		"uiSliderRotaryOutlineColour", g_XWModel[g_XWModel.id].scol ) 
		setModNameStringProperty("tssCOMrev", 		"uiSliderRotaryOutlineColour", g_XWModel[g_XWModel.id].scol )
	-- CFG:
		setModNameStringProperty("cfgHelpGrp", 		"uiGroupTextColour", g_XWModel[g_XWModel.id].gtcol ) 
		setModNameStringProperty("cfgCTRLRGrp", 	"uiGroupTextColour", g_XWModel[g_XWModel.id].gtcol ) 
		setModNameStringProperty("cfgPartCHGrp", 	"uiGroupTextColour", g_XWModel[g_XWModel.id].gtcol ) 
	-- Hixer:


	-- save xw model to config file
	table.save(g_gloReg, g_gloRegFilePath)

end



--
-- SYSContollers: MAIN module
--
SYSControllers = function(mod, value)
	if not isPanelReady() then do return end ; end
	if not value then do return end ; end

 	local thisMod 		= L(mod:getName())			-- get modulator name

	-- Zoom the panel
	if		thisMod	== "CTRLRZoom" then
		local z	= 1.0 + 0.02*value				-- if you change initial values, change also zoom init in initPanel
		panel:getPanelEditor():setPropertyDouble("uiPanelZoom", z)

	-- MIDI panic: Mute all channels, reset midi-connection
	elseif 	thisMod	== "MIDIpanic" then
		local midiDevice

		-- reset (reconnect) existing connection:
		midiDevice	=	panel:getProperty("panelMidiInputDevice")
		panel:setPropertyString("panelMidiInputDevice","-- None")
		panel:setPropertyString("panelMidiInputDevice",midiDevice)

		midiDevice	=	panel:getProperty("panelMidiOutputDevice")
		panel:setPropertyString("panelMidiOutputDevice","-- None")
		panel:setPropertyString("panelMidiOutputDevice",midiDevice)

		-- panic/mute all channels
		for c = 0,15 do  sendCC(c,0x78,0) ; sendCC(c,0x79,0) ; sendCC(c,0x7b,0) ; end 	-- 0x78: sounds off / 0x79: reset CC / 0x7b: notes off

	-- sync with XW
	elseif	thisMod == "MIDIsync" then
		blinkButton({Modulator=thisMod, initialState=0, finalState=0, colON="FFFF0000", colOFF="ff0000ff", blinkTime=5000, blinkInterval=200})
		syncXW(value)

	-- toggles the visibilty of the CTRLR Menu bar
	elseif 	thisMod	== "CTRLRMenu" then
		panel:getPanelEditor():setProperty("uiPanelMenuBarVisible", value, false)

	-- XW Model (P1/G1):
	elseif	thisMod == "cfgXWModel" then
		setXWModel(value)

	-- Start View
	elseif	thisMod == "cfgStartPanel" then
		g_gloReg["CFG"]['pstart'] 	= value	; table.save(g_gloReg, g_gloRegFilePath)

	-- RX/TX channels for parts:
	elseif	thisMod:match("cfgMIDIPartRxCH") then
		local p	= tonumber(thisMod:gsub(".*-",""),10)
		g_XWChannels.rx[p]			= value	
		g_gloReg.MIDI["p"..p].rx	= value		;  table.save(g_gloReg, g_gloRegFilePath)

	elseif	thisMod:match("cfgMIDIPartTxCH") then
		local p	= tonumber(thisMod:gsub(".*-",""),10)
		g_XWChannels.tx[p]			= value	
		g_gloReg.MIDI["p"..p].tx	= value		;  table.save(g_gloReg, g_gloRegFilePath)

	elseif	thisMod:match("cfgMIDIPartRxReset") then
		for p = 1,16 do setModNameVal("cfgMIDIPartRxCH-"..p, p-1, true) end

	elseif	thisMod:match("cfgMIDIPartTxReset") then
		for p = 1,16 do setModNameVal("cfgMIDIPartTxCH-"..p, p-1, true) end

	-- link V-Keys:
	elseif thisMod	== "VKeyAction1" then
		local vkeynote, vkeych2, vkeycc

		if getModNameVal("VKeyLink") == 1 then
			vkeych2		= getModNameVal("VKeyCH2")
			vkeynote	= mod:getMidiMessage(0):getNumber()
			if value	== 0 then vkeymsg = 0x80+vkeych2 else vkeymsg = 0x90+vkeych2 end ; sendMidiMsg({vkeymsg,vkeynote,value})
		end
		do return value end 		-- required for modulator property luaModulatorGetValueForMIDI 'function call' to prevent midi hangers

	-- NoteHold  V-Keys:
	elseif	thisMod == "VKeyHold1" then
		local mch
		if value == 1 then
			mch = getModNameVal("VKeyCH1") ; sendCC(mch,0x40,127)
			mch = getModNameVal("VKeyCH2") ; sendCC(mch,0x40,127)
		else for c=0,15 do 					 sendCC(c,0x40,0) 		end
		end
	end
end
