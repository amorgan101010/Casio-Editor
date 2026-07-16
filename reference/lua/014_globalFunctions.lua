-- CTRLR method: globalFunctions
-- extracted from CasioXW-EDITOR-v0101.bpanelz (author: franky)

--
-- global Functions
--
-- these a functions used in the modulator methods
--

local DEBUG 	= 0
local CONSOLE 	= 0

--
-- Check if the panel is not busy to load or receiving a program
--
function isPanelReady()
	if g_isPanelReady == true then
		return true 
	else
		if panel:getBootstrapState() == false and panel:getProgramState() == false and g_PanelLoaded == true then g_isPanelReady = true ; return true else  return false end
	end
end


--# Standard/Error-Out ##################################################################################

--
-- output to console
--
function echo(...)
	-- do return end
	local str = ""
	for k,v in ipairs(arg) do if v then str=str..tostring(v).." : " end ; end

	console("["..os.date("%H:%M:%S").."] "..str)
end
--
-- Error output to console
--
function debug(...)
	if DEBUG == 0 then return end

	local str = ""
	for k,v in ipairs(arg) do str=str..tostring(v).." : " end

	console("["..os.date("%H:%M:%S").."] "..str)
end
--
-- Error output to poupup windows
--
function toConsole(...)
	if CONSOLE == 0 then return end

	local str = ""
	for k,v in ipairs(arg) do str=str..tostring(v).." : " end

	utils.warnWindow("RUNTME ERROR:", str)
end
--
-- Error output to file
--
function toFile(...)
	local str=""
	local fh
	local debugFile		= "V-Combo.debug"						--  debug file name
	local fdirectory	= File.etetSpecialLocation(File.userDocumentsDirectory):getFullPathName().."\\CTRLR\\V-Combo\\"
	local debugFilePath	= fdirectory.."\\"..debugFile
	local t				= tonumber(  os.date("%H%M%S")..string.gsub(os.clock(),"(%d+)%.",""),10)

	for k,v in ipairs(arg) do str = str..tostring(v).." : " end

 	fh = io.open(debugFilePath,"r") ; if fh then io.close(fh) else File(fdirectory):createDirectory() end
	fh = io.open(debugFilePath,"ab")
	fh:write("["..t.."] "..str.."\n")
	fh:close()
end
--
-- AlertWindows 
--
function utils_textWindow(title, text)							-- allows copy-paste
	local Window
	local fill = ''
	for i=1,32,1 do fill=fill.."                                                                                                                             "; end
	fill = "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"..fill

	Window	= AlertWindow(title..":", "", AlertWindow.NoIcon)
	Window:addTextBlock(text..fill)
	Window:addButton("Close", 0, KeyPress(KeyPress.escapeKey), KeyPress())

	if Window:runModalLoop() == 0 then Window:setVisible (false) end  -- cancel
end

function utils_sysWindow(title, text)							-- OS type window
	local Window
	AlertWindow.showNativeDialogBox(title..":", text, true)
end



--# common Functions ##################################################################################

function tableConcat(t1,t2)
	local t	= {}
	
	for i1,a1 in pairs(t1) do t[i1] = a1 end
	for i2,a2 in pairs(t2) do t[i2] = a2 end
	return t
end

-- return 'value' of the modulator component (visible controller) with name "modname"
function getModNameVal(modname)
	local mod, val

	mod	= panel:getModulatorByName(modname) ; if not mod then toConsole("MOD name",modname); return ; end
	val = mod:getComponent():getValue() 	; if not val then toConsole("MOD getValue",modname); return ; end

	return val
end

-- return 'value' of the modulator with name "modname"
function getModNameMVal(modname)
	local mod, val

	mod	= panel:getModulatorByName(modname) ; if not mod then toConsole("MOD name",modname); return ; end
	val = mod:getValue() 					; if not val then toConsole("MOD getValue",modname); return ; end

	return val
end

-- return 'mapped value' of the modulator of type fixed, combo etc with name "modname"
function getModNameValMap(modname)
	local mod, val

	mod	= panel:getModulatorByName(modname.."") ; if not mod then toConsole("MOD name",modname); return ; end
	val = mod:getValueMapped()					; if not val then toConsole("MOD getValue",modname); return ; end

	return val
end

-- set 'value' of the modulator component (visible controller) with name "modname"
function setModNameVal(modname, val, exe)
	if not exe then exe = false end
	local mod

	mod	= panel:getModulatorByName(modname)	; if not mod then toConsole("MOD name",modname); return ; end
	mod:getComponent():setValue(val, exe)
end

-- set 'value' of the modulator with name "modname" (force execution even if value is unchanged)
function setModNameMVal(modname, val, exe)
	if not exe then exe = false end
	local mod

	mod	= panel:getModulatorByName(modname)	; if not mod then toConsole("MOD name",modname); return ; end
	mod:setValue(val, exe)
end

-- set 'mapped value' of the modulator of type fixed, combo etc with name "modname"
function setModNameValMap(modname, val, exe)
	if not exe then exe = false end
	local mod

	mod	= panel:getModulatorByName(modname)	; if not mod then toConsole("MOD name",modname); return ; end
	mod:setValueMapped(val, exe)
end

function setModNameEnable(modname, modbool)
	local mod
	mod	= panel:getModulatorByName(modname) ; if not mod then toConsole("MOD name",modname); return ; end
	mod:getComponent():setEnabled(modbool)
end

function setModNameIntProperty(modName, modProperty, propInt)
	local mod, prop
	mod		= panel:getModulatorByName(modName)
	if mod then
		prop		= mod:getComponent():getProperty(modProperty)
		if prop  then mod:getComponent():setProperty(modProperty, ""..propInt, true) end
	end
end

function setModNameStringProperty(modName, modProperty, propString)
	local mod, prop
	mod		= panel:getModulatorByName(modName)
	if mod then
		prop		= mod:getComponent():getProperty(modProperty)
		if prop  then mod:getComponent():setPropertyString(modProperty, ""..propString) end
	end
end

function getModNameProperty(modName, modProperty)
	local mod, prop
	mod		= panel:getModulatorByName(modName)
	if mod then
		prop		= mod:getComponent():getProperty(modProperty)
	end
	return prop
end

function getModNameIntProperty(modName, modProperty)
	local mod, prop
	mod		= panel:getModulatorByName(modName)
	if mod then
		prop		= mod:getComponent():getProperty(modProperty)
	end
	return tonumber(prop,10)
end





--
-- blinkButton_t()
-- timer function for blinkButton()
--
function blinkButton_t(tid)
	local function setModColour(colorString,tid)
		panel:getModulatorByName(g_blinkButton[tid].bMod):getComponent():setPropertyString("uiButtonColour"..g_blinkButton[tid].bModS,  colorString)
	end

	if not g_blinkButton or not g_blinkButton[tid] or not g_blinkButton[tid].cnt then do return end ; end

	if g_blinkButton[tid].state  == 0 then  	setModColour(g_blinkButton[tid].cON, tid)  ; g_blinkButton[tid].state = 1
	else  										setModColour(g_blinkButton[tid].cOFF,tid)  ; g_blinkButton[tid].state = 0
	end

	g_blinkButton[tid].cnt = g_blinkButton[tid].cnt+1;

	timer:stopTimer(tid)

	if  g_blinkButton[tid].cnt < g_blinkButton[tid].bNo  then
		timer:setCallback(tid, blinkButton_t); timer:startTimer(tid, g_blinkButton[tid].bInt)
	else 
		if g_blinkButton[tid].fstate ==  1 then setModColour(g_blinkButton[tid].cON, tid) else setModColour(g_blinkButton[tid].cOFF, tid) end
		if g_blinkButton[tid].fModS	 ~= -1 then setModNameVal(g_blinkButton[tid].bMod, g_blinkButton[tid].fModS) end
		g_blinkButton[tid] = nil
	end
end

--
-- blinkButton
--
-- this toggles the COLOUR at a given Modulator State. Examples:
-- 1. toggle button,    Mod-off == black, Mod-on == red, button shall blink when switched on : Modstate=1, finalModState=0, initState=red=1,  finaltState=red=1
-- 2. onetouch button:  Mod-off == black,                button shall blink when touched:    : Modstate=0, initState=black=0,  finaltState=black=0
--
function blinkButton(blkCfg)     	-- blkCfg := { Modulator, ModState, timerId, initialState, finalState, colON, colOFF, blinkTime, blinkInterval }
	local function setModColour(colorString, timerId)
		panel:getModulatorByName(g_blinkButton[timerId].bMod):getComponent():setPropertyString("uiButtonColour"..g_blinkButton[timerId].bModS,  colorString)
	end

	local  timerId	= 22000															-- 'base' blink timer ID. For trimerID used a free timerID is searched
	local initialCol, finalCol, blinkNumber

	if	   blkCfg.timerId   	then  timerId	=  blkCfg.timerId	end
	if not blkCfg.ModState   	then  blkCfg.ModState  		= 0 	end				-- default value: "Modulator" State where blinking appears
	if not blkCfg.finalModState	then  blkCfg.finalModState	= -1 	end				-- set 'invalid' default state
	if not blkCfg.blinkTime		then  blkCfg.blinkTime		= 1000	end 			-- time interval in ms
	if not blkCfg.blinkInterval	then  blkCfg.blinkInterval	= 300	end 			-- time interval in ms
	if not blkCfg.colON   		then  blkCfg.colON	= "FFFF0000"	end 			-- default value
	if not blkCfg.colOFF   		then  blkCfg.colOFF	= "FF111111"	end 			-- default value


	if    blkCfg.blinkInterval  > blkCfg.blinkTime then do return end ; end			-- arithmetic check

	for t=timerId, timerId+63 do
		timerId = t
		if not timer:isTimerRunning(t) then g_blinkButton[t] = {} break ; end		-- get first free  timer
	end

	if not g_blinkButton[timerId] then do return nil end ; end						-- max number of 64 timers reached

	for t=timerId+1,timerId+63 do
		if not timer:isTimerRunning(t) then g_blinkButton[timerId] = nil end		-- clean all other timer-arrays
	end

	blkCfg.blinkInterval 	= blkCfg.blinkInterval/2								-- use 'half'interval
	blinkNumber				= blkCfg.blinkTime/blkCfg.blinkInterval

	if    blkCfg.ModState  	== 1 then blkCfg.ModState = "On" else blkCfg.ModState = "Off" end

	g_blinkButton[timerId]	= { bMod=blkCfg.Modulator, bModS=blkCfg.ModState, fModS=blkCfg.finalModState, state=0, fstate=blkCfg.finalState, 
								cON=blkCfg.colON, cOFF=blkCfg.colOFF, bNo=blinkNumber, bInt=blkCfg.blinkInterval, cnt=1 }

	-- do 'first blink' :
	if 	 blkCfg.initialState == 0 then	setModColour(g_blinkButton[timerId].cON,  timerId) ; g_blinkButton[timerId].state = 1 ; 
 	else 								setModColour(g_blinkButton[timerId].cOFF, timerId) ; g_blinkButton[timerId].state = 0 ;
	end

	-- run flasher: 
	timer:setCallback(timerId, blinkButton_t); timer:startTimer(timerId, g_blinkButton[timerId].bInt)

	return timerId
end




--# Midi Send Functions ##################################################################################

--
-- sendMidiMsg
-- gets a midi message in array or hex-string format:
--   a) full  (array or hex-string) midi message
--   b) hex-string midi message w/o checksum and F7 end byte, adds Roland checksum and F7 byte, sends message
--
-- midimsg  : midi message (array or hex-string)
--
function sendMidiMsg(midimsg)
	local cmsg	= CtrlrMidiMessage(midimsg)												-- to ctrlr midi msg memory block

 	panel:sendMidiMessageNow(cmsg)														-- send CTRLR midi message

	--  console ("sendMidiMsg "..cmsg:getData():getRange(0,cmsg:getSize()):toHexString(1))
end
--
-- prepares and sends control change messages
--
function sendXWSX(act, _msgid, v1, v2, v3)
	-- act: 0 = request data, 1 = send data
	local _val = ""
	local sxmsg

	if 		 not v1 then _val = ""													-- request
	elseif 	 not v2 then _val = string.format("%.2x "          ,v1)
	elseif 	 not v3 then _val = string.format("%.2x %.2x "     ,v2, v1)				-- invert msb/lsb to lsb-msb-mmsb etc
	else 			     _val = string.format("%.2x %.2x %.2x ",v3, v2, v1)	
	end

	sxmsg = string.format("f0 44 16 03 7f %.2x %s %sf7", act, _msgid, _val)  ;  sendMidiMsg(sxmsg)
end
--
-- prepares and sends control change messages
--
function sendCC(zch,cc,val)
	local ccmsg
	local ccch	= 0xb0+zch

	ccmsg = {ccch, cc, val} 	; 	sendMidiMsg(ccmsg)
end
--
-- prepares and sends NRNP messages
--
function sendNRNP(zch,nmsb,nlsb,vmsb,vlsb)

	local ccmsg
	local ccch	= 0xb0+zch

	ccmsg = {ccch, 0x63, nmsb} 	; 	sendMidiMsg(ccmsg)
	ccmsg = {ccch, 0x62, nlsb} 	; 	sendMidiMsg(ccmsg)
	ccmsg = {ccch, 0x06, vmsb} 	; 	sendMidiMsg(ccmsg)
	if vlsb then ccmsg = {ccch, 0x26, vlsb} 	; 	sendMidiMsg(ccmsg) ; end
end
--
-- prepares and sends program (sound) change
--
sendPC = function(zch, msb, lsb, prg)
	local ccmsg
	local ccch	= 0xb0+zch
	local pcch	= 0xc0+zch

	ccmsg = {ccch, 0x00, msb} 	; 	sendMidiMsg(ccmsg)
	ccmsg = {ccch, 0x20, lsb} 	; 	sendMidiMsg(ccmsg)
	ccmsg = {pcch, prg} 		; 	sendMidiMsg(ccmsg)
end
