-- CTRLR method: INFORHandler
-- extracted from CasioXW-EDITOR-v0101.bpanelz (author: franky)

--
-- INFORHandler
-- show popup windows for 'release' and 'version' info buttons
--


local info = {}

--[[
##############################################################################################################
####    RELEASE INFO 
##############################################################################################################
--]]

info.PANELReleaseInfo = {title="CASIO XW P1/G1 EDITOR RELEASE INFO", desc="\
                                WELCOME TO CASIO XW P1/G1 EDITOR v01.01 'SYNC' MAJOR RELEASE'\
                             ________________________________________________________________________\
\
\
          +-------------------------------------------------------------------------------------------------------------------+\
          |  novice and advanced users are advised to:                                                                                       |\
          |  - change to PANEL [config] and dive into 'QUICK GUIDES' to learn about old and new features             |\
          |  - toggle EXPERT/NOVICE mode (left main menu) to show yellow [?] buttons for context related INFO   |\
          +-------------------------------------------------------------------------------------------------------------------+\
\
WHAT'S in this release (overview):\
____________________________________________________________________________________________________\
\
OVERVIEW:\
Added 'sync' to synchronise EDITOR to actual XW solo synth patch, made DSP selectable, bugfixes\
\
FEATURES:\
+ XW-'sync': added main-menu [sync]-button to synchronise EDITOR Solo Synth params to actual XW solo synth patch\
+ Solo-Synth DSP can now be switches via the EDITOR\
+ introduced '[1>2]' buttons in VCO/VCF/VCA blocks (dublicate 'block' data from syn/pcm 1 to syn/pcm 2)\
(+ deleted '[set]' buttons)\
\
BUGFIXES:\
+ fixed waveform select for PCM and Noise\
+ corrected number scheme in waveform lists\
+ corrected LFO1/2 wave selectors\
+ corrected PWM LFO-depth value range\
\
\
Howto run XW EDITOR and Casio XW\
____________________________________________________________________________________________________\
\
P1 or G1?\
   first select your XW model:\
   in EDITOR switch to PANEL [config] (left main menu). In the 'PANEL CONFIG' box select XW-P1 or XW-G1'.\
\
Setup Midi Connection: \
   in EDITOR switch to PANEL [config] (left main menu) to access the 'Quick-Guide' menu.\
   Click on Quick-Guide [SETUP] and follow the instructions for configuring EDITOR and midi-connection\
\
\
Integrated Help menus\
____________________________________________________________________________________________________\
\
Novice users please read the Quick-Guide menus 'INTRO' and 'TUTORIALS' in PANEL [config]\
Novice and experienced users please:\
- read the regulary updated Quick-Guide menus in PANEL [config]\
- set EDITOR to 'NOVICE' mode (left menu [NOVICE|EXPERT] button) to activate (?)-Info buttons.\
      Clicking on a (?) button will show context related Help-Popups.\
      Mouse can be used to copy-paste the content to e.g. a textfile\
\
\
OUTLOOK to future releases:\
____________________________________________________________________________________________________\
\
* integration of complete 'XW-MIXER\
* integration of XW-P1 'Hex Layer'\
\
\
Known BUGS:\
____________________________________________________________________________________________________\
\
EDITOR is not free from 'bugs' :) Some can be fixed, others cannot due to bugs of CTRLR platform:\
\
* bulk pop-up of ctrlr windows when loading a panel to CTRLR (CTRLR bug): \
    SOLUTION: before loading a new EDITOR panel, all old panels MUST BE CLOSED\
    Whenever the bulk pop-up 'happens', Windows users should press 'control+shift+escape' \
    to open the task manager and manually kill the CTRLR task\
* Quick-Guide/Info popups: formating large text is restricted by CTRLR.\
    We do the best we can, but text layout can look strange on low resolution displays \
"}






--[[
##############################################################################################################
####    CHANGE LOG
##############################################################################################################
--]]


--
-- basic help functions:
--

info.QHelpChLog = {title="CASIO XW P1/G1 EDITOR VERSION INFO", desc="\
\
-------------------------------------------------------------------------------------------\
v.01.01 major release\
-------------------------------------------------------------------------------------------\
MAIN MENU:\
   [sync]: added main-menu [sync]-button to synchronise EDITOR Solo Synth params to actual XW solo synth patch\
TSS:\
   DSP can now be switches via the EDITOR\
   introduced '[1>2]' buttons in VCO/VCF/VCA blocks (dublicate 'block' data from syn/pcm 1 to syn/pcm 2)\
   deleted '[set]' buttons\
   bugfix: corrected PWM LFO number range \
   fixed waveform select for PCM and Noise\
   corrected number schemes in waveform lists\
   corrected LFO1/2 wave selectors\
   corrected PWM LFO-depth value range\
\
\
-------------------------------------------------------------------------------------------\
v.01.00 initial release\
-------------------------------------------------------------------------------------------\
+ Initial release of XW EDITOR as a CONTROLLER for Casio XW-P1 and XW-G1. XW-EDITOR\
   - runs on any PC, laptop or tablet running MS Windows (XP-W11), MaxOS (OSX) or Linux\
   - can be countrolled by mouse or touch display \
   - offers REALTIME editing of selected features of XW: Solo Synthesizer, Tonewheel Organ and 'HexLayer-Mixer'\
+ IMPORTANT (due to limitations of XW-Midi): \
   1. features like HexLayer-Synth, Performance, Sequencer, Arp,  etc. CANNOT be edited realtime\
        for this purpose use the official 'off-time' Casio XW-Editor (for PRESET editing)\
   2. XW-EDITOR CANNOT (yet) synchronise to the actual state of XW: the editor has to be used like a classic hardware synth\
\
FEATURES:\
+ realtime editing of the (actually loaded) Solo-Synth sound\
+ realtime editing of XW-P1 Hex-Layer parameters like on P1s left control board \
+ realtime editing of XW-P1 Tonewheel organ\
\
"}





--[[
#### HELPER FUNCTIONS ######################################################################################################
--]]

--
-- to strech windows, add a number of CRs and a big 'placeholder blank megastring'
--
info.FILL = "";
for i=1,32,1 do info.FILL=info.FILL.."                                                                                                                             "; end
info.FILL = "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"..info.FILL


--
-- show/hide all INFO buttons
--
local function KKBDInfoMode(value)
	local visible	= false

	if 	value == 0 then visible = false else visible = true end

	for mod,txt in pairs(info) do
		if panel:getModulatorByName(mod) then panel:getModulatorByName(mod):getComponent():setVisible(visible) end
	end	
end


--
-- Alert Window handler
--
local function infoTWindow(infoid)
	local iWindow, winret

	if not info[infoid] then utils.infoWindow("INFO", "sorry, no Quick Guide available") ; do return end ; end

	iWindow	= AlertWindow(info[infoid].title..":", "", AlertWindow.InfoIcon)
	iWindow:addButton("Close", 0, KeyPress(KeyPress.escapeKey), KeyPress())

	if info[infoid].imgT then		-- text + image at TOP
		for k,comp in ipairs(info[infoid].imgT) do if panel:getComponent(comp) then iWindow:addCustomComponent(panel:getComponent(comp)) end;end
		iWindow:addTextBlock(info[infoid].desc..info.FILL..info.FILL..info.FILL)

	elseif info[infoid].imgB then	-- text + image at BOTTOM
		iWindow:addTextBlock(info[infoid].desc..info.FILL)
		for k,comp in ipairs(info[infoid].imgB) do if panel:getComponent(comp) then iWindow:addCustomComponent(panel:getComponent(comp)) end;end
		iWindow:addTextBlock(info.FILL..info.FILL)
	else							-- text without image
		iWindow:addTextBlock(info[infoid].desc..info.FILL..info.FILL)
	end

	winret 	= iWindow:runModalLoop()
	if winret == 0 then iWindow:setVisible (false) end  -- cancel
end



--[[
##############################################################################################################
####  MAIN  
##############################################################################################################
--]]


INFORHandler = function(mod,value)
	if not isPanelReady() or not value then do return end ; end
	if panel:getPanelEditor():getPropertyInt("uiPanelEditMode") == 1 then do return end ; end 	-- do not switch layers in edit mode

 	local thisMod 		= L(mod:getName())			-- get modulator name

	if 		thisMod == 'KKBDInfoMode'	then KKBDInfoMode(value)
	else	infoTWindow(thisMod)
	end
end

