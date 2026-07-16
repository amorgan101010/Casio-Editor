-- CTRLR method: EXTPrograms
-- extracted from CasioXW-EDITOR-v0101.bpanelz (author: franky)

--
-- EXTPrograms	= function(mod,value)
-- This is a launcher for external Programs
-- some of the programs come included in the panel packages (as 'resources')
-- others can be configured in the 'path' set variable
--


--
-- setPrgPath
-- get and save the path to the external exetuable
--
local function setPrgPath(OS, OSroot, prgdsc, prgid)
	local prgpathHdl, prgfile
	local useNative	= true

	if OS == 'win' then useNative = false end 		-- trick: Windows Native windows fails to read 'shortcuts to windows Apps'

	prgpathHdl = utils.openFileWindow("** set program path for ["..prgdsc.."]  **", OSroot, "*.*", useNative)  -- open window to read in-file

	if prgpathHdl:getFullPathName() == "" then return end	-- closed without selection
	if prgpathHdl:isDirectory() then utils.warnWindow("Not an executable !", "Select a file, not a directory :)") ; return ; end
	if not prgpathHdl:existsAsFile() then utils.warnWindow("General File error", "Something went wrong)") ; return ; end

	prgfile		= prgpathHdl:getFileName()

	if OS == 'win' and not ( prgfile:match(".exe$") or prgfile:match(".lnk$") or prgfile:match(".bat$") or prgfile:match(".ps1$") ) then
			utils.warnWindow("Windows file error", "no windows executable: valid executables are .exe, .lnk, .bat, .ps1") ; do return end
	end
	if OS == 'osx' and not ( prgfile:match(".app$") or prgfile:match(".dmg$") or prgfile:match(".sh") )  then
			utils.warnWindow("Windows file error", "no windows executable: valid executables are .exe, .lnk, .bat, .ps1") ; do return end
	end

	g_gloReg["CFG"][prgid]	= prgpathHdl:getFullPathName()
	table.save(g_gloReg, g_gloRegFilePath)

	utils.infoWindow(prgfile.." SAVED", "Saved to EXTERNAL PROGRAM button  [" ..prgdsc.."]") 
end


--
-- launchPrg
-- start program exetuable
--
local function launchPrg(OS, prgdsc, prgpid)
	local prgpath, prgfh, prgdir, prgbin, sep1, sep2

	local function errout(errtype)
		if errtype=='path'  then utils.warnWindow("External Program:", "external program path not set, see (?)-Info") end
		if errtype=='prg'   then utils.warnWindow("Program missing or wrong path", "Install program and/or set correct path") end
 	end

 	prgpath		= g_gloReg["CFG"][prgpid] ; if not prgpath then return ; end		-- do not errout to prevent windows popups during gig
	prgfh		= io.open(prgpath,"r")	 ; if not prgfh   then errout('prg') ; return ; end
	io.close(prgfh)

	sep2,sep1	= prgpath:reverse():find("[/\\]")	; if not sep2 or not sep1 then errout('prg') ; return ; end	-- find path separators
	prgdir 		= prgpath:sub( 1,-(sep1)-1 )		; if not prgdir           then errout('prg') ; return ; end	-- get directory name
	prgbin 		= prgpath:sub( -(sep2)+1 )			; if not prgbin           then errout('prg') ; return ; end -- get executable/binary

	if 		OS == 'win' then	os.execute('start "" /D "'..prgdir..'" /B "'..prgbin..'"')
	elseif	OS == 'osx' then	os.execute('open '..prgpath)
	elseif 	OS == 'lnx' then	os.execute(prgpath)
	end
end


--
-- SwArpeggiator
-- start free arpeggiator program "Sweet Arpeggiator 32"
--
local function SwArpeggiator(OS)
	local prgdir, prgbin

	prgdir	= File.getSpecialLocation(File.currentExecutableFile):getFullPathName().."\\Ctrlr\\"..panel:getProperty("panelUID")
	prgbin	= "Swmiarp32.exe"

	if 		OS == 'win' then 	os.execute('start "" /D "'..prgdir..'" /B "'..prgbin..'"')
	else						toConsole("INFO: builtin arpeggiator is only avaiable on Windows systems")
	end
end


--
-- MidiAdventure
-- start free sequencer program "Midi Adventure 2" 
--
local function MidiAdventure(OS)
	local prgdir, prgbin

	prgdir	= File.getSpecialLocation(File.currentExecutableFile):getFullPathName().."\\Ctrlr\\"..panel:getProperty("panelUID")
	prgbin	= "ma2.exe"

	if 		OS == 'win' then 	os.execute('start "" /D "'..prgdir..'" /B "'..prgbin..'"')
	else						toConsole("INFO: builtin arpeggiator is only avaiable on Windows systems")
	end

end




--
-- MAIN
--
EXTPrograms	= function(mod,value)

	if not isPanelReady() then do return end ; end
	if panel:getPanelEditor():getPropertyInt("uiPanelEditMode") == 1 then do return end ; end 	-- do not start programs in edit mode

 	local thisMod 		= L(mod:getName())			-- get modulator name
	local OS, OSroot

	if package.config:sub(1,1) == '\\' 		then 	OS 	= 'win' ; OSroot = 'C:'				-- Window
	else 						
		if os.popen('uname -s') == 'Linux' 	then 	OS	= 'lnx' ; OSroot = '/bin' 			-- Linux
		else 										OS	= 'osx' ; OSroot = '/Applications' 	-- OSX
		end
	end

	OSroot	= File.getCurrentWorkingDirectory()

	if 		thisMod	== "PPrgArpI" 		then	SwArpeggiator(OS)
	elseif	thisMod	== "KPrgArp" 		then	launchPrg (OS,         "ARPEGGIATOR", "arppath")
	elseif	thisMod	== "KPrgArpP" 		then	setPrgPath(OS, OSroot, "ARPEGGIATOR", "arppath")
	elseif 	thisMod	== "PPrgSeqI" 		then	MidiAdventure(OS)
	elseif	thisMod	== "KPrgSeq" 		then	launchPrg (OS,         "ARPEGGIATOR", "arppath")
	elseif	thisMod	== "KPrgSeqP" 		then	setPrgPath(OS, OSroot, "ARPEGGIATOR", "arppath")
	elseif	thisMod	== "KPrgClan" 		then	launchPrg (OS,         "COPPERLAN MANAGER", "clmpath")
	elseif	thisMod	== "KPrgCLanP" 		then	setPrgPath(OS, OSroot, "COPPERLAN MANAGER", "clmpath")
	elseif	thisMod	== "KPrgMRouter" 	then	launchPrg (OS,         "TRANSMIDIFIER", "tmfpath")
	elseif	thisMod	== "KPrgMRouterP" 	then	setPrgPath(OS, OSroot, "TRANSMIDIFIER", "tmfpath")
	elseif	thisMod	== "KPrgDrum" 		then	launchPrg (OS,         "DRUM COMPUTER", "dcpath")
	elseif	thisMod	== "KPrgDrumP" 		then	setPrgPath(OS, OSroot, "DRUM COMPUTER", "dcpath")
	elseif	thisMod	== "KPrgSeq" 		then	launchPrg (OS,         "SEQUENCER", "seqpath")
	elseif	thisMod	== "KPrgSeqP" 		then	setPrgPath(OS, OSroot, "SEQUENCER", "seqpath")
	elseif	thisMod	== "KPrgSBook" 		then	launchPrg (OS,         "SONG BOOK", "sbpath")
	elseif	thisMod	== "KPrgSBookP" 	then	setPrgPath(OS, OSroot, "SONG BOOK", "sbpath")
	elseif	thisMod	== "KPrgCust-1" 	then	launchPrg (OS,         "CUSTOM PROGRAM", "xp1path")
	elseif	thisMod	== "KPrgCustP-1"	then	setPrgPath(OS, OSroot, "CUSTOM PROGRAM", "xp1path")
	elseif	thisMod	== "KPrgCust-2" 	then	launchPrg (OS,         "CUSTOM PROGRAM", "xp2path")
	elseif	thisMod	== "KPrgCustP-2"	then	setPrgPath(OS, OSroot, "CUSTOM PROGRAM", "xp2path")
	elseif	thisMod	== "KPrgCust-3" 	then	launchPrg (OS,         "CUSTOM PROGRAM", "xp3path")
	elseif	thisMod	== "KPrgCustP-3"	then	setPrgPath(OS, OSroot, "CUSTOM PROGRAM", "xp3path")
	elseif	thisMod	== "KPrgCust-4" 	then	launchPrg (OS,         "CUSTOM PROGRAM", "xp4path")
	elseif	thisMod	== "KPrgCustP-4"	then	setPrgPath(OS, OSroot, "CUSTOM PROGRAM", "xp4path")
	else
	end
end
