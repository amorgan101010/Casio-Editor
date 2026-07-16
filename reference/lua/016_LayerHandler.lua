-- CTRLR method: LayerHandler
-- extracted from CasioXW-EDITOR-v0101.bpanelz (author: franky)

--
-- LayerHandler: handle Layers ;)
--

local setModNameVal	= setModNameVal


LayerHandler = function(mod, value)
	if panel:getPanelEditor():getPropertyInt("uiPanelEditMode") == 1 then do return end ; end 	-- do not switch layers in edit mode
	if not isPanelReady() then do return end ; end

	local thisMod = mod:getProperty("name")											-- get modulator name
	if not thisMod then do return end ; end

	local canvas	= panel:getCanvas()
	local layers	= { MAIN=1, SYN=0, MIX=0, ORG=0, CFG=0}				-- { MAIN=1, MENU=1, SYNTH=0, MIXER=0, ORGAN=0, CFG=0}
	local ENVCanvas

	if thisMod == "LayMIXsw"  then
	 	layers.MIX		= 1
		for l,v in pairs(layers) do 	canvas:getLayerByName(l):setPropertyInt("uiPanelCanvasLayerVisibility", v) end
		setModNameVal("LayMIXsw",  1)
		setModNameVal("LaySYNsw",  0)
		setModNameVal("LayORGsw",  0)
		setModNameVal("LayCFGsw",  0)

   elseif thisMod == "LaySYNsw"  then
	 	layers.SYN		= 1
		for l,v in pairs(layers) do 	canvas:getLayerByName(l):setPropertyInt("uiPanelCanvasLayerVisibility", v) end
		setModNameVal("LayMIXsw",  0)
		setModNameVal("LaySYNsw",  1)
		setModNameVal("LayORGsw",  0)
		setModNameVal("LayCFGsw",  0)

   elseif thisMod == "LayORGsw"  then
	 	layers.ORG		= 1
		for l,v in pairs(layers) do 	canvas:getLayerByName(l):setPropertyInt("uiPanelCanvasLayerVisibility", v) end
		setModNameVal("LayMIXsw",  0)
		setModNameVal("LaySYNsw",  0)
		setModNameVal("LayORGsw",  1)
		setModNameVal("LayCFGsw",  0)

	elseif thisMod == "LayCFGsw" then
	 	layers.CFG		= 1
		for l,v in pairs(layers) do 	canvas:getLayerByName(l):setPropertyInt("uiPanelCanvasLayerVisibility", v) end
		setModNameVal("LayMIXsw",  0)
		setModNameVal("LaySYNsw",  0)
		setModNameVal("LayORGsw",  0)
		setModNameVal("LayCFGsw",  1)

	elseif thisMod == "LayVKEYsw"  then
 		canvas:getLayerByName("VKEY"):setPropertyInt("uiPanelCanvasLayerVisibility", value)

	elseif thisMod == "LayVKEYClose"  then
		setModNameVal("LayVKEYsw",  0, true)

	end
end
