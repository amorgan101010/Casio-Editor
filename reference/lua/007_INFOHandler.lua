-- CTRLR method: INFOHandler
-- extracted from CasioXW-EDITOR-v0101.bpanelz (author: franky)

--
-- INFOHandler
-- show popup windows for the (?) INFO buttons
--
--


local info = {}


--[[
##############################################################################################################
####    INFO Helps
##############################################################################################################
--]]


info.VKeyInfo  = {title="VIRTUAL MIDI KEYBOARD", desc="\
The virtual midi keyboards can be used for e.g. designing souds on EDITOR (to avoid flipping between PC and keys...)\
To close the keyboard, click on the [x] button or the the left menu |k|e|y| button\
\
* Select midi channels to play XW parts (see below)\
\
* Octave-Shift : by tipping the 'black bars' at each side of the keybed or spinning the mouse-wheel over it\
\
* [LINK] connects right to left keyboard: playing on the right keyboard also plays the left keyboard\
\
* [HOLD] activates Note-Hold on both manuals ([HOLD] 'off' switches ALL Midi Channels off)\
             Note: if 'off' fails to mute the tones, apply left menu 'MIDI PANIC'\
\
\
Default channels/parts\
\
  NOTE: you might have changed rx-channels of XW parts\
\
        [ch]  default part:\
         [1]    Zone1\
         [2]    Zone2\
         [3]    Zone3\
         [4]    Zone4\
         [5]    extern\
         [6]    extern\
         [7]    extern\
         [8]    seq/drum1\
         [9]    seq/drum2\
        [10]   seq/drum3\
        [11]   seq/drum4\
        [12]   seq/drum5\
        [13]   seq/bass\
        [14]   seq/solo1\
        [15]   seq/sol2\
        [16]   seq/chord\
"}



info.ToolsInfo  = {title="Midi TOOLS", desc="\
Software Tools:\
\
\
WINDOWS & OSX (Mac):\
____________________________________________________________________________________________________\
\
|k|e|y|  : Opens a virtual keyboard within EDITOR for remote playing on XW (see also Info (?) in the virtual key\
\
\
WINDOWS only:\
____________________________________________________________________________________________________\
\
[CLAN] : calls the CopperLan Virtual-Midi-Ports middleware Manager\
            CopperLan must be installed on your PC (see Quick-Guide [Software]\
[SEQ]  : MidiAdventure2 superlite freeware sequencer for running a sequencer from your PC while programming XW\
              To run MA2 to Casio XW, a virtual port middleware like Copperlan or LoopMidi must be used\
"}


info.tssDSPInfo  = {title="Solo Synth DSP", desc="\
Solo Synth DSP parameter edit:\
\
Select a Solo Synth DSP (ob bypass) by selecting the appropriate 'tab'\
After that you can edit its parameters\
"}


info.cfgPartMidiInfo  = {title="XW Midi Channel config", desc="\
PARTS MIDI Channel:\
\
If you have changed the RX/TX midi channels on your XW must copy the  values of XW into here (especially for RX)\
"}



info.tssOSCInfo  = {title="SOLO SYNTH HELP", desc="\
XW Solo Synth\
____________________________________________________________________________________________________\
\
XW Solo Synth corresponds to classic VCO/VCF/VCA synthesizers\
See Casio XW manuals, Casio/Mike Martin youtube videos, literature on 'synthesis' etc\
\
\
NOTE: don't forget to select your XW model (P1/G1) in left menu PANEL [CFG]\
This is important as it sets the model specifigc waveform-lists, tone-presets etc\
\
Load a Tone or wave\
____________________________________________________________________________________________________\
\
XW Synth Tone Presets':\
    use PRESET and USER dropdowns in the 'MAIN' group to select a (factory) Preset or one of your User-tones.\
    NOTES:\
    User-tone names cannot be shown yet (reserved for a later release of EDITOR)\
    Loading preset/user tones via Editor auto-executes 'SYNC'. If tones are loaded on XW, manually apply [SYNC]\
\
XW Waves:\
    use 'Wave' dropdowns in the SYN1/2, PCM1/2 etc groups to select factory waves\
\
    Tip: to select or scroll waves you can pick on from the dropdown menu or use the little 'mouse-scroll' \
         button right of the dropdown window: click on +/- or scroll with the middle mouse wheel\
DSP:\
    use the DSP block to select DSP for Solo Synth and parameters\
\
\
Tips&Tricks:\
____________________________________________________________________________________________________\
\
* pots or faders scan be moved by:\
     mouse pointer / fingertouch\
     moise wheel\
     typing a value (with your computer keyboard) into the value field\
* double-click on a pot sets the knob to its default value\
* [set]:  the [set] button above ADSRR-graphs sends the actual ADSRR envelope and 'module' parameters to XW\
* [init]: the [init] button above ADSRR-graphs resets ADSRR envelope and 'module' parameters to\
           defaults and sends them to XW (useful if you 'got lost' in sound programming)\
\
"}



--[[
#### HELPER FUNCTIONS ######################################################################################################
--]]

--
-- to strech windows, add a number of CRs and a big 'placeholder blank megastring'
--
info.FILL = '';
for i=1,32,1 do info.FILL=info.FILL.."                                                                                                                             "; end
info.FILL = "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"..info.FILL


--
-- show/hide all INFO buttons
--
local function PanelInfoMode(value)
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

	if not info[infoid] then utils.infoWindow("INFO", "sorry, no INFO Help available") ; do return end ; end

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


INFOHandler = function(mod,value)
	if not isPanelReady() or not value then do return end ; end
	if panel:getPanelEditor():getPropertyInt("uiPanelEditMode") == 1 then do return end ; end 	-- do not switch layers in edit mode

 	local thisMod 		= L(mod:getName())			-- get modulator name

	if 		thisMod == 'PANELInfoMode'	then PanelInfoMode(value)
	else	infoTWindow(thisMod)
	end
end

