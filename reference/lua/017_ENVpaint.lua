-- CTRLR method: ENVpaint
-- extracted from CasioXW-EDITOR-v0101.bpanelz (author: franky)

--
-- ENVPaint: draw the ADSRR graphs
--


local function drawENV(gobject, os, line, spcol, epcol)

	local l 	= {}
	local scale	= {x=0.45, y=0.39}			-- plot canvas: 176 56 272 60
	local lcol	= 0x6666ff					-- line colour

	l.x1=(line.x1 - os.x)*scale.x+4
	l.x2=(line.x2 - os.x)*scale.x+4
	l.y1=(140 - line.y1 + os.y)*scale.y
	l.y2=(140 - line.y2 + os.y)*scale.y

	if l.x2 >= l.x1 then
		gobject:setColour (Colour(lcol):withAlpha (1.0))  ; 	gobject:drawLine(l.x1, l.y1, l.x2, l.y2, 3)		-- drawLine (x1, y1, x2, y2, thickness)
		gobject:setColour (Colour(spcol):withAlpha (1.0)) ;		gobject:fillEllipse (l.x1-4, l.y1-4, 8, 8)		-- draw start point
		gobject:setColour (Colour(epcol):withAlpha (1.0)) ; 	gobject:fillEllipse (l.x2-4, l.y2-4, 8, 8)		-- draw end point
	end
end


--
-- MAIN Zone Canvas component 'repaint' 
--
ENVpaint = function(comp,gobj)
	if not isPanelReady() then do return end ; end

	local canvMod = comp:getProperty("componentVisibleName")				-- find better workaround
	local e = g_CANVdata[canvMod]											-- local short links to global graph data
	local g	= {o={}, a={}, d={}, s={}, r1={}, r2={} }							-- line-point array
	local bgcol	= 0xff111133
	local lcol	= 0x6666ff
	local red	= 0xff0000
	local yel	= 0xffff00
	local org	= 0xf9a602
	local blu	= 0xaaaaff
	local grn	= 0x00ff00

	-- local canvasw = panel:getComponent("KZNCanvas"):getWidth() ; local canvash = panel:getComponent("KZNCanvas"):getHeight()

	gobj:setImageResamplingQuality(Graphics.highResamplingQuality)		-- ??	gobj:setImageResamplingQuality (Graphics.lowResamplingQuality)

	-- default offset:
	g.o.x=0; g.o.y=0

	-- set graph points:
	g.a.x1 =0 		; g.a.x2 =e.aT			; g.a.y1 =e.iL	; g.a.y2 =e.aL	;
	g.d.x1 =g.a.x2	; g.d.x2 =g.a.x2+e.dT	; g.d.y1 =e.aL	; g.d.y2 =e.sL	;
	g.s.x1 =g.d.x2	; g.s.x2 =g.d.x2+75		; g.s.y1 =e.sL	; g.s.y2 =e.sL	;
	g.r1.x1=g.s.x2	; g.r1.x2=g.s.x2+e.r1T	; g.r1.y1=e.sL	; g.r1.y2=e.r1L	;
	g.r2.x1=g.r1.x2	; g.r2.x2=g.r1.x2+e.r2T	; g.r2.y1=e.r1L ; g.r2.y2=e.r2L	;

	-- draw lines:
	gobj:fillAll(Colour(bgcol))

	if canvMod:match("PENV") then g.o.y=-64 ;	gobj:setColour (Colour(lcol):withAlpha (0.8)) ; gobj:drawLine(0, 30, 272, 30, 1) 	-- draw y=0 line
	else 										gobj:setColour (Colour(lcol):withAlpha (0.8)) ; gobj:drawLine(0, 55, 272, 55, 1) 	-- draw y=0 line
	end

	drawENV(gobj, g.o, g.a,  yel, red )
	drawENV(gobj, g.o, g.d,  red, org)
	drawENV(gobj, g.o, g.s,  org, org)
	drawENV(gobj, g.o, g.r1, org, grn)
	drawENV(gobj, g.o, g.r2, grn, yel)
end
