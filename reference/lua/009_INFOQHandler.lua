-- CTRLR method: INFOQHandler
-- extracted from CasioXW-EDITOR-v0101.bpanelz (author: franky)

--
-- INFOQHandler
-- show popup windows for the Quick-Guide buttons
--
--



local info = {}


--[[
##############################################################################################################
####    Quick-Guides
##############################################################################################################
--]]


info.QHelpIntro = {title="QUICK GUIDE", desc="\
Casio XW-P1/G1 and the CTRLR EDITOR:\
____________________________________________________________________________________________________\
\
CTRLR XW EDITOR is a CONTROLLER for Casio XW-P1 and XW-G1.\
\
It can be used on any PC, laptop or tablet running MS Windows (XP-W11), MaxOS (OSX) or Linux with mouse or touch display.\
\
EDITOR offers REALTIME editing of selected features of XW: Solo Synthesizer, Tonewheel organ (and in a future release: XW Mixer and DSP)\
\
INPORTANT: due to limitations of XW-Midi \
1. features like HexLayer-Synth, Performance, Sequencer, Arp,  etc. CANNOT be edited realtime\
     for this purpose use the official 'off-line' Casio XW-Editor (for PRESET editing)\
2. XW-EDITOR CANNOT synchronise to the actual state of XW: the editor has to be used like a classic hardware synth\
\
\
XW EDITOR is featuring/supporting:\
____________________________________________________________________________________________________\
\
    - real time editing of the (actual) XW-P1/G1 solo synth patch\
    - real time editing of XW-P1 Hex-Layer parameters like on P1s left control board \
    - real time editing of XW-P1 Tonewheel organ\
\
    in future relases:\
    - real time editing of XW-P1/G1 MIXER\
    - real time editing of DSP parameters\
    - synchronisation of XW to EDITOR\
\
\
EDITOR PLUGIN vs. STANDALONE (MS Windows only)\
____________________________________________________________________________________________________\
\
XW EDITOR panel is a panel-PLUGIN running on a panel-host platform called 'CTRLR'\
\
For Windows, XW EDITOR panel is released as a plugin for CTRLR-platform AND as a standalone ('.exe') program\
(the Windows-standalone is a combined 'CTRLR + EDITOR panel' package in 'one box')\
\
IMPORTANT: EDITOR is optimsed for CTRLR v5.4.29 (newer CTRLR versions radically change the panel design and are buggy)\
\
\
EDITOR as a VST\
____________________________________________________________________________________________________\
\
CTRLR EDITOR can be also used as a VST plugin:\
The installer-package of CTRLR comes with VST-plugins for 32 and 64 bit. To use EDITOR as a VST plugin, you can:\
a) load the CTRLR-VST plugin into your host, then in the plugin, load EDITOR panel\
b) create a 'EDITOR VST plugin':\
      in your VST host: load CTRLR, then load panel, then go to CTRLR-Menu 'File > Export > Export instance' and save the VST-plugin\
\
NOTE: To run CTRLR-VST plugins, MS Visual Studio 2015 or  2017 (only C++ pack) must be installed\
\
In the list of tested VST-hosts only Reaper worked with the CTRLR plugin (and only with 64bit plugin)\
\
* Tested VST-Hosts with CTRLR-VST-Plugins:\
\
   - Plugin: Platform-VST 'Ctrlr....dll'\
      CTRLR-vers.	VST-Host/x32	VSTHost/x64	SAVIHost2/x32	SAVIHost2/x64	Zynewave	LMMSx64	Reaper x64\
      v5.429 x32	<ok>[1]				<ok>[1]     	<ok>[2]					ld.fail						ld.fail			win.dpl	 	<ok>[1]\
      v5.429 x64	<ok>[1]				<ok>[1]     	<ok>[2]					ld.fail						ld.fail			win.dpl	 	<ok>\
\
   - Plugin: Panel-VST 'CASIO-EDITOR-<version>.dll' (VST export with VST-Host and Reaper)\
      CTRLR-vers.	VST-Host/x32	VSTHost/x64	SAVIHost2/x32	SAVIHost2/x64	Zynewave	LMMSx64	Reaper x64\
      v5.429 x32 	<ok>        		<ok>        		<ok>							ld.fail						ld.fail			<ok>			<ok>\
      v5.429 x64 	<ok>        		<ok>        		<ok>							ld.fail						ld.fail			<ok>			<ok>\
\
\
    glossary:\
    ld.fail    : load fails: VST host cannot load the plugin (.dll not recognized etc..)\
    dm.dis	: dropdown menu disfunctional: the dropdowns of CRTLR-Menu (e.g. for midi config) or panel (iCombo-modulators) do not popp up \
    win.dpl  : window displaced:  displaced (uncentered) CTRLR window in the VST-frame, so the panel working area cannot be accessed\
    [1]        : as win.dpl but fixable: load CTRLR panel, close/open VST windows => CTRLR is centered\
    [2]        : as win.dpl but fixable: load CTRLR panel, save SAVI config, exit and restat SAVI => CTRLR is centered\
\
\
Hardware recommendations: \
____________________________________________________________________________________________________\
\
EDITOR is not performance consuming: even low spec Windows tablets (eg. 2GB ram, intel 'X' CPU) do the job.\
\
Thoughts about chosing hardware:\
\
- Althought EDITOR is primarily designed for building Solo Synth sounds you might think about using it also as\
    live controller e.g. for drawbar organ or Mixer, particulary with a *touch-sentitive* tablet/laptop:\
- Screen size for 'live usage': 10 inch is enough for the editor. If you want to run other programs like\
    songbooks or virtual instruments, 12 inches is the minimum size\
- Touch-laptops/tablets/2-in-1: \
    tablets, 2-in-1 (laptops with detachable display that serves as standalone tablet) or MS Surface+Clones are\
    by far the most 'handy' tools for live usage (space-saving, touch control etc).\
    In case of 'pure' tablets think of a bluetooth keyboard for 'daily work', e.g. software installs, configuration etc\
- Built-in soundcards: modern tablets/laptops have builtin soundcards that are OK for playing virtual instruments.\
    Mini USB-audiocards (size of a thumb drive) can significantly improve the tablet sound:\
    An excellent mini soundcard is 'Axagon ADA-17' (10-20 euro/USD - with stereo(!)-in for elektret/Javalier microphones or line-in).\
    The line-out/headphone jack of your tablet/PC can be connected to XW audio-in\
    Note that 'big' virtual instruments (Grands, Tonewheel Organs ...) require a powerfull premium tablet/laptop\
\
\
Examples:\
- desktop PC: any\
- laptop: any\
- Windows tablets: any\
- 2-in-1 laptops: industry has abandoned small 2-in-1 laptops (with solid keyboard + detachable tablet-display)\
      Former products (2nd hand) were HP X2-10, IBM Miix 320, Chuwi Hi10 X,  etc.\
- tablets+keyboard: Microsoft 'Surface' and 'clones' (HP, Lenovo, Huawei etc)\
- 'big-screen' detachable tablet-laptop: Medion AKOYA S6213T/S6214T with 15.6 FHD touch display\
         2nd hand in EU (ebay or www.kleinanzeigen.de, ca. 100-200 euros)\
\
\
BACKUP, RECOVERY and EDITOR troubleshooting\
____________________________________________________________________________________________________\
\
1) XW backups\
\
    !! IMPORTANT !! REGULARY BACKUP your XW presets to an usb-stick\
    If not, the day XW needs a factory reset all your work will be lost\
\
    Howto save/import backups is explained in XW Manuals (download from Casio Website)\
\
2) EDITOR backup & config recovery\
\
    !! IMPORTANT !! REGULARY COPY the EDITOR config files from your PC to other devices (storage, other PC, usb-stick, SD-card...)\
    Otherweise the day your hard disk crashes or the PC needs a reinstall your EDITOR settings will be lost\
\
    EDITOR saves its config to file 'casioxw.pcfg' in the 'Document' -> CTRLR -> CASIO folder of your PC/Mac.\
\
    To recover the EDITOR casioxw.pcfg-file (file got corrupted), close EDITOR and copy a backup of the file to the mentionned directory\
\
3) EDITOR (CTRLR) crashes on startup (does not start up):\
\
    Windows:\
    EDITOR 'CTRLR panel-plugin': go to folder C:\\Users\\<user>\\AppData\\Roaming\\Ctrlr\\ and delete file 'Ctrlr.settings', restart EDITOR\
    EDITOR 'standalone-exe'      : go to folder C:\\Users\\<user>\\AppData\\Roaming\\CASIO-EDITOR-<version>\\ and delete \
                                                                        file 'CASIOXW-EDITOR-<version>.settings', restart EDITOR\
\
    Mac/OSX:\
    EDITOR 'CTRLR panel-plugin': in your home-directory go to folder Library/Preferences/Ctrlr and delete file 'Ctrlr.settings', restart EDITOR\
\
4) EDITOR/CTRLR hangs in a 'crash-loop' at startup\
\
    use task manager to kill the CTRLR process(es), then delete file 'Ctrlr.settings' (see above '3)')\
\
"}


info.QHelpDox = {title="QUICK GUIDE", desc="\
MANUALS, TUTORIALS and inbuilt 'HELP'\
____________________________________________________________________________________________________\
\
** EDITOR builtin 'Quick-Guides':\
\
    First steps for beginners\
\
** EDITOR builtin INFO (Help) mode:\
\
    In the left bottom menu panel of the EDITOR, use the 'EXPERT/NOVICE' button to switch from EXPERT to NOVICE mode \
    In NOVICE mode, small yellow INFO-buttons (?) are shown on the panel.\
    Click them to open context related INFO windows.\
\
** USER MANUALS for Casio XW-P1 XW-G1:\
\
    Download from Casio Music Website\
\
** EMAIL SUPPORT in English, German and French :   vcombo09730@gmail.com \
\
    Please consider that the EDITOR is voluntary, non commercial software. \
    Please have the patience that answers can have 2-3 weeks delay\
    (The author is offline during vacancies :) )\
\
** FORUMS / online help:\
\
    facebook  : group 'Casio XW-G1 and XW-P1 Synthesizer Group' : vivid FB group for any question or suggestion\
\
    www.casiomusicforums.com : sections for XW\
\
    Youtube   :   youtube channel 'higgy77'\
\
** TUTORIALS for VA synth editing :\
\
    Roland 'Gaia Owners Manual' (download from Roland website): detailed explanation for using Roland Virtual Analog Synthesizer\
                     (Chapter 'Creating Sounds')\
\
    Youtube   : there are excellent tutorials from Casio and Mike Martin about the XW\
"}



info.QHelpUsage = {title="QUICK GUIDE", desc="\
Casio XW-P1/G1 EDITOR functions\
\
\
LEFT MENU structure:\
____________________________________________________________________________________________________\
\
Zoom: zooms the EDITOR to your display size\
\
PANEL selectors: select one of those 'panels':\
    [MIX]  : XW-Mixer\
    [SYN]  : XW Solo Synthesizer\
    [ORG]  : XW-Organ\
    [CFG]  : EDITOR Configuration\
\
    Note: you can define the 'panel' into which EDITOR starts up in [config]\
\
MIDI: control Midi:\
    [PANIC] : Midi Panic button\
    [cfg]   : opens a window for setting Midi connections to XW\
    [SYNC] : synchronises EDITOR to the actual XW (Solo-synth) patch\
\
TOOLS:\
    [|k|e|y|] : opens a Virtual Keyboard to play XW remotely from your PC\
    [CLAN] : [Windows only]: starts CopperLan Virtual Midi Ports Manager (CL has to be installed)\
    [SEQ]  : [Windows only]: opens an inbuilt sequencer to play XW remotely from your PC (requires virtual ports)\
\
EDITOR:\
    [CTRLR menu SHOW|HIDE]: this fades in/out the main control menu of CTRLR platform:\
                 hiding the menu saves place on the monitor\
                 showing the menu is necessary for e.g. loading (upgrading) panel plugins (e.g. in OSX)\
    [NOVICE/EXPERT] : 'NOVICE' mode adds 'info buttons' (?) to the editor with context related help\
\
\
XW-Mixer:\
____________________________________________________________________________________________________\
\
The 'MIXER' actually contains only a 'HexLayer-Mixer': the hexlayer-mixer corresponds to XWs left user panel in 'HEX LAYER' mode:\
- Knobs 1-4\
- Layer Volumes 1-6\
\
A later upgrade of XW-EDITOR shall entire XW-Mixer\
\
\
Solo Synthesizer:\
____________________________________________________________________________________________________\
\
As far as possible, the Solo-Synth-Editor is 'simliar' to the menu structure of XW and the offical Casio XW editor\
\
\
XW-Organ:\
____________________________________________________________________________________________________\
\
Self-explaining\
Note that you have to manually select DSP 'Rotary' on XW in order to customise the 'Rotary' parameters\
\
\
CFG (config & quick-guides)\
____________________________________________________________________________________________________\
\
The quick quides contain information about xW-Editor: you might find them insufficient .. \
  - CTRLR is very limited in displaying text and pictures\
  - more text will be added with time\
Panel config: set your XW-Model and the 'startup panel' of EDITOR\
PARTS MIDI Channel: if you have changed the RX/TX midi channels on your XW, enter those values here\
\
\
Tips*Tricks\
____________________________________________________________________________________________________\
\
* generally when you change values on XW via Editor the XW display does not immediately show the new value:\
    to 'refresh' just move the cursor of XW display\
* pots or faders scan be moved by:\
     mouse pointer / fingertouch\
     moise wheel\
     typing a value (with your computer keyboard) into the value field\
- double-click on a pot sets the knob to its default value\
- [1>2]:  the [1>2] button over ADSRR-graphs copies the values of 'oscillator 1' to 2: from syn1 to syn2 and pcm1 to pcm2\
- [init]: the [init] button over ADSRR-graphs resets all parameters of this 'module' to defaults\
      and sends them to XW (this can be useful if you 'got lost' in sound programming)\
* Computer-mouse scroll on 'large dropdown lists': besides dropdown boxes (e.g. syn waves) is a '+/-' button:\
   - use mouse-click on '+/-' to scroll the list up and down\
   - use mouse-wheel on the button to scroll the list up and down\
* to use the inbuilt sequencer (in Windows) a virtual midi port middleware has to be installed on your PC, e.g. CopperLan\
* inbuilt sequencer and Virtual Keyboard': these allow to play tones on your XW during sound design right sitting on your PC\
    The sequencer a very basic tool: it takes some time to create sequences but once one get's use to it becomes quite easy\
* INFO (?) popups can be closed with [close] button or by ESC key of your computer keyboard\
\
"}


info.QHelpMidi = {title="QUICK GUIDE", imgT={"_midicfg"}, desc="\
(1) EDITOR-CONFIGURATION\
____________________________________________________________________________________________________\
\
First adapt EDITOR to your XW:\
\
In EDITOR select PANEL [CFG]:\
- in 'PANEL CONFIG' select your XW model\
- in 'PANEL CONFIG' select in which Panel shall be shown on EDITOR startup\
- in 'PARTS MIDI CHannel:' if you have changed the TX/RX channels of your XW, insert those values here\
\
\
(2) MIDI-CONNECTION\
____________________________________________________________________________________________________\
\
EDITOR and XW need a 'midi connection' to communicate with each other:\
   - a physical connection (cable)\
   - configuration of the midi connection in EDITOR:\
       - at the 'very first use' of EDITOR or any new EDITOR release\
       - each time a panel has been reloaded\
       - after having deleted the 'settings' file (because of EDITOR problems)\
\
\
(2A) EDITOR - first use / simple setup\
____________________________________________________________________________________________________\
\
   This connects EDITOR directly with XW midi ports: this is the 'simple' setup on Windows and default on Mac\
   1) connect your XW to your PC/Mac using either\
        - USB-A-to-USB-B cable ('printer-USB-cable', see picture) to the square USB socket on the back of XW\
        - 5-pin midi in/out cables to XW, running to a midi-usb box like 'MidiMan' which then goes into your PC \
   2) on Windows, make sure that no other application is connected to XW midi ports\
   3) configure the midi connection in CTRLR EDITOR:\
        in EDITOR left main menu, section MIDI, push [cfg] to open the midi config window. Set:\
        Set (see also picture)\
                     Input device :       'CASIO USB-MIDI'  -- input 'MIDI Channel' to 'ALL'\
                     Output device :     'CASIO USB-MIDI'  -- input 'MIDI Channel' to 'any'\
                     Controller device :  (don't set)\
   4) test your connection:\
        load a solo synth sound. In EDITOR click left main menu [SYNC] and observe if EDITOR syncs to XW Solo-Synth patch\
\
   If the connection fails (no reaction) try (in that order):\
   - in EDITOR: verify midi ports \
   - in EDITOR: click MIDI-PANIC to reset the midi connection \
   - in EDITOR: click left main menu 'CTRLR-MENU' to open the CTRLR menu, go to CTRLR menu 'MIDI', select 'refresh devices'\
       (eventually do a MIDI-PANIC)'\
   - restart EDITOR\
   - restart XW and redo the connection process\
   - restart your computer and redo the connection process\
   - contact facebook, the author and cry for help :)\
\
   Note: for Windows we recommend using 'CopperLan'-Software or 'loopMIDI' to connect XW and EDITOR via 'virtual midi ports',\
        See below (C) and Quick-Guide [Software] for details about (freeware) CopperLan/loopMIDI\
\
\
(2B) EDITOR - regular use\
____________________________________________________________________________________________________\
\
   - Connect your XW to your PC/Mac as explained in (A)\
   - Start 'CTRLR' with the EDITOR panel\
\
\
(2C) Sophisticated midi setup for MS-WINDOWS (not OSX or Linux!):\
____________________________________________________________________________________________________\
\
   To connect the XW to multiple (midi) programs (EDITOR + songbook + virtual organs etc) Windows requires a 'multi-socket'\
   midi-software like CopperLan or LoopMIDI which provides virtual midi ports (OSX and Linux have this 'on board')\
\
   => CopperLan: has 'single-client' ports (maximal one app connected to a port) but sophisticated routing between ports\
                           and offers 'auto reconnect' between XW and PC after restarting EDITOR (or PC).\
\
   => loopMIIDI: has 'multi-client' ports (any number of apps connected to a port) but requires additional software like \
                           midiOX (or copperLan as router)  for 'port-routing'\
\
   For details see Quick-Guide [Software]\
"}



info.QHelpXWSW = {title="QUICK GUIDE", desc="\
Software for Casio XW:\
____________________________________________________________________________________________________\
\
\
This quickhelp gives advice for 'external software' *** FOR MS WINDOWS *** that can be used with the XW:\
\
\
Usefull little software tools\
____________________________________________________________________________________________________\
\
- the builtin 'SEQ': EDITOR comes with a basic sequencer app for Windows ([SEQ] in left main menu)\
\
- TechnoToys Omega tool pack: and oldschool 'abandonware' tool pack that contains (standalone) programs for drumcomputer, sequencer, arpeggiator etc.\
     Download link:   http://drive.google.com/drive/folders/1j5me_UK6TLqfDIxyk1ft-5qkZ4y5iuqT\
\
- VMPK 'Virtual MIDI Piano Keyboard': 'master controller app' for sending (midi) notes and controllers  to XW.\
     VMPK is 'the standard app', but rather big installer and needs 2-3 secs to start.\
\
- Freepiano 2 : a compact (only 1.3 MB), quick starting 'keybed app' for sending (midi) notes to XW.\
     Freepiano 2 also has a 'phrase recorder' for recording playing back (also as 'loops') melodiies and 'phrases\
\
\
Virtual Ports and routing\
____________________________________________________________________________________________________\
\
MS Windows midi ports are so called 'single client' ports, e.g. you can only attach ONE application to the XW midi port\
\
To connect more than one app to the XW (e.g. CTRLR + DAW) or connect apps to each other:\
1. so called 'virtual ports' must be used\
2. 'routes' must be set between the ports (so called virtual midi cables)\
\
There are some (freeware) software products providing 'single client'-virtual ports or even multi-client virtual ports\
\
The most reliable 'virtual ports' middlewares are 'CopperLan' and 'LoopMIDI' (both are freeware)\
Due to 'versatilty' and 'ease of use' we fortly recommend CopperLan\
\
*) CopperLan: CopperLan (freeware) is a mighty but easy to use Midi Router software that provides virtual midi ports:\
    - very stable\
    - starts as a 'service' at PC boot time\
    - up to 32 (single-client) virtual ports with routing and multiplexing (e.g. sending a message to several ports):\
    - 'autoconnect' after XW or PC reboot (see INFO 1)\
    - basic message filtering (channel filter)\
    - works with virtual midi ports of other software products (e.g.  multi-client LoopMIDI or LoopBe, see below).\
\
    Download CopperLan from 'www.copperlan.org': on Windows 10/11 install 1.4.3 or 1.3.3 (V1.4.5 drops midi channels during runtime)\
\
    INFOS:\
      - CL detects every midi gear plugged to the PC and 'hijacks' its midi ports.\
             To 'free' midi ports, go to CL Manager 'Edit' menu and turn the ports off in 'CP to Midi' and 'Midi to CP' \
      - CL normaly auto-reconnects XW and PC after reboot. If reconnect fails go to CL menu 'Edit'=>'MIDI settings' and toggle 'Active Sensing'\
      - CL runs as a Windows Service at PC boot and cannot be stopped/started by 'normal user', only by 'Adminstrator', e.g. in the case CL 'hangs up'\
                and needs a restart (which rarely happens and only when 'experimentating' with weird midi).  Use the command line options:\
                   stop service:         C:\Program Files\CopperLan\CPVNM\cpvnm  -t\
                   start service:        C:\Program Files\CopperLan\CPVNM\cpvnm  -s\
                   restart service:      C:\Program Files\CopperLan\CPVNM\cpvnm  -s -t\
               (add those to 'cmd' bat-scripts and 'Run as Administrator' - if it does not help, restart PC)\
\
*) LoopMIDI: Tobias Erichsens 'industrial standard' LoopMIDI provides an unlimited number of virtual multi-client ports.\
    LoopMIDI offers 'only' virtual ports, no 'routes': to create necessary routes a 2nd software (e.g. MidiOX) must be installed\
\
*) LoopBe: like LoopMIDI, but no port naming, free version 'LoopBe1' provides only one port\
\
*) MidiOx:  despite oldfashioned GUI, MidiOx remains THE workhorse for Midi administration. A part from monitoring, filtering,\
    etc MidiOX can be configured as router between virtual ports (e.g. from LoopMIDI).\
\
*) TransMidiFier: midi router, filter and channel multiplexer (sends a midi message, e.g. a 'note on/off' from one channel to several channels).\
      Does not transmit SysEx\
\
*) BOME Classic: powerful midi mapper/translator (e.g. mapping XW CC or NRNP to CC) and router (the Pro Version is not needed for 'normal usage')\
\
\
Digital Song Book \
____________________________________________________________________________________________________\
\
Electronic songbooks (or setlists) are tools used to display 'note sheets' in pdf/image/proprietary formats on your tablet/laptop\
Sophisticated songbooks provide a lot of addional functions like \
- setlist-handling\
- song-depending midi-control for keyboards, e.g switching of 'presets', effects etc\
- network 'band' function (the band zampano can remote-switch the songs on the tablets/laptop of all other band menbers)\
\
Examples: free (simple) apps (download from Microsoft Store):\
- Sheet Music Viewer (Robert Annis): simple 'viewer' for pdf-notesheets: nicely made (song switching by foot pedal does not work)\
- enScore (Jonathan Vardouniotis):   simple 'viewer' for pdf-notesheets with possibilty to add handwritten (touch, mouse) remarks. \
     Setlists as in-app purchase (4 euros)\
- Sheet Music (Bugsbyte): an excellent, stable and 'simplistic' setlist + sheet-viewer app (requires notesheets as gif or png images)\
\
Examples for  (sophisticated) commercial apps (download from Microsoft Store) - can send 'midi program changes' to switch XW presets:\
- Mobile Sheets: workhouse, can send and receive midi for song/preset switching, has Wifi 'group' sync\
- Song Repertoire: close to MobileSheets but with 'modern' user interface, can send midi\
\
\
Virtual instruments / VST \
____________________________________________________________________________________________________\
\
You can connect your XW by MIDI to a PC, tablet or smartphone for playing 'virtual instruments' (pianos, organs, synthesizers etc etc).\
\
A 'virtual instrument' can be a 'standalone instrument', a 'SF2'- or 'VST'-plugin etc...\
\
- Standalone instruments:\
   A 'single instrument' that installs on the computer as a program. Thousands of freeware or payed standalones are available in the Web\
\
- VST:\
   VST is a standard from 'Steinberg' for 'virtual instrument plugins'. To run VST plugins, a 'VST host' software is needed to 'plug them in'.\
   A very good (freeware) host that does 'only hosting'  is 'VST HOST' from Mr. Hermann Seib.\
   You also can use DAWs to load VST plugins (see next chapter).\
\
- Soundfont (SF2) instruments:\
   'SF2' is a worldwide standard for instrument samples. To run SF2-instruments a SF2-player is needed (most common: Sforzando)\
   Thousands of freeware or payed SF2-instruments available on the web\
\
- Latency on MS Windows:\
   native Windows sound drivers have lot of 'latency': the 'output' of a sound is delayed to key-press, making live-playing difficult.\
   ASIO is a standard for latency-free sound drivers: either you buy a high spec ASIO compatible sound card or \
   install the 'generic' ASIO4ALL driver package on your PC (google on web).\
   Note: Your virtual instrument or VST-host must be capable of using ASIO (ASIO4ALL)\
   Note: Magix Music Maker uses a proprietary (latency-free) ASIO driver\
\
- Soundcard:\
   - 'headphone'-jacks or 'line-outs' of most Windos-Laptops do not have good audio quality.\
   - modern 'mini' usb soundcards can be a big improvment at low costs (10-20 euros/USD)\
      Example: Axagon ADA-17: much better sound than builtin audio, works also with smartphones, has ins for Electret stereo microphone\
   - a step further are highgrade soundcards/audio-interfaces (not necessary for playing sound 'live')\
\
- Examples of virtual instruments:\
   tonewheel organs:  GSi 'VB3/VB3-II' (modelling, OS of Crumar Mojo), IK-Multimedia 'B3X' (sampling), GG-Audio 'Blue3'\
   pipe organ:  GrandOrgue (free), jOrgan (free),  Hauptwerk\
   acoustic pianos:  Sforzando (SF2-player) + SalamanderGrandPiano (free), Pianoteq\
\
\
VST Hosts and DAWs \
____________________________________________________________________________________________________\
\
VST-hosts is a class of software that does 'host' vst instruments and  effects plugins.\
Any modern DAW (Digital Audio Workstations) can do vst-hosting.\
The function stack of DAWs is quite simliar, the differences are more in the GUI and 'workflow'. \
\
Examples (freeware):\
\
- 'VSTHost' (by Hermann Seib): fast starting 'pure' VST host for loading VST instruments and effects.\
   Graphical 'chaining' of VST plugins. ASIO/ASIO4ALL compatible.\
   Perfect for 'just playing' VST instruments and FX on the XW.\
   If you only want to use single VST instruments and start them like 'standalones', look at 'SAVIHost'.\
- LMMS: Opensource DAW created for Linux and ported to Windows. FX Studio lookalike, fast, stable, but NOT ASIO COMPATIBLE\
- Podium Free (by Zynewave): 'old' (2014) free-version of Podium but still very usable. \
- Reaper (Trial): 60 days 'expiry' - but continues to run ;) \
- Cakewalk by Bandlab: no limitations, requires registration\
- Waveform free (by Tracktion): no limitations, requires registration\
- Magix Music Maker: very nice features, esp. the midi editor - but brings back Windows 3.2 times: crashes every 30 seconds\
\
NOTE: CTRLR EDITOR can also run as a VST plugin - see Quick-Guide 'INTRO'\
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
-- Alert Window handler
--
local function infoTWindow(infoid)
	local iWindow, winret

	if not info[infoid] then utils.infoWindow("INFO", "sorry, no Quick-Guide available") ; do return end ; end

	iWindow	= AlertWindow(info[infoid].title..":", "", AlertWindow.InfoIcon)
	iWindow:addButton("Close", 0, KeyPress(KeyPress.escapeKey), KeyPress())

	if info[infoid].imgT then		-- text + image at TOP
		for k,comp in ipairs(info[infoid].imgT) do if panel:getComponent(comp) then iWindow:addCustomComponent(panel:getComponent(comp)) end;end
		iWindow:addTextBlock(info[infoid].desc..info.FILL..info.FILL..info.FILL)
	elseif info[infoid].imgB then	-- text + image at BOTTOM
		iWindow:addTextBlock(info[infoid].desc)
		for k,comp in ipairs(info[infoid].imgB) do if panel:getComponent(comp) then iWindow:addCustomComponent(panel:getComponent(comp)) end;end
		iWindow:addTextBlock(info.FILL)
	else							-- text without image
		iWindow:addTextBlock(info[infoid].desc..info.FILL..info.FILL)
	end

	winret 	= iWindow:runModalLoop()
	if winret == 0 then iWindow:setVisible (false) end  	-- cancel
end




--[[
##############################################################################################################
####  MAIN  
##############################################################################################################
--]]
--
-- MAIN
--
INFOQHandler = function(mod,value)
	if not isPanelReady() or not value then do return end ; end
	if panel:getPanelEditor():getPropertyInt("uiPanelEditMode") == 1 then do return end ; end 	-- do not switch layers in edit mode

	local thisMod 		= L(mod:getName())			-- get modulator name
	local QHComplexMods = {}

	if 		QHComplexMods[thisMod] 		then QHComplexMods[thisMod]()
	else	infoTWindow(thisMod)
	end
end
