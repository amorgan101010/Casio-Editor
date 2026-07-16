-- CTRLR method: initPanel
-- extracted from CasioXW-EDITOR-v0101.bpanelz (author: franky)

--
-- init variables on panel load (startup)
--


local function initPanelModulator(modName, modValue, modExe)
	local mod	 = panel:getModulatorByName(modName)

	if mod then if modExe == true then  mod:setValue(modValue, true) else mod:getComponent():setValue(modValue, false) end ; end
end

local function initPanelModulatorText(modName, modTextProperty, modText)
	local mod	 = panel:getModulatorByName(modName)

	if mod then mod:getComponent():setPropertyString(modTextProperty, ""..modText) end
end

local function initPanelModulatorLCD(modName, modText)
	local mod	 = panel:getLCDLabelComponent(modName)

	if mod then mod:setText(""..modText) end
end

local function initPanelModulatorEnable(modName, modValue)
	local mod	 = panel:getModulatorByName(modName)

	if mod then mod:getComponent():setEnabled( (modValue==1) ) end
end


--
-- init TSS ADSRR graphs and faders (NOT! pots)
--
local function initTSSParam(dummy)
	local oscmid, oscmval, envcanv

	for c,va in pairs(g_CANVdata) do
		for m,v in pairs(va) do														-- sliders
			oscmid		= c:gsub('canv', m)
			oscmval 	= getModNameIntProperty(oscmid,"uiSliderDoubleClickValue") or 0
			initPanelModulator(oscmid,oscmval)
		end

		envcanv = panel:getComponent(c) ; if envcanv then envcanv:repaint() end		-- graphs
	end
end


--
-- timer for startup (delayed initialise after 'g_PanelLoaded' == true)
--
function initPanel_t(timerId)

	-- stop timer:
	timer:stopTimer(timerId)

	-- set g_PanelLoaded
	g_PanelLoaded = true

	-- organ modulators:
	initPanelModulator("orgVibSw", 0)
	initPanelModulator("orgVibT",  1)
	initPanelModulator("orgTWclckoff", 0)
	initPanelModulator("orgTWclckon", 0)
	initPanelModulator("orgTWperc2", 0)
	initPanelModulator("orgTWperc3", 0)
	initPanelModulator("orgTWtype", 0)
	initPanelModulator("orgHMoon", 0)
	setModNameVal("orgTWperc3",0) 		; setModNameVal("orgTWperc2",0)
	setModNameVal("orgPercF",1, true) 	; setModNameVal("orgPercS",0)

	-- TSS modulators:
	for o=1,6 do
	initPanelModulator("tssOSCsw-"..o, 0)
	initPanelModulator("tssOSCPortaSw-"..o, 0)
	initPanelModulator("tssOSCLegatoSw-"..o, 0)
	initPanelModulator("tssLFOsync-"..o, 0)
	initPanelModulator("tssTFTFErtrg-"..o, 0)
	initPanelModulator("tssOSCXTFxtrg-"..o, 0)
	initPanelModulator("tssOSCXPxtrg-"..o, 0)
	end


	-- init panel Zoom button (delayed repeated execution for problematic standalone exe):

	local n = panel:getModulatorByName("CTRLRZoom"):getComponent():getValue() or 1
	initPanelModulator("CTRLRZoom",n, true)

	-- init Modulators:
	for p=1,16 do																-- CFG midi channel
	initPanelModulator("cfgMIDIPartRxCH-"..p, g_XWChannels.rx[p])
	initPanelModulator("cfgMIDIPartTxCH-"..p, g_XWChannels.tx[p])
	end

	-- hide CTRLR Menu (delayed repeated execution for problematic standalone exe):
    initPanelModulator("CTRLRMenu", 0, true)

	-- start CTRLR in 'no edit' mode (delayed repeated execution for problematic standalone exe):
	panel:getPanelEditor():setPropertyInt("uiPanelEditMode",0)

	-- set startup view: default or 'custom':
	initPanelModulator("cfgStartPanel",  	g_gloReg["CFG"]["pstart"], true)
	initPanelModulator("cfgXWModel",    	g_gloReg["CFG"]["xwtype"], true)
	initPanelModulator("LayVKEYsw",    		0, true)

	panel:getCanvas():getLayerByName("VKEY"):setPropertyInt("uiPanelCanvasLayerVisibility", 0)
	panel:getCanvas():getLayerByName("HIDE"):setPropertyInt("uiPanelCanvasLayerVisibility", 0)
	if		g_gloReg["CFG"]["pstart"] == 0 then initPanelModulator("LayMIXsw", 1, true)
	elseif	g_gloReg["CFG"]["pstart"] == 1 then initPanelModulator("LaySYNsw", 1, true)
	elseif	g_gloReg["CFG"]["pstart"] == 2 then initPanelModulator("LayORGsw", 1, true)
	else										initPanelModulator("LayMIXsw", 1, true)
	end

	-- init ADSRR:
	initTSSParam()

	-- welcome the user to new editor and/or remind remind user to setup midi::)

	local	prevCEDvers	= 0
	local	thisCEDvers	= tonumber(panel:getProperty("panelVersionMajor"),10)
	if 		g_gloReg["CFG"]["cedver"]	then  prevCEDvers				= tonumber(g_gloReg["CFG"]["cedver"],10) end
 	if 		thisCEDvers > prevCEDvers 	then  g_gloReg["CFG"]["cedver"] = thisCEDvers ; table.save(g_gloReg, g_gloRegFilePath); initPanelModulator("PANELReleaseInfo", 0, true) end

	if panel:getProperty("panelMidiOutputDevice")=="-- None" or panel:getProperty("panelMidiInputDevice")=="-- None" then
		utils.infoWindow("Please setup EDITOR MIDI connection","\
The first start of a (new) EDITOR release requires to (re)configure EDITOR MIDI\n\
=> experienced users: same procedure as every year (edit MIDI CONFIG)\
=> novice users: please go to PANEL [CFG] and read Quick-Guide 'SETUP' for connecting XW and EDITOR by midi")
	end

end


--
-- timer for midi sync
--
function syncMidi_t(timerId)

	-- stop timer:
	timer:stopTimer(timerId)

	-- sync:
	initPanelModulator("KVRSync", 0, true)
end


--
-- MAIN init
--
function initPanel()

	-- global variables:

	g_PanelLoaded	= false
	g_isPanelReady  = false

	g_canv			= {id='',val=0}
	g_blinkButton 	= {}
	g_XWModel		= {
		id="P1",
		P1={scol="FFFFA420",gtcol="ffFFA420",bgcol1="DDFFA420",bgcol2="88FFA420", tsssynwno=310, tsspcmwno=2157},
		G1={scol="FFFF0000",gtcol="ddFF0000",bgcol1="DDFF0000",bgcol2="88FF0000", tsssynwno=765, tsspcmwno=1990},
	}
	g_XWSysEx = {
		syn	= "f0 44 16 03 7f 01 09",			-- tone solo synth
		hex	= "f0 44 16 03 7f 01 08",			-- tone hex layer 
		dsp	= "f0 44 16 03 7f 01 13",			-- dsp for tss and general
	}


	-- init global config table, config file, file path, if file exists, load to global config variable

	g_gloReg			= {}

	local fdirectory	= File.getSpecialLocation(File.userDocumentsDirectory):getFullPathName().."\\CTRLR\\CASIO"

	g_gloRegFilePath 	= fdirectory.."\\casioxw.pcfg"					-- config file name path

 	local fh = io.open(g_gloRegFilePath,"r") ; 							-- load file (or create directory)
	if fh  then io.close(fh) ; g_gloReg = table.load(g_gloRegFilePath) else File(fdirectory):createDirectory() end

	if not g_gloReg then
		g_gloReg	= {}												-- if file is corrupt
		utils.warnWindow("FATAL: EDITOR data file corrupt !", "1. Close EDITOR\n2. Delete file\n"..g_gloRegFilePath.."\n3. Restart EDITOR\n4.Setup your configs again")
		do return end
	end

	local today 	= os.date("*t", os.time())['yday']

	g_gloReg["DATE"] = today

	if not g_gloReg["CFG"] 				then g_gloReg["CFG"] 			= {} end
	if not g_gloReg["CFG"]["cfgver"]	then g_gloReg["CFG"]["cfgver"]	= 0101  end
	if not g_gloReg["CFG"]['xwtype']	then g_gloReg["CFG"]['xwtype'] 	= 0 end						-- P1 or G1
	if not g_gloReg["CFG"]['pstart']	then g_gloReg["CFG"]['pstart'] 	= 0 end						-- editor start mode
	if not g_gloReg["CFG"]["tmfpath"]	then g_gloReg["CFG"]["tmfpath"] = "C:\\Program Files (x86)\\TransMIDIfier\\TransMIDIfier.exe"  end
	if not g_gloReg["CFG"]["clmpath"]	then g_gloReg["CFG"]["clmpath"] = "C:\\Program Files\\CopperLan\\CPManager\\CopperLanManager.exe"  end

	if not g_gloReg["MIDI"] 			then g_gloReg["MIDI"] 			= {} ; for p=1,16 do g_gloReg["MIDI"]["p"..p]={rx=p-1, tx=p-1} end ; end

	-- hash table for XW midi channels:

	g_XWChannels	= { rx={},tx={} } ;  for p=1,16 do g_XWChannels.rx[p]=g_gloReg["MIDI"]["p"..p].rx ; g_XWChannels.tx[p]=g_gloReg["MIDI"]["p"..p].tx  end

	-- Casio XW model:

	if g_gloReg["CFG"]['xwtype'] == 0 then g_XWModel.id="P1" ; elseif g_gloReg["CFG"]['xwtype'] == 1 then g_XWModel.id="G1" ; end


	-- setGlobalVariables:
		-- 1: TSS syn wave number P1/G1
		-- 2: TSS pcm wave number P1/G1

	-- load sound and wave data

	initTables()
	initWaves()
	initPresets()

	-- run delayed 'PanelLoaded' (editor startup view)

	timer:setCallback (1, initPanel_t)  ; timer:startTimer(1,  500)

end
