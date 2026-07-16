-- Concatenated Lua from CasioXW-EDITOR-v0101.bpanelz (author: franky)
-- 25 CTRLR methods


-- ================================================================
-- METHOD 000: table2file  (4409 bytes)
-- ================================================================

--[[
   Save Table to File
   Load Table from File
   v 1.0
   
   Lua 5.2 compatible
   
   Only Saves Tables, Numbers and Strings
   Insides Table References are saved
   Does not save Userdata, Metatables, Functions and indices of these
   ----------------------------------------------------
   table.save( table , filename )
   
   on failure: returns an error msg
   
   ----------------------------------------------------
   table.load( filename or stringtable )
   
   Loads a table that has been saved via the table.save function
   
   on success: returns a previously saved table
   on failure: returns as second argument an error msg
   ----------------------------------------------------
   
   Licensed under the same terms as Lua itself.
]]--
do
   -- declare local variables
   --// exportstring( string )
   --// returns a "Lua" portable version of the string
   local function exportstring( s )
      return string.format("%q", s)
   end

   --// The Save Function
   function table.save(  tbl,filename )

      local charS,charE = "   ","\n"
      local file,err = io.open( filename, "wb" )
      if err then return err end

      -- initiate variables for save procedure
      local tables,lookup = { tbl },{ [tbl] = 1 }
      file:write( "return {"..charE )

      for idx,t in ipairs( tables ) do
         file:write( "-- Table: {"..idx.."}"..charE )
         file:write( "{"..charE )
         local thandled = {}

         for i,v in ipairs( t ) do
            thandled[i] = true
            local stype = type( v )
            -- only handle value
            if stype == "table" then
               if not lookup[v] then
                  table.insert( tables, v )
                  lookup[v] = #tables
               end
               file:write( charS.."{"..lookup[v].."},"..charE )
            elseif stype == "string" then
               file:write(  charS..exportstring( v )..","..charE )
            elseif stype == "number" then
               file:write(  charS..tostring( v )..","..charE )
            end
         end

         for i,v in pairs( t ) do
            -- escape handled values
            if (not thandled[i]) then
            
               local str = ""
               local stype = type( i )
               -- handle index
               if stype == "table" then
                  if not lookup[i] then
                     table.insert( tables,i )
                     lookup[i] = #tables
                  end
                  str = charS.."[{"..lookup[i].."}]="
               elseif stype == "string" then
                  str = charS.."["..exportstring( i ).."]="
               elseif stype == "number" then
                  str = charS.."["..tostring( i ).."]="
               end
            
               if str ~= "" then
                  stype = type( v )
                  -- handle value
                  if stype == "table" then
                     if not lookup[v] then
                        table.insert( tables,v )
                        lookup[v] = #tables
                     end
                     file:write( str.."{"..lookup[v].."},"..charE )
                  elseif stype == "string" then
                     file:write( str..exportstring( v )..","..charE )
                  elseif stype == "number" then
                     file:write( str..tostring( v )..","..charE )
                  end
               end
            end
         end
         file:write( "},"..charE )
      end
      file:write( "}" )
      file:close()
   end
   
   --// The Load Function
   function table.load( sfile )
      local ftables,err = loadfile( sfile )
      if err then return _,err end
      local tables = ftables()
      for idx = 1,#tables do
         local tolinki = {}
         for i,v in pairs( tables[idx] ) do
            if type( v ) == "table" then
               tables[idx][i] = tables[v[1]]
            end
            if type( i ) == "table" and tables[i[1]] then
               table.insert( tolinki,{ i,tables[i[1]] } )
            end
         end
         -- link indices
         for _,v in ipairs( tolinki ) do
            tables[idx][v[2]],tables[idx][v[1]] =  tables[idx][v[1]],nil
         end
      end
      return tables[1]
   end
-- close do
end

-- ChillCode


-- ================================================================
-- METHOD 001: table_dump  (278 bytes)
-- ================================================================

-- @1.1
--
-- Print table contents
--
function table_dump(table)
	for key,value in ipairs(table) do
		_DBG ("KEY= ["..key.."]")

		if (type(value) == "table") then
			table_dump(value)
		elseif (type(value) == "nil") then
			_DBG (" = NIL")
		else
			what (value)
		end
	end
end

-- ================================================================
-- METHOD 002: what  (657 bytes)
-- ================================================================

-- @1.1
--
-- Print methods for an object
--
function what(o)
	info = class_info(o)
	if info ~= nil then
		ret = "Object type [" .. info.name .. "]\n-----------------------------------------------------------------\n\n".."Members:\n"

		if info.name == "table" then
			table_dump(o)
		end

		for k, v in pairs(info.methods) do
			ret = ret .. string.format ("\t%30s:\t%5s\n", k, type(v))
		end
		ret = ret .. "\n\nAttributes:\n"
		for k, v in pairs(info.attributes) do
			ret = ret .. string.format ("\t%30s:\t%5s\n", k, type(v))
		end
		ret = ret .. "\n-----------------------------------------------------------------"
	end

	console (ret)
	return ret
end

-- ================================================================
-- METHOD 003: how  (365 bytes)
-- ================================================================

-- @1.1
--
-- Print all available classes
--
function how()
	ret = "Available classes:\n"
	ret = ret .. "\n-----------------------------------------------------------------"
	for i,v in ipairs(class_names()) do
		ret = ret .. "\t".. v .. "\n"
	end
	ret = ret .. "\n-----------------------------------------------------------------"
	console (J(ret))
	return ret
end

-- ================================================================
-- METHOD 004: json4lua  (17170 bytes)
-- ================================================================

-----------------------------------------------------------------------------
-- JSON4Lua: JSON encoding / decoding support for the Lua language.
-- json Module.
-- Author: Craig Mason-Jones
-- Homepage: http://json.luaforge.net/
-- Version: 0.9.40
-- This module is released under the MIT License (MIT).
-- Please see LICENCE.txt for details.
--
-- USAGE:
-- This module exposes two functions:
--   encode(o)
--     Returns the table / string / boolean / number / nil / json.null value as a JSON-encoded string.
--   decode(json_string)
--     Returns a Lua object populated with the data encoded in the JSON string json_string.
--
-- REQUIREMENTS:
--   compat-5.1 if using Lua 5.0
--
-- CHANGELOG
--   0.9.20 Introduction of local Lua functions for private functions (removed _ function prefix). 
--          Fixed Lua 5.1 compatibility issues.
--   		Introduced json.null to have null values in associative arrays.
--          encode() performance improvement (more than 50%) through table.concat rather than ..
--          Introduced decode ability to ignore /**/ comments in the JSON string.
--   0.9.10 Fix to array encoding / decoding to correctly manage nil/null values in arrays.
-----------------------------------------------------------------------------

-----------------------------------------------------------------------------
-- Imports and dependencies
-----------------------------------------------------------------------------
local math = require('math')
local string = require("string")
local table = require("table")

local base = _G

-----------------------------------------------------------------------------
-- Module declaration
-----------------------------------------------------------------------------
module("json")

-- Public functions

-- Private functions
local decode_scanArray
local decode_scanComment
local decode_scanConstant
local decode_scanNumber
local decode_scanObject
local decode_scanString
local decode_scanWhitespace
local encodeString
local isArray
local isEncodable

-----------------------------------------------------------------------------
-- PUBLIC FUNCTIONS
-----------------------------------------------------------------------------
--- Encodes an arbitrary Lua object / variable.
-- @param v The Lua object / variable to be JSON encoded.
-- @return String containing the JSON encoding in internal Lua string format (i.e. not unicode)
function encode (v)
  -- Handle nil values
  if v==nil then
    return "null"
  end
  
  local vtype = base.type(v)  

  -- Handle strings
  if vtype=='string' then    
    return '"' .. encodeString(v) .. '"'	    -- Need to handle encoding in string
  end
  
  -- Handle booleans
  if vtype=='number' or vtype=='boolean' then
    return base.tostring(v)
  end
  
  -- Handle tables
  if vtype=='table' then
    local rval = {}
    -- Consider arrays separately
    local bArray, maxCount = isArray(v)
    if bArray then
      for i = 1,maxCount do
        table.insert(rval, encode(v[i]))
      end
    else	-- An object, not an array
      for i,j in base.pairs(v) do
        if isEncodable(i) and isEncodable(j) then
          table.insert(rval, '"' .. encodeString(i) .. '":' .. encode(j))
        end
      end
    end
    if bArray then
      return '[' .. table.concat(rval,',') ..']'
    else
      return '{' .. table.concat(rval,',') .. '}'
    end
  end
  
  -- Handle null values
  if vtype=='function' and v==null then
    return 'null'
  end
  
  base.assert(false,'encode attempt to encode unsupported type ' .. vtype .. ':' .. base.tostring(v))
end


--- Decodes a JSON string and returns the decoded value as a Lua data structure / value.
-- @param s The string to scan.
-- @param [startPos] Optional starting position where the JSON string is located. Defaults to 1.
-- @param Lua object, number The object that was scanned, as a Lua table / string / number / boolean or nil,
-- and the position of the first character after
-- the scanned JSON object.
function decode(s, startPos)
  startPos = startPos and startPos or 1
  startPos = decode_scanWhitespace(s,startPos)
  base.assert(startPos<=string.len(s), 'Unterminated JSON encoded object found at position in [' .. s .. ']')
  local curChar = string.sub(s,startPos,startPos)
  -- Object
  if curChar=='{' then
    return decode_scanObject(s,startPos)
  end
  -- Array
  if curChar=='[' then
    return decode_scanArray(s,startPos)
  end
  -- Number
  if string.find("+-0123456789.e", curChar, 1, true) then
    return decode_scanNumber(s,startPos)
  end
  -- String
  if curChar==[["]] or curChar==[[']] then
    return decode_scanString(s,startPos)
  end
  if string.sub(s,startPos,startPos+1)=='/*' then
    return decode(s, decode_scanComment(s,startPos))
  end
  -- Otherwise, it must be a constant
  return decode_scanConstant(s,startPos)
end

--- The null function allows one to specify a null value in an associative array (which is otherwise
-- discarded if you set the value with 'nil' in Lua. Simply set t = { first=json.null }
function null()
  return null -- so json.null() will also return null ;-)
end
-----------------------------------------------------------------------------
-- Internal, PRIVATE functions.
-- Following a Python-like convention, I have prefixed all these 'PRIVATE'
-- functions with an underscore.
-----------------------------------------------------------------------------

--- Scans an array from JSON into a Lua object
-- startPos begins at the start of the array.
-- Returns the array and the next starting position
-- @param s The string being scanned.
-- @param startPos The starting position for the scan.
-- @return table, int The scanned array as a table, and the position of the next character to scan.
function decode_scanArray(s,startPos)
  local array = {}	-- The return value
  local stringLen = string.len(s)
  base.assert(string.sub(s,startPos,startPos)=='[','decode_scanArray called but array does not start at position ' .. startPos .. ' in string:\n'..s )
  startPos = startPos + 1
  -- Infinite loop for array elements
  repeat
    startPos = decode_scanWhitespace(s,startPos)
    base.assert(startPos<=stringLen,'JSON String ended unexpectedly scanning array.')
    local curChar = string.sub(s,startPos,startPos)
    if (curChar==']') then
      return array, startPos+1
    end
    if (curChar==',') then
      startPos = decode_scanWhitespace(s,startPos+1)
    end
    base.assert(startPos<=stringLen, 'JSON String ended unexpectedly scanning array.')
    object, startPos = decode(s,startPos)
    table.insert(array,object)
  until false
end

--- Scans a comment and discards the comment.
-- Returns the position of the next character following the comment.
-- @param string s The JSON string to scan.
-- @param int startPos The starting position of the comment
function decode_scanComment(s, startPos)
  base.assert( string.sub(s,startPos,startPos+1)=='/*', "decode_scanComment called but comment does not start at position " .. startPos)
  local endPos = string.find(s,'*/',startPos+2)
  base.assert(endPos~=nil, "Unterminated comment in string at " .. startPos)
  return endPos+2  
end

--- Scans for given constants: true, false or null
-- Returns the appropriate Lua type, and the position of the next character to read.
-- @param s The string being scanned.
-- @param startPos The position in the string at which to start scanning.
-- @return object, int The object (true, false or nil) and the position at which the next character should be 
-- scanned.
function decode_scanConstant(s, startPos)
  local consts = { ["true"] = true, ["false"] = false, ["null"] = nil }
  local constNames = {"true","false","null"}

  for i,k in base.pairs(constNames) do
    --print ("[" .. string.sub(s,startPos, startPos + string.len(k) -1) .."]", k)
    if string.sub(s,startPos, startPos + string.len(k) -1 )==k then
      return consts[k], startPos + string.len(k)
    end
  end
  base.assert(nil, 'Failed to scan constant from string ' .. s .. ' at starting position ' .. startPos)
end

--- Scans a number from the JSON encoded string.
-- (in fact, also is able to scan numeric +- eqns, which is not
-- in the JSON spec.)
-- Returns the number, and the position of the next character
-- after the number.
-- @param s The string being scanned.
-- @param startPos The position at which to start scanning.
-- @return number, int The extracted number and the position of the next character to scan.
function decode_scanNumber(s,startPos)
  local endPos = startPos+1
  local stringLen = string.len(s)
  local acceptableChars = "+-0123456789.e"
  while (string.find(acceptableChars, string.sub(s,endPos,endPos), 1, true)
	and endPos<=stringLen
	) do
    endPos = endPos + 1
  end
  local stringValue = 'return ' .. string.sub(s,startPos, endPos-1)
  local stringEval = base.loadstring(stringValue)
  base.assert(stringEval, 'Failed to scan number [ ' .. stringValue .. '] in JSON string at position ' .. startPos .. ' : ' .. endPos)
  return stringEval(), endPos
end

--- Scans a JSON object into a Lua object.
-- startPos begins at the start of the object.
-- Returns the object and the next starting position.
-- @param s The string being scanned.
-- @param startPos The starting position of the scan.
-- @return table, int The scanned object as a table and the position of the next character to scan.
function decode_scanObject(s,startPos)
  local object = {}
  local stringLen = string.len(s)
  local key, value
  base.assert(string.sub(s,startPos,startPos)=='{','decode_scanObject called but object does not start at position ' .. startPos .. ' in string:\n' .. s)
  startPos = startPos + 1
  repeat
    startPos = decode_scanWhitespace(s,startPos)
    base.assert(startPos<=stringLen, 'JSON string ended unexpectedly while scanning object.')
    local curChar = string.sub(s,startPos,startPos)
    if (curChar=='}') then
      return object,startPos+1
    end
    if (curChar==',') then
      startPos = decode_scanWhitespace(s,startPos+1)
    end
    base.assert(startPos<=stringLen, 'JSON string ended unexpectedly scanning object.')
    -- Scan the key
    key, startPos = decode(s,startPos)
    base.assert(startPos<=stringLen, 'JSON string ended unexpectedly searching for value of key ' .. key)
    startPos = decode_scanWhitespace(s,startPos)
    base.assert(startPos<=stringLen, 'JSON string ended unexpectedly searching for value of key ' .. key)
    base.assert(string.sub(s,startPos,startPos)==':','JSON object key-value assignment mal-formed at ' .. startPos)
    startPos = decode_scanWhitespace(s,startPos+1)
    base.assert(startPos<=stringLen, 'JSON string ended unexpectedly searching for value of key ' .. key)
    value, startPos = decode(s,startPos)
    object[key]=value
  until false	-- infinite loop while key-value pairs are found
end

-- START SoniEx2
-- Initialize some things used by decode_scanString
-- You know, for efficiency
local escapeSequences = {
  ["\\t"] = "\t",
  ["\\f"] = "\f",
  ["\\r"] = "\r",
  ["\\n"] = "\n",
  ["\\b"] = "\b"
}
base.setmetatable(escapeSequences, {__index = function(t,k)
  -- skip "\" aka strip escape
  return string.sub(k,2)
end})
-- END SoniEx2

--- Scans a JSON string from the opening inverted comma or single quote to the
-- end of the string.
-- Returns the string extracted as a Lua string,
-- and the position of the next non-string character
-- (after the closing inverted comma or single quote).
-- @param s The string being scanned.
-- @param startPos The starting position of the scan.
-- @return string, int The extracted string as a Lua string, and the next character to parse.
function decode_scanString(s,startPos)
  base.assert(startPos, 'decode_scanString(..) called without start position')
  local startChar = string.sub(s,startPos,startPos)
  -- START SoniEx2
  -- PS: I don't think single quotes are valid JSON
  base.assert(startChar == [["]] or startChar == [[']],'decode_scanString called for a non-string')
  --base.assert(startPos, "String decoding failed: missing closing " .. startChar .. " for string at position " .. oldStart)
  local t = {}
  local i,j = startPos,startPos
  while string.find(s, startChar, j+1) ~= j+1 do
    local oldj = j
    i,j = string.find(s, "\\.", j+1)
    local x,y = string.find(s, startChar, oldj+1)
    if not i or x < i then
      base.print(s, startPos, string.sub(s,startPos,oldj))
      i,j = x,y-1
      if not x then base.print(s, startPos, string.sub(s,startPos,oldj)) end
    end
    table.insert(t, string.sub(s, oldj+1, i-1))
    if string.sub(s, i, j) == "\\u" then
      local a = string.sub(s,j+1,j+4)
      j = j + 4
      local n = base.tonumber(a, 16)
      base.assert(n, "String decoding failed: bad Unicode escape " .. a .. " at position " .. i .. " : " .. j)
      -- math.floor(x/2^y) == lazy right shift
      -- a % 2^b == bitwise_and(a, (2^b)-1)
      -- 64 = 2^6
      -- 4096 = 2^12 (or 2^6 * 2^6)
      local x
      if n < 0x80 then
        x = string.char(n % 0x80)
      elseif n < 0x800 then
        -- [110x xxxx] [10xx xxxx]
        x = string.char(0xC0 + (math.floor(n/64) % 0x20), 0x80 + (n % 0x40))
      else
        -- [1110 xxxx] [10xx xxxx] [10xx xxxx]
        x = string.char(0xE0 + (math.floor(n/4096) % 0x10), 0x80 + (math.floor(n/64) % 0x40), 0x80 + (n % 0x40))
      end
      table.insert(t, x)
    else
      table.insert(t, escapeSequences[string.sub(s, i, j)])
    end
  end
  table.insert(t,string.sub(j, j+1))
  base.assert(string.find(s, startChar, j+1), "String decoding failed: missing closing " .. startChar .. " at position " .. j .. "(for string at position " .. startPos .. ")")
  return table.concat(t,""), j+2
  -- END SoniEx2
end

--- Scans a JSON string skipping all whitespace from the current start position.
-- Returns the position of the first non-whitespace character, or nil if the whole end of string is reached.
-- @param s The string being scanned
-- @param startPos The starting position where we should begin removing whitespace.
-- @return int The first position where non-whitespace was encountered, or string.len(s)+1 if the end of string
-- was reached.
function decode_scanWhitespace(s,startPos)
  local whitespace=" \n\r\t"
  local stringLen = string.len(s)
  while ( string.find(whitespace, string.sub(s,startPos,startPos), 1, true)  and startPos <= stringLen) do
    startPos = startPos + 1
  end
  return startPos
end

--- Encodes a string to be JSON-compatible.
-- This just involves back-quoting inverted commas, back-quotes and newlines, I think ;-)
-- @param s The string to return as a JSON encoded (i.e. backquoted string)
-- @return The string appropriately escaped.

local escapeList = {
    ['"']  = '\\"',
    ['\\'] = '\\\\',
    ['/']  = '\\/', 
    ['\b'] = '\\b',
    ['\f'] = '\\f',
    ['\n'] = '\\n',
    ['\r'] = '\\r',
    ['\t'] = '\\t'
}

function encodeString(s)
 return s:gsub(".", function(c) return escapeList[c] end) -- SoniEx2: 5.0 compat
end

-- Determines whether the given Lua type is an array or a table / dictionary.
-- We consider any table an array if it has indexes 1..n for its n items, and no
-- other data in the table.
-- I think this method is currently a little 'flaky', but can't think of a good way around it yet...
-- @param t The table to evaluate as an array
-- @return boolean, number True if the table can be represented as an array, false otherwise. If true,
-- the second returned value is the maximum
-- number of indexed elements in the array. 
function isArray(t)
  -- Next we count all the elements, ensuring that any non-indexed elements are not-encodable 
  -- (with the possible exception of 'n')
  local maxIndex = 0
  for k,v in base.pairs(t) do
    if (base.type(k)=='number' and math.floor(k)==k and 1<=k) then	-- k,v is an indexed pair
      if (not isEncodable(v)) then return false end	-- All array elements must be encodable
      maxIndex = math.max(maxIndex,k)
    else
      if (k=='n') then
        if v ~= table.getn(t) then return false end  -- False if n does not hold the number of elements
      else -- Else of (k=='n')
        if isEncodable(v) then return false end
      end  -- End of (k~='n')
    end -- End of k,v not an indexed pair
  end  -- End of loop across all pairs
  return true, maxIndex
end

--- Determines whether the given Lua object / table / variable can be JSON encoded. The only
-- types that are JSON encodable are: string, boolean, number, nil, table and json.null.
-- In this implementation, all other types are ignored.
-- @param o The object to examine.
-- @return boolean True if the object should be JSON encoded, false if it should be ignored.
function isEncodable(o)
  local t = base.type(o)
  return (t=='string' or t=='boolean' or t=='number' or t=='nil' or t=='table') or (t=='function' and o==null) 
end

-- ================================================================
-- METHOD 005: debugger.lua  (43369 bytes)
-- ================================================================

--{{{  history

--15/03/06 DCN Created based on RemDebug
--28/04/06 DCN Update for Lua 5.1
--01/06/06 DCN Fix command argument parsing
--             Add step/over N facility
--             Add trace lines facility
--05/06/06 DCN Add trace call/return facility
--06/06/06 DCN Make it behave when stepping through the creation of a coroutine
--06/06/06 DCN Integrate the simple debugger into the main one
--07/06/06 DCN Provide facility to step into coroutines
--13/06/06 DCN Fix bug that caused the function environment to get corrupted with the global one
--14/06/06 DCN Allow 'sloppy' file names when setting breakpoints
--04/08/06 DCN Allow for no space after command name
--11/08/06 DCN Use io.write not print
--30/08/06 DCN Allow access to array elements in 'dump'
--10/10/06 DCN Default to breakfile for all commands that require a filename and give '-'
--06/12/06 DCN Allow for punctuation characters in DUMP variable names
--03/01/07 DCN Add pause on/off facility
--19/06/07 DCN Allow for duff commands being typed in the debugger (thanks to Michael.Bringmann@lsi.com)
--             Allow for case sensitive file systems               (thanks to Michael.Bringmann@lsi.com)
--04/08/09 DCN Add optional line count param to pause
--05/08/09 DCN Reset the debug hook in Pause() even if we think we're started
--30/09/09 DCN Re-jig to not use co-routines (makes debugging co-routines awkward)
--01/10/09 DCN Add ability to break on reaching any line in a file
--24/07/13 TWW Added code for emulating setfenv/getfenv in Lua 5.2 as per
--             http://lua-users.org/lists/lua-l/2010-06/msg00313.html
--25/07/13 TWW Copied Alex Parrill's fix for errors when tracing back across a C frame
--             (https://github.com/ColonelThirtyTwo/clidebugger, 26/01/12)
--25/07/13 DCN Allow for windows and unix file name conventions in has_breakpoint
--26/07/13 DCN Allow for \ being interpreted as an escape inside a [] pattern in 5.2

--}}}
--{{{  description

--A simple command line debug system for Lua written by Dave Nichols of
--Match-IT Limited. Its public domain software. Do with it as you wish.

--This debugger was inspired by:
-- RemDebug 1.0 Beta
-- Copyright Kepler Project 2005 (http://www.keplerproject.org/remdebug)

--Usage:
--  require('debugger')        --load the debug library
--  pause(message)             --start/resume a debug session

--An assert() failure will also invoke the debugger.

--}}}

local IsWindows = string.find(string.lower(os.getenv('OS') or ''),'^windows')

local coro_debugger
local events = { BREAK = 1, WATCH = 2, STEP = 3, SET = 4 }
breakpoints = {}
local watches = {}
local step_into   = false
local step_over   = false
local step_lines  = 0
local step_level  = {main=0}
local stack_level = {main=0}
local trace_level = {main=0}
local trace_calls = false
local trace_returns = false
local trace_lines = false
local ret_file, ret_line, ret_name
local current_thread = 'main'
local started = false
local pause_off = false
local _g      = _G
local cocreate, cowrap = coroutine.create, coroutine.wrap
local pausemsg = 'pause'

--{{{  make Lua 5.2 compatible

if not setfenv then -- Lua 5.2
  --[[
  As far as I can see, the only missing detail of these functions (except
  for occasional bugs) to achieve 100% compatibility is the case of
  'getfenv' over a function that does not have an _ENV variable (that is,
  it uses no globals).

  We could use a weak table to keep the environments of these functions
  when set by setfenv, but that still misses the case of a function
  without _ENV that was not subjected to setfenv.

  -- Roberto
  ]]--

  setfenv = setfenv or function(f, t)
    f = (type(f) == 'function' and f or debug.getinfo(f + 1, 'f').func)
    local name
    local up = 0
    repeat
      up = up + 1
      name = debug.getupvalue(f, up)
    until name == '_ENV' or name == nil
    if name then
      debug.upvaluejoin(f, up, function() return name end, 1) -- use unique upvalue
      debug.setupvalue(f, up, t)
    end
  end

  getfenv = getfenv or function(f)
    f = (type(f) == 'function' and f or debug.getinfo(f + 1, 'f').func)
    local name, val
    local up = 0
    repeat
      up = up + 1
      name, val = debug.getupvalue(f, up)
    until name == '_ENV' or name == nil
    return val
  end

end

--}}}

--{{{  local hints -- command help
--The format in here is name=summary|description
local hints = {

pause =   [[
pause(msg[,lines][,force]) -- start/resume a debugger session|

This can only be used in your code or from the console as a means to
start/resume a debug session.
If msg is given that is shown when the session starts/resumes. Useful to
give a context if you've instrumented your code with pause() statements.

If lines is given, the script pauses after that many lines, else it pauses
immediately.

If force is true, the pause function is honoured even if poff has been used.
This is useful when in an interactive console session to regain debugger
control.
]],

poff =    [[
poff                -- turn off pause() command|

This causes all pause() commands to be ignored. This is useful if you have
instrumented your code in a busy loop and want to continue normal execution
with no further interruption.
]],

pon =     [[
pon                 -- turn on pause() command|

This re-instates honouring the pause() commands you may have instrumented
your code with.
]],

setb =    [[
setb [line file]    -- set a breakpoint to line/file|, line 0 means 'any'

If file is omitted or is "-" the breakpoint is set at the file for the
currently set level (see "set"). Execution pauses when this line is about
to be executed and the debugger session is re-activated.

The file can be given as the fully qualified name, partially qualified or
just the file name. E.g. if file is set as "myfile.lua", then whenever
execution reaches any file that ends with "myfile.lua" it will pause. If
no extension is given, any extension will do.

If the line is given as 0, then reaching any line in the file will do.
]],

delb =    [[
delb [line file]    -- removes a breakpoint|

If file is omitted or is "-" the breakpoint is removed for the file of the
currently set level (see "set").
]],

delallb = [[
delallb             -- removes all breakpoints|
]],

setw =    [[
setw <exp>          -- adds a new watch expression|

The expression is evaluated before each line is executed. If the expression
yields true then execution is paused and the debugger session re-activated.
The expression is executed in the context of the line about to be executed.
]],

delw =    [[
delw <index>        -- removes the watch expression at index|

The index is that returned when the watch expression was set by setw.
]],

delallw = [[
delallw             -- removes all watch expressions|
]],

run     = [[
run                 -- run until next breakpoint or watch expression|
]],

step    = [[
step [N]            -- run next N lines, stepping into function calls|

If N is omitted, use 1.
]],

over    = [[
over [N]            -- run next N lines, stepping over function calls|

If N is omitted, use 1.
]],

out     = [[
out [N]             -- run lines until stepped out of N functions|

If N is omitted, use 1.
If you are inside a function, using "out 1" will run until you return
from that function to the caller.
]],

gotoo   = [[
gotoo [line file]    -- step to line in file|

This is equivalent to 'setb line file', followed by 'run', followed
by 'delb line file'.
]],

listb   = [[
listb               -- lists breakpoints|
]],

listw   = [[
listw               -- lists watch expressions|
]],

set     = [[
set [level]         -- set context to stack level, omitted=show|

If level is omitted it just prints the current level set.
This sets the current context to the level given. This affects the
context used for several other functions (e.g. vars). The possible
levels are those shown by trace.
]],

vars    = [[
vars [depth]        -- list context locals to depth, omitted=1|

If depth is omitted then uses 1.
Use a depth of 0 for the maximum.
Lists all non-nil local variables and all non-nil upvalues in the
currently set context. For variables that are tables, lists all fields
to the given depth.
]],

fenv    = [[
fenv [depth]        -- list context function env to depth, omitted=1|

If depth is omitted then uses 1.
Use a depth of 0 for the maximum.
Lists all function environment variables in the currently set context.
For variables that are tables, lists all fields to the given depth.
]],

glob    = [[
glob [depth]        -- list globals to depth, omitted=1|

If depth is omitted then uses 1.
Use a depth of 0 for the maximum.
Lists all global variables.
For variables that are tables, lists all fields to the given depth.
]],

ups     = [[
ups                 -- list all the upvalue names|

These names will also be in the "vars" list unless their value is nil.
This provides a means to identify which vars are upvalues and which are
locals. If a name is both an upvalue and a local, the local value takes
precedance.
]],

locs    = [[
locs                -- list all the locals names|

These names will also be in the "vars" list unless their value is nil.
This provides a means to identify which vars are upvalues and which are
locals. If a name is both an upvalue and a local, the local value takes
precedance.
]],

dump    = [[
dump <var> [depth]  -- dump all fields of variable to depth|

If depth is omitted then uses 1.
Use a depth of 0 for the maximum.
Prints the value of <var> in the currently set context level. If <var>
is a table, lists all fields to the given depth. <var> can be just a
name, or name.field or name.# to any depth, e.g. t.1.f accesses field
'f' in array element 1 in table 't'.

Can also be called from a script as dump(var,depth).
]],

tron    = [[
tron [crl]          -- turn trace on for (c)alls, (r)etuns, (l)lines|

If no parameter is given then tracing is turned off.
When tracing is turned on a line is printed to the console for each
debug 'event' selected. c=function calls, r=function returns, l=lines.
]],

trace   = [[
trace               -- dumps a stack trace|

Format is [level] = file,line,name
The level is a candidate for use by the 'set' command.
]],

info    = [[
info                -- dumps the complete debug info captured|

Only useful as a diagnostic aid for the debugger itself. This information
can be HUGE as it dumps all variables to the maximum depth, so be careful.
]],

show    = [[
show line file X Y  -- show X lines before and Y after line in file|

If line is omitted or is '-' then the current set context line is used.
If file is omitted or is '-' then the current set context file is used.
If file is not fully qualified and cannot be opened as specified, then
a search for the file in the package[path] is performed using the usual
"require" searching rules. If no file extension is given, .lua is used.
Prints the lines from the source file around the given line.
]],

exit    = [[
exit                -- exits debugger, re-start it using pause()|
]],

help    = [[
help [command]      -- show this list or help for command|
]],

["<statement>"] = [[
<statement>         -- execute a statement in the current context|

The statement can be anything that is legal in the context, including
assignments. Such assignments affect the context and will be in force
immediately. Any results returned are printed. Use '=' as a short-hand
for 'return', e.g. "=func(arg)" will call 'func' with 'arg' and print
the results, and "=var" will just print the value of 'var'.
]],

what    = [[
what <func>         -- show where <func> is defined (if known)|
]],

}
--}}}

--{{{ Local function to get table size
local function tsize(t)
    local count=0

    for k,v in pairs(t) do
        count = count + 1
    end

    return count
end
---}}}

---{{{ Global utility function to set breakpoints, used inside Ctrlr
function setBreakpoint(line, file, shouldBeSet)
    if not breakpoints[line] then
        breakpoints[line] = {}
    end

    if shouldBeSet then
        breakpoints[line][file] = true
    else
        breakpoints[line] = nil
    end
end
---}}}


--{{{  local function getinfo(level,field)

--like debug.getinfo but copes with no activation record at the given level
--and knows how to get 'field'. 'field' can be the name of any of the
--activation record fields or any of the 'what' names or nil for everything.
--only valid when using the stack level to get info, not a function name.

local function getinfo(level,field)
  level = level + 1  --to get to the same relative level as the caller
  if not field then return debug.getinfo(level) end
  local what
  if field == 'name' or field == 'namewhat' then
    what = 'n'
  elseif field == 'what' or field == 'source' or field == 'linedefined' or field == 'lastlinedefined' or field == 'short_src' then
    what = 'S'
  elseif field == 'currentline' then
    what = 'l'
  elseif field == 'nups' then
    what = 'u'
  elseif field == 'func' then
    what = 'f'
  else
    return debug.getinfo(level,field)
  end
  local ar = debug.getinfo(level,what)
  if ar then return ar[field] else return nil end
end

--}}}
--{{{  local function indented( level, ... )

local function indented( level, ... )
  ctrlrDebugger:write( string.format ("%s%s\n", string.rep('  ',level), table.concat({...}) ))
end

--}}}
--{{{  local function dumpval( level, name, value, limit )

local dumpvisited

local function dumpval( level, name, value, limit )
    local index

    if type(name) == 'number' then
        index = string.format("%q,",name)
    elseif type(name) == 'string' and (name == '__VARSLEVEL__' or name == '__ENVIRONMENT__' or name == '__GLOBALS__' or name == '__UPVALUES__' or name == '__LOCALS__') then
        --ignore these, they are debugger generated
        return
    elseif type(name) == 'string' and string.find(name,'^[_%a][_.%w]*$') then
        index = string.format ("%q: ", name);
    else
        index = string.format ("%q,", tostring(name))
    end

    if type(value) == 'table' then
        if dumpvisited[value] then
            indented (level, index, string.format("%q", dumpvisited[value]))
        else
            dumpvisited[value] = string.format ("\"table\": \"%d\",", tsize (value))
            if (limit or 0) > 0 and level+1 >= limit then
                indented (level, index, string.format ("{%s", string.gsub(dumpvisited[value], ",", "},")))
            else
                indented (level, index, "{\n", dumpvisited[value])

                for n,v in pairs(value) do
                    dumpval (level+1, n, v, limit)
                end

                indented (level, "}")
            end
        end
    else
        if type(value) == 'string' then
            indented (level, index, string.format("{\"string\": %q}",value), ',')
        end

        if type(value) == 'userdata' then
            info = class_info (value)
            indented (level, index, string.format ("{\"userdata\": %q}", info.name) , ',')
        end

        if type(value) == 'number' then
            indented (level, index, string.format ("{\"number\": %q}",tostring(value)), ',')
        end
    end
end

--}}}
--{{{  local function dumpvar( value, limit, name )

local function dumpvar( value, limit, name )
  ctrlrDebugger:write ("\n::start dumpvar\n")
  dumpvisited = {}
  dumpval( 0, name or tostring(value), value, limit )
  ctrlrDebugger:write ("::end\n")
end

--}}}
--{{{  local function show(file,line,before,after)

--show +/-N lines of a file around line M

local function show(file,line,before,after)

  line   = tonumber(line   or 1)
  before = tonumber(before or 10)
  after  = tonumber(after  or before)

  if not string.find(file,'%.') then file = file..'.lua' end

  local f = io.open(file,'r')
  if not f then
    --{{{  try to find the file in the path

    --
    -- looks for a file in the package path
    --
    local path = package.path or LUA_PATH or ''
    for c in string.gmatch (path, "[^;]+") do
      local c = string.gsub (c, "%?%.lua", file)
      f = io.open (c,'r')
      if f then
        break
      end
    end

    --}}}
    if not f then
      ctrlrDebugger:write('Cannot find '..file..'\n')
      return
    end
  end

  local i = 0
  for l in f:lines() do
    i = i + 1
    if i >= (line-before) then
      if i > (line+after) then break end
      if i == line then
        ctrlrDebugger:write(i..'***\t'..l..'\n')
      else
        ctrlrDebugger:write(i..'\t'..l..'\n')
      end
    end
  end

  f:close()

end

--}}}
--{{{  local function tracestack(l)

local function gi( i )
  return function() i=i+1 return debug.getinfo(i),i end
end

local function gl( level, j )
  return function() j=j+1 return debug.getlocal( level, j ) end
end

local function gu( func, k )
  return function() k=k+1 return debug.getupvalue( func, k ) end
end

local  traceinfo

local function tracestack(l)
  local l = l + 1                        --NB: +1 to get level relative to caller
  traceinfo = {}
  traceinfo.pausemsg = pausemsg
  for ar,i in gi(l) do
    table.insert( traceinfo, ar )
    if ar.what ~= 'C' then
      local names  = {}
      local values = {}
      for n,v in gl(i,0) do
        if string.sub(n,1,1) ~= '(' then   --ignore internal control variables
          table.insert( names, n )
          table.insert( values, v )
        end
      end
      if #names > 0 then
        ar.lnames  = names
        ar.lvalues = values
      end
    end
    if ar.func then
      local names  = {}
      local values = {}
      for n,v in gu(ar.func,0) do
        if string.sub(n,1,1) ~= '(' then   --ignore internal control variables
          table.insert( names, n )
          table.insert( values, v )
        end
      end
      if #names > 0 then
        ar.unames  = names
        ar.uvalues = values
      end
    end
  end
end

--}}}
--{{{  local function trace()

local function trace(set)
  ctrlrDebugger:write ("\n::start trace\n")
  local mark
  for level,ar in ipairs(traceinfo) do
    if level == set then
      mark = '***'
    else
      mark = ''
    end
    ctrlrDebugger:write('['..level..']'..mark..'\t'..(ar.name or ar.what)..' in '..ar.short_src..':'..ar.currentline..'\n')
  end

  ctrlrDebugger:write ("::end\n")
end

--}}}
--{{{  local function info()

local function info()
    dumpvar( traceinfo, 0, 'traceinfo' )
end

--}}}

--{{{  local function set_breakpoint(file, line, once)

local function set_breakpoint(file, line, once)
  if not breakpoints[line] then
    breakpoints[line] = {}
  end
  if once then
    breakpoints[line][file] = 1
  else
    breakpoints[line][file] = true
  end
end

--}}}
--{{{  local function remove_breakpoint(file, line)

local function remove_breakpoint(file, line)
  if breakpoints[line] then
    breakpoints[line][file] = nil
  end
end

--}}}
--{{{  local function has_breakpoint(file, line)

--allow for 'sloppy' file names
--search for file and all variations walking up its directory hierachy
--ditto for the file with no extension
--a breakpoint can be permenant or once only, if once only its removed
--after detection here, these are used for temporary breakpoints in the
--debugger loop when executing the 'gotoo' command
--a breakpoint on line 0 of a file means any line in that file

local function has_breakpoint(file, line)
  local isLine = breakpoints[line]
  local isZero = breakpoints[0]
  if not isLine and not isZero then return false end
  local noext = string.gsub(file,"(%..-)$",'',1)
  if noext == file then noext = nil end
  while file do
    if isLine and isLine[file] then
      if isLine[file] == 1 then isLine[file] = nil end
      return true
    end
    if isZero and isZero[file] then
      if isZero[file] == 1 then isZero[file] = nil end
      return true
    end
    if IsWindows then
      file = string.match(file,"[:/\\](.+)$")
    else
      file = string.match(file,"[:/](.+)$")
    end
  end
  while noext do
    if isLine and isLine[noext] then
      if isLine[noext] == 1 then isLine[noext] = nil end
      return true
    end
    if isZero and isZero[noext] then
      if isZero[noext] == 1 then isZero[noext] = nil end
      return true
    end
    if IsWindows then
      noext = string.match(noext,"[:/\\](.+)$")
    else
      noext = string.match(noext,"[:/](.+)$")
    end
  end
  return false
end

--}}}
--{{{  local function capture_vars(ref,level,line)

local function capture_vars(ref,level,line)
  --get vars, file and line for the given level relative to debug_hook offset by ref

  local lvl = ref + level                --NB: This includes an offset of +1 for the call to here

  --{{{  capture variables

  local ar = debug.getinfo(lvl, "f")
  if not ar then return {},'?',0 end

  local vars = {__UPVALUES__={}, __LOCALS__={}}
  local i

  local func = ar.func
  if func then
    i = 1
    while true do
      local name, value = debug.getupvalue(func, i)
      if not name then break end
      if string.sub(name,1,1) ~= '(' then  --NB: ignoring internal control variables
        vars[name] = value
        vars.__UPVALUES__[i] = name
      end
      i = i + 1
    end
    vars.__ENVIRONMENT__ = getfenv(func)
  end

  vars.__GLOBALS__ = getfenv(0)

  i = 1
  while true do
    local name, value = debug.getlocal(lvl, i)
    if not name then break end
    if string.sub(name,1,1) ~= '(' then    --NB: ignoring internal control variables
      vars[name] = value
      vars.__LOCALS__[i] = name
    end
    i = i + 1
  end

  vars.__VARSLEVEL__ = level

  if func then
    --NB: Do not do this until finished filling the vars table
    setmetatable(vars, { __index = getfenv(func), __newindex = getfenv(func) })
  end

  --NB: Do not read or write the vars table anymore else the metatable functions will get invoked!

  --}}}

  local file = getinfo(lvl, "source")
  if string.find(file, "@") == 1 then
    file = string.sub(file, 2)
  end
  if IsWindows then file = string.lower(file) end

  if not line then
    line = getinfo(lvl, "currentline")
  end

  return vars,file,line

end

--}}}
--{{{  local function restore_vars(ref,vars)

local function restore_vars(ref,vars)

  if type(vars) ~= 'table' then return end

  local level = vars.__VARSLEVEL__       --NB: This level is relative to debug_hook offset by ref
  if not level then return end

  level = level + ref                    --NB: This includes an offset of +1 for the call to here

  local i
  local written_vars = {}

  i = 1
  while true do
    local name, value = debug.getlocal(level, i)
    if not name then break end
    if vars[name] and string.sub(name,1,1) ~= '(' then     --NB: ignoring internal control variables
      debug.setlocal(level, i, vars[name])
      written_vars[name] = true
    end
    i = i + 1
  end

  local ar = debug.getinfo(level, "f")
  if not ar then return end

  local func = ar.func
  if func then

    i = 1
    while true do
      local name, value = debug.getupvalue(func, i)
      if not name then break end
      if vars[name] and string.sub(name,1,1) ~= '(' then   --NB: ignoring internal control variables
        if not written_vars[name] then
          debug.setupvalue(func, i, vars[name])
        end
        written_vars[name] = true
      end
      i = i + 1
    end

  end

end

--}}}
--{{{  local function trace_event(event, line, level)

local function print_trace(level,depth,event,file,line,name)

  --NB: level here is relative to the caller of trace_event, so offset by 2 to get to there
  level = level + 2

  local file = file or getinfo(level,'short_src')
  local line = line or getinfo(level,'currentline')
  local name = name or getinfo(level,'name')

  local prefix = ''
  if current_thread ~= 'main' then prefix = '['..tostring(current_thread)..'] ' end

  ctrlrDebugger:write(prefix..
           string.format('%08.2f:%02i.',os.clock(),depth)..
           string.rep('.',depth%32)..
           (file or '')..' ('..(line or '')..') '..
           (name or '')..
           ' ('..event..')\n')

end

local function trace_event(event, line, level)

  if event == 'return' and trace_returns then
    --note the line info for later
    ret_file = getinfo(level+1,'short_src')
    ret_line = getinfo(level+1,'currentline')
    ret_name = getinfo(level+1,'name')
  end

  if event ~= 'line' then return end

  local slevel = stack_level[current_thread]
  local tlevel = trace_level[current_thread]

  if trace_calls and slevel > tlevel then
    --we are now in the function called, so look back 1 level further to find the calling file and line
    print_trace(level+1,slevel-1,'c',nil,nil,getinfo(level+1,'name'))
  end

  if trace_returns and slevel < tlevel then
    print_trace(level,slevel,'r',ret_file,ret_line,ret_name)
  end

  if trace_lines then
    print_trace(level,slevel,'l')
  end

  trace_level[current_thread] = stack_level[current_thread]

end

--}}}
--{{{  local function report(ev, vars, file, line, idx_watch)

local function report(ev, vars, file, line, idx_watch)
  local vars = vars or {}
  local file = file or '?'
  local line = line or 0
  local prefix = ''
  if current_thread ~= 'main' then prefix = '['..tostring(current_thread)..'] ' end
  if ev == events.STEP then
    ctrlrDebugger:write(prefix.."Paused at file "..file.." line "..line..' ('..stack_level[current_thread]..')\n')
  elseif ev == events.BREAK then
    ctrlrDebugger:write(prefix.."Paused at file "..file.." line "..line..' ('..stack_level[current_thread]..') (breakpoint)\n')
  elseif ev == events.WATCH then
    ctrlrDebugger:write(prefix.."Paused at file "..file.." line "..line..' ('..stack_level[current_thread]..')'.." (watch expression "..idx_watch.. ": ["..watches[idx_watch].exp.."])\n")
  elseif ev == events.SET then
    --do nothing
  else
    ctrlrDebugger:write(prefix.."Error in application: "..file.." line "..line.."\n")
  end
  if ev ~= events.SET then
    if pausemsg and pausemsg ~= '' then ctrlrDebugger:write('Message: '..pausemsg..'\n') end
    pausemsg = ''
  end
  return vars, file, line
end

--}}}

--{{{  local function debugger_loop(ev, vars, file, line, idx_watch)

local function debugger_loop(ev, vars, file, line, idx_watch)

  local eval_env  = vars or {}
  local breakfile = file or '?'
  local breakline = line or 0

  local command, args

  --{{{  local function getargs(spec)

  --get command arguments according to the given spec from the args string
  --the spec has a single character for each argument, arguments are separated
  --by white space, the spec characters can be one of:
  -- F for a filename    (defaults to breakfile if - given in args)
  -- L for a line number (defaults to breakline if - given in args)
  -- N for a number
  -- V for a variable name
  -- S for a string

  local function getargs(spec)
    local res={}
    local char,arg
    local ptr=1
    for i=1,string.len(spec) do
      char = string.sub(spec,i,i)
      if     char == 'F' then
        _,ptr,arg = string.find(args..' ',"%s*([%w%p]*)%s*",ptr)
        if not arg or arg == '' then arg = '-' end
        if arg == '-' then arg = breakfile end
      elseif char == 'L' then
        _,ptr,arg = string.find(args..' ',"%s*([%w%p]*)%s*",ptr)
        if not arg or arg == '' then arg = '-' end
        if arg == '-' then arg = breakline end
        arg = tonumber(arg) or 0
      elseif char == 'N' then
        _,ptr,arg = string.find(args..' ',"%s*([%w%p]*)%s*",ptr)
        if not arg or arg == '' then arg = '0' end
        arg = tonumber(arg) or 0
      elseif char == 'V' then
        _,ptr,arg = string.find(args..' ',"%s*([%w%p]*)%s*",ptr)
        if not arg or arg == '' then arg = '' end
      elseif char == 'S' then
        _,ptr,arg = string.find(args..' ',"%s*([%w%p]*)%s*",ptr)
        if not arg or arg == '' then arg = '' end
      else
        arg = ''
      end
      table.insert(res,arg or '')
    end
    return unpack(res)
  end

  --}}}

  while true do
    -- io.write("[DEBUG]> ")
    ctrlrDebugger:write("[DEBUG]> ")
    -- local line = io.read("*line")
    local line = ctrlrDebugger:read()
    if line == nil then ctrlrDebugger:write('\n'); line = 'exit' end

    if string.find(line, "^[a-z]+") then
      command = string.sub(line, string.find(line, "^[a-z]+"))
      args    = string.gsub(line,"^[a-z]+%s*",'',1)            --strip command off line
    else
      command = ''
    end

    if command == "setb" then
      --{{{  set breakpoint

      local line, filename  = getargs('LF')
      if filename ~= '' and line ~= '' then
        set_breakpoint(filename,line)
        ctrlrDebugger:write("Breakpoint set in file "..filename..' line '..line..'\n')
      else
        ctrlrDebugger:write("Bad request\n")
      end

      --}}}

    elseif command == "delb" then
      --{{{  delete breakpoint

      local line, filename = getargs('LF')
      if filename ~= '' and line ~= '' then
        remove_breakpoint(filename, line)
        ctrlrDebugger:write("Breakpoint deleted from file "..filename..' line '..line.."\n")
      else
        ctrlrDebugger:write("Bad request\n")
      end

      --}}}

    elseif command == "delallb" then
      --{{{  delete all breakpoints
      breakpoints = {}
      ctrlrDebugger:write('All breakpoints deleted\n')
      --}}}

    elseif command == "listb" then
      --{{{  list breakpoints
      for i, v in pairs(breakpoints) do
        for ii, vv in pairs(v) do
          ctrlrDebugger:write("Break at: "..i..' in '..ii..'\n')
        end
      end
      --}}}

    elseif command == "setw" then
      --{{{  set watch expression

      if args and args ~= '' then
        local func = loadstring("return(" .. args .. ")")
        local newidx = #watches + 1
        watches[newidx] = {func = func, exp = args}
        ctrlrDebugger:write("Set watch exp no. " .. newidx..'\n')
      else
        ctrlrDebugger:write("Bad request\n")
      end

      --}}}

    elseif command == "delw" then
      --{{{  delete watch expression

      local index = tonumber(args)
      if index then
        watches[index] = nil
        ctrlrDebugger:write("Watch expression deleted\n")
      else
        ctrlrDebugger:write("Bad request\n")
      end

      --}}}

    elseif command == "delallw" then
      --{{{  delete all watch expressions
      watches = {}
      ctrlrDebugger:write('All watch expressions deleted\n')
      --}}}

    elseif command == "listw" then
      --{{{  list watch expressions
      for i, v in pairs(watches) do
        ctrlrDebugger:write("Watch exp. " .. i .. ": " .. v.exp..'\n')
      end
      --}}}

    elseif command == "run" then
      --{{{  run until breakpoint
      step_into = false
      step_over = false
      return 'cont'
      --}}}

    elseif command == "step" then
      --{{{  step N lines (into functions)
      local N = tonumber(args) or 1
      step_over  = false
      step_into  = true
      step_lines = tonumber(N or 1)
      return 'cont'
      --}}}

    elseif command == "over" then
      --{{{  step N lines (over functions)
      local N = tonumber(args) or 1
      step_into  = false
      step_over  = true
      step_lines = tonumber(N or 1)
      step_level[current_thread] = stack_level[current_thread]
      return 'cont'
      --}}}

    elseif command == "out" then
      --{{{  step N lines (out of functions)
      local N = tonumber(args) or 1
      step_into  = false
      step_over  = true
      step_lines = 1
      step_level[current_thread] = stack_level[current_thread] - tonumber(N or 1)
      return 'cont'
      --}}}

    elseif command == "gotoo" then
      --{{{  step until reach line
      local line, filename = getargs('LF')
      if line ~= '' then
        step_over  = false
        step_into  = false
        if has_breakpoint(filename,line) then
          return 'cont'
        else
          set_breakpoint(filename,line,true)
          return 'cont'
        end
      else
        ctrlrDebugger:write("Bad request\n")
      end
      --}}}

    elseif command == "set" then
      --{{{  set/show context level
      local level = args
      if level and level == '' then level = nil end
      if level then return level end
      --}}}

    elseif command == "vars" then
      --{{{  list context variables
      local depth = args
      if depth and depth == '' then depth = nil end
      depth = tonumber(depth) or 1
      dumpvar(eval_env, depth+1, 'variables')
      --}}}

    elseif command == "glob" then
      --{{{  list global variables
      local depth = args
      if depth and depth == '' then depth = nil end
      depth = tonumber(depth) or 1
      dumpvar(eval_env.__GLOBALS__,depth+1,'globals')
      --}}}

    elseif command == "fenv" then
      --{{{  list function environment variables
      local depth = args
      if depth and depth == '' then depth = nil end
      depth = tonumber(depth) or 1
      dumpvar(eval_env.__ENVIRONMENT__,depth+1,'environment')
      --}}}

    elseif command == "ups" then
      --{{{  list upvalue names
      dumpvar(eval_env.__UPVALUES__,2,'upvalues')
      --}}}

    elseif command == "locs" then
      --{{{  list locals names
      dumpvar(eval_env.__LOCALS__,2,'upvalues')
      --}}}

    elseif command == "what" then
      --{{{  show where a function is defined
      if args and args ~= '' then
        local v = eval_env
        local n = nil
        for w in string.gmatch(args,"[%w_]+") do
          v = v[w]
          if n then n = n..'.'..w else n = w end
          if not v then break end
        end
        if type(v) == 'function' then
          local def = debug.getinfo(v,'S')
          if def then
            ctrlrDebugger:write(def.what..' in '..def.short_src..' '..def.linedefined..'..'..def.lastlinedefined..'\n')
          else
            ctrlrDebugger:write('Cannot get info for '..v..'\n')
          end
        else
          ctrlrDebugger:write(v..' is not a function\n')
        end
      else
        ctrlrDebugger:write("Bad request\n")
      end
      --}}}

    elseif command == "dump" then
      --{{{  dump a variable
      local name, depth = getargs('VN')
      if name ~= '' then
        if depth == '' or depth == 0 then depth = nil end
        depth = tonumber(depth or 1)
        local v = eval_env
        local n = nil
        for w in string.gmatch(name,"[^%.]+") do     --get everything between dots
          if tonumber(w) then
            v = v[tonumber(w)]
          else
            v = v[w]
          end
          if n then n = n..'.'..w else n = w end
          if not v then break end
        end
        dumpvar(v,depth+1,n)
      else
        ctrlrDebugger:write("Bad request\n")
      end
      --}}}

    elseif command == "show" then
      --{{{  show file around a line or the current breakpoint

      local line, file, before, after = getargs('LFNN')
      if before == 0 then before = 10     end
      if after  == 0 then after  = before end

      if file ~= '' and file ~= "=stdin" then
        show(file,line,before,after)
      else
        ctrlrDebugger:write('Nothing to show\n')
      end

      --}}}

    elseif command == "poff" then
      --{{{  turn pause command off
      pause_off = true
      --}}}

    elseif command == "pon" then
      --{{{  turn pause command on
      pause_off = false
      --}}}

    elseif command == "tron" then
      --{{{  turn tracing on/off
      local option = getargs('S')
      trace_calls   = false
      trace_returns = false
      trace_lines   = false
      if string.find(option,'c') then trace_calls   = true end
      if string.find(option,'r') then trace_returns = true end
      if string.find(option,'l') then trace_lines   = true end
      --}}}

    elseif command == "trace" then
      --{{{  dump a stack trace
      trace(eval_env.__VARSLEVEL__)
      --}}}

    elseif command == "info" then
      --{{{  dump all debug info captured
      info()
      --}}}

    elseif command == "pause" then
      --{{{  not allowed in here
      ctrlrDebugger:write('pause() should only be used in the script you are debugging\n')
      --}}}

    elseif command == "help" then
      --{{{  help
      local command = getargs('S')
      if command ~= '' and hints[command] then
        ctrlrDebugger:write(hints[command]..'\n')
      else
        for _,v in pairs(hints) do
          local _,_,h = string.find(v,"(.+)|")
          ctrlrDebugger:write(h..'\n')
        end
      end
      --}}}

    elseif command == "exit" then
      --{{{  exit debugger
      return 'stop'
      --}}}

    elseif line ~= '' then
      --{{{  just execute whatever it is in the current context

      --map line starting with "=..." to "return ..."
      if string.sub(line,1,1) == '=' then line = string.gsub(line,'=','return ',1) end

      local ok, func = pcall(loadstring,line)
      if func == nil then                             --Michael.Bringmann@lsi.com
        ctrlrDebugger:write("Compile error: "..line..'\n')
      elseif not ok then
        ctrlrDebugger:write("Compile error: "..func..'\n')
      else
        setfenv(func, eval_env)
        local res = {pcall(func)}
        if res[1] then
          if res[2] then
            table.remove(res,1)
            for _,v in ipairs(res) do
              ctrlrDebugger:write(tostring(v))
              ctrlrDebugger:write('\t')
            end
            ctrlrDebugger:write('\n')
          end
          --update in the context
          return 0
        else
          ctrlrDebugger:write("Run error: "..res[2]..'\n')
        end
      end

      --}}}
    end
  end

end

--}}}
--{{{  local function debug_hook(event, line, level, thread)

local function debug_hook(event, line, level, thread)
  if not started then debug.sethook(); coro_debugger = nil; return end
  current_thread = thread or 'main'
  local level = level or 2
  trace_event(event,line,level)
  if event == "call" then
    stack_level[current_thread] = stack_level[current_thread] + 1
  elseif event == "return" then
    stack_level[current_thread] = stack_level[current_thread] - 1
    if stack_level[current_thread] < 0 then stack_level[current_thread] = 0 end
  else
    local vars,file,line = capture_vars(level,1,line)
    local stop, ev, idx = false, events.STEP, 0
    while true do
      for index, value in pairs(watches) do
        setfenv(value.func, vars)
        local status, res = pcall(value.func)
        if status and res then
          ev, idx = events.WATCH, index
          stop = true
          break
        end
      end
      if stop then break end
      if (step_into)
      or (step_over and (stack_level[current_thread] <= step_level[current_thread] or stack_level[current_thread] == 0)) then
        step_lines = step_lines - 1
        if step_lines < 1 then
          ev, idx = events.STEP, 0
          break
        end
      end
      if has_breakpoint(file, line) then
        ev, idx = events.BREAK, 0
        break
      end
      return
    end
    if not coro_debugger then
      ctrlrDebugger:write("Lua Debugger\n")
      vars, file, line = report(ev, vars, file, line, idx)
      ctrlrDebugger:write("Type 'help' for commands\n")
      coro_debugger = true
    else
      vars, file, line = report(ev, vars, file, line, idx)
    end
    tracestack(level)
    local last_next = 1
    local next = 'ask'
    local silent = false
    while true do
      if next == 'ask' then
        next = debugger_loop(ev, vars, file, line, idx)
      elseif next == 'cont' then
        return
      elseif next == 'stop' then
        started = false
        debug.sethook()
        coro_debugger = nil
        return
      elseif tonumber(next) then --get vars for given level or last level
        next = tonumber(next)
        if next == 0 then silent = true; next = last_next else silent = false end
        last_next = next
        restore_vars(level,vars)
        vars, file, line = capture_vars(level,next)
        if not silent then
          if vars and vars.__VARSLEVEL__ then
            ctrlrDebugger:write('Level: '..vars.__VARSLEVEL__..'\n')
          else
            ctrlrDebugger:write('No level set\n')
          end
        end
        ev = events.SET
        next = 'ask'
      else
        ctrlrDebugger:write('Unknown command from debugger_loop: '..tostring(next)..'\n')
        ctrlrDebugger:write('Stopping debugger\n')
        next = 'stop'
      end
    end
  end
end

--}}}

--{{{  coroutine.create

--This function overrides the built-in for the purposes of propagating
--the debug hook settings from the creator into the created coroutine.

_G.coroutine.create = function(f)
  local thread
  local hook, mask, count = debug.gethook()
  if hook then
    local function thread_hook(event,line)
      hook(event,line,3,thread)
    end
    thread = cocreate(function(...)
                        stack_level[thread] = 0
                        trace_level[thread] = 0
                        step_level [thread] = 0
                        debug.sethook(thread_hook,mask,count)
                        return f(...)
                      end)
    return thread
  else
    return cocreate(f)
  end
end

--}}}
--{{{  coroutine.wrap

--This function overrides the built-in for the purposes of propagating
--the debug hook settings from the creator into the created coroutine.

_G.coroutine.wrap = function(f)
  local thread
  local hook, mask, count = debug.gethook()
  if hook then
    local function thread_hook(event,line)
      hook(event,line,3,thread)
    end
    thread = cowrap(function(...)
                      stack_level[thread] = 0
                      trace_level[thread] = 0
                      step_level [thread] = 0
                      debug.sethook(thread_hook,mask,count)
                      return f(...)
                    end)
    return thread
  else
    return cowrap(f)
  end
end

--}}}

--{{{  function pause(x,l,f)

--
-- Starts/resumes a debug session
--

function pause(x,l,f)
  if not f and pause_off then return end       --being told to ignore pauses
  pausemsg = x or 'pause'
  local lines
  local src = getinfo(2,'short_src')
  if l then
    lines = l   --being told when to stop
  elseif src == "stdin" then
    lines = 1   --if in a console session, stop now
  else
    lines = 2   --if in a script, stop when get out of pause()
  end
  if started then
    --we'll stop now 'cos the existing debug hook will grab us
    step_lines = lines
    step_into  = true
    debug.sethook(debug_hook, "crl")         --reset it in case some external agent fiddled with it
  else
    --set to stop when get out of pause()
    trace_level[current_thread] = 0
    step_level [current_thread] = 0
    stack_level[current_thread] = 1
    step_lines = lines
    step_into  = true
    started    = true
    debug.sethook(debug_hook, "crl")         --NB: this will cause an immediate entry to the debugger_loop
  end
end

--}}}
--{{{  function dump(v,depth)

--shows the value of the given variable, only really useful
--when the variable is a table
--see dump debug command hints for full semantics

function dump(v,depth)
  dumpvar(v,(depth or 1)+1,tostring(v))
end

--}}}
--{{{  function debug.traceback(x)

local _traceback = debug.traceback       --note original function

--override standard function
debug.traceback = function(x)
  local assertmsg = _traceback(x)        --do original function
  pause(x)                               --let user have a look at stuff
  return assertmsg                       --carry on
end

_TRACEBACK = debug.traceback             --Lua 5.0 function

--}}}

-- ================================================================
-- METHOD 006: inspect  (9233 bytes)
-- ================================================================

function getInspect()
local inspect ={
  _VERSION = 'inspect.lua 3.0.0',
  _URL     = 'http://github.com/kikito/inspect.lua',
  _DESCRIPTION = 'human-readable representations of tables',
  _LICENSE = [[
    MIT LICENSE

    Copyright (c) 2013 Enrique GarcÃ­a Cota

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the
    "Software"), to deal in the Software without restriction, including
    without limitation the rights to use, copy, modify, merge, publish,
    distribute, sublicense, and/or sell copies of the Software, and to
    permit persons to whom the Software is furnished to do so, subject to
    the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
    OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  ]]
}

inspect.KEY       = setmetatable({}, {__tostring = function() return 'inspect.KEY' end})
inspect.METATABLE = setmetatable({}, {__tostring = function() return 'inspect.METATABLE' end})

-- Apostrophizes the string if it has quotes, but not aphostrophes
-- Otherwise, it returns a regular quoted string
local function smartQuote(str)
  if str:match('"') and not str:match("'") then
    return "'" .. str .. "'"
  end
  return '"' .. str:gsub('"', '\\"') .. '"'
end

local controlCharsTranslation = {
  ["\a"] = "\\a",  ["\b"] = "\\b", ["\f"] = "\\f",  ["\n"] = "\\n",
  ["\r"] = "\\r",  ["\t"] = "\\t", ["\v"] = "\\v"
}

local function escape(str)
  local result = str:gsub("\\", "\\\\"):gsub("(%c)", controlCharsTranslation)
  return result
end

local function isIdentifier(str)
  return type(str) == 'string' and str:match( "^[_%a][_%a%d]*$" )
end

local function isSequenceKey(k, length)
  return type(k) == 'number'
     and 1 <= k
     and k <= length
     and math.floor(k) == k
end

local defaultTypeOrders = {
  ['number']   = 1, ['boolean']  = 2, ['string'] = 3, ['table'] = 4,
  ['function'] = 5, ['userdata'] = 6, ['thread'] = 7
}

local function sortKeys(a, b)
  local ta, tb = type(a), type(b)

  -- strings and numbers are sorted numerically/alphabetically
  if ta == tb and (ta == 'string' or ta == 'number') then return a < b end

  local dta, dtb = defaultTypeOrders[ta], defaultTypeOrders[tb]
  -- Two default types are compared according to the defaultTypeOrders table
  if dta and dtb then return defaultTypeOrders[ta] < defaultTypeOrders[tb]
  elseif dta     then return true  -- default types before custom ones
  elseif dtb     then return false -- custom types after default ones
  end

  -- custom types are sorted out alphabetically
  return ta < tb
end

local function getNonSequentialKeys(t)
  local keys, length = {}, #t
  for k,_ in pairs(t) do
    if not isSequenceKey(k, length) then table.insert(keys, k) end
  end
  table.sort(keys, sortKeys)
  return keys
end

local function getToStringResultSafely(t, mt)
  local __tostring = type(mt) == 'table' and rawget(mt, '__tostring')
  local str, ok
  if type(__tostring) == 'function' then
    ok, str = pcall(__tostring, t)
    str = ok and str or 'error: ' .. tostring(str)
  end
  if type(str) == 'string' and #str > 0 then return str end
end

local maxIdsMetaTable = {
  __index = function(self, typeName)
    rawset(self, typeName, 0)
    return 0
  end
}

local idsMetaTable = {
  __index = function (self, typeName)
    local col = setmetatable({}, {__mode = "kv"})
    rawset(self, typeName, col)
    return col
  end
}

local function countTableAppearances(t, tableAppearances)
  tableAppearances = tableAppearances or setmetatable({}, {__mode = "k"})

  if type(t) == 'table' then
    if not tableAppearances[t] then
      tableAppearances[t] = 1
      for k,v in pairs(t) do
        countTableAppearances(k, tableAppearances)
        countTableAppearances(v, tableAppearances)
      end
      countTableAppearances(getmetatable(t), tableAppearances)
    else
      tableAppearances[t] = tableAppearances[t] + 1
    end
  end

  return tableAppearances
end

local copySequence = function(s)
  local copy, len = {}, #s
  for i=1, len do copy[i] = s[i] end
  return copy, len
end

local function makePath(path, ...)
  local keys = {...}
  local newPath, len = copySequence(path)
  for i=1, #keys do
    newPath[len + i] = keys[i]
  end
  return newPath
end

local function processRecursive(process, item, path)
  if item == nil then return nil end

  local processed = process(item, path)
  if type(processed) == 'table' then
    local processedCopy = {}
    local processedKey

    for k,v in pairs(processed) do
      processedKey = processRecursive(process, k, makePath(path, k, inspect.KEY))
      if processedKey ~= nil then
        processedCopy[processedKey] = processRecursive(process, v, makePath(path, processedKey))
      end
    end

    local mt  = processRecursive(process, getmetatable(processed), makePath(path, inspect.METATABLE))
    setmetatable(processedCopy, mt)
    processed = processedCopy
  end
  return processed
end


-------------------------------------------------------------------

local Inspector = {}
local Inspector_mt = {__index = Inspector}

function Inspector:puts(...)
  local args   = {...}
  local buffer = self.buffer
  local len    = #buffer
  for i=1, #args do
    len = len + 1
    buffer[len] = tostring(args[i])
  end
end

function Inspector:down(f)
  self.level = self.level + 1
  f()
  self.level = self.level - 1
end

function Inspector:tabify()
  self:puts(self.newline, string.rep(self.indent, self.level))
end

function Inspector:alreadyVisited(v)
  return self.ids[type(v)][v] ~= nil
end

function Inspector:getId(v)
  local tv = type(v)
  local id = self.ids[tv][v]
  if not id then
    id              = self.maxIds[tv] + 1
    self.maxIds[tv] = id
    self.ids[tv][v] = id
  end
  return id
end

function Inspector:putKey(k)
  if isIdentifier(k) then return self:puts(k) end
  self:puts("[")
  self:putValue(k)
  self:puts("]")
end

function Inspector:putTable(t)
  if t == inspect.KEY or t == inspect.METATABLE then
    self:puts(tostring(t))
  elseif self:alreadyVisited(t) then
    self:puts('<table ', self:getId(t), '>')
  elseif self.level >= self.depth then
    self:puts('{...}')
  else
    if self.tableAppearances[t] > 1 then self:puts('<', self:getId(t), '>') end

    local nonSequentialKeys = getNonSequentialKeys(t)
    local length            = #t
    local mt                = getmetatable(t)
    local toStringResult    = getToStringResultSafely(t, mt)

    self:puts('{')
    self:down(function()
      if toStringResult then
        self:puts(' -- ', escape(toStringResult))
        if length >= 1 then self:tabify() end
      end

      local count = 0
      for i=1, length do
        if count > 0 then self:puts(',') end
        self:puts(' ')
        self:putValue(t[i])
        count = count + 1
      end

      for _,k in ipairs(nonSequentialKeys) do
        if count > 0 then self:puts(',') end
        self:tabify()
        self:putKey(k)
        self:puts(' = ')
        self:putValue(t[k])
        count = count + 1
      end

      if mt then
        if count > 0 then self:puts(',') end
        self:tabify()
        self:puts('<metatable> = ')
        self:putValue(mt)
      end
    end)

    if #nonSequentialKeys > 0 or mt then -- result is multi-lined. Justify closing }
      self:tabify()
    elseif length > 0 then -- array tables have one extra space before closing }
      self:puts(' ')
    end

    self:puts('}')
  end
end

function Inspector:putValue(v)
  local tv = type(v)

  if tv == 'string' then
    self:puts(smartQuote(escape(v)))
  elseif tv == 'number' or tv == 'boolean' or tv == 'nil' then
    self:puts(tostring(v))
  elseif tv == 'table' then
    self:putTable(v)
  else
    self:puts('<',tv,' ',self:getId(v),'>')
  end
end

-------------------------------------------------------------------

function inspect.inspect(root, options)
  options       = options or {}

  local depth   = options.depth   or math.huge
  local newline = options.newline or '\n'
  local indent  = options.indent  or '  '
  local process = options.process

  if process then
    root = processRecursive(process, root, {})
  end

  local inspector = setmetatable({
    depth            = depth,
    buffer           = {},
    level            = 0,
    ids              = setmetatable({}, idsMetaTable),
    maxIds           = setmetatable({}, maxIdsMetaTable),
    newline          = newline,
    indent           = indent,
    tableAppearances = countTableAppearances(root)
  }, Inspector_mt)

  inspector:putValue(root)

  return table.concat(inspector.buffer)
end

setmetatable(inspect, { __call = function(_, ...) return inspect.inspect(...) end })

return inspect
end

inspect = getInspect()

-- ================================================================
-- METHOD 007: INFOHandler  (7502 bytes)
-- ================================================================

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



-- ================================================================
-- METHOD 008: INFORHandler  (8962 bytes)
-- ================================================================

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



-- ================================================================
-- METHOD 009: INFOQHandler  (30359 bytes)
-- ================================================================

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


-- ================================================================
-- METHOD 010: initPanel  (7804 bytes)
-- ================================================================

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


-- ================================================================
-- METHOD 011: initTables  (24776 bytes)
-- ================================================================



function initTables()
--
-- value tables for ADSSR graphs
--
g_CANVdataInit	= {
["tssOSCPENVcanv"] = {iL=0,  aT=127, aL=0,   dT=127, sL=0,   r1T=127, r1L=0,  r2T=127, r2L=0},
["tssOSCFENVcanv"] = {iL=0,  aT=127, aL=0,   dT=127, sL=0,   r1T=127, r1L=0,  r2T=127, r2L=0},
["tssOSCAENVcanv"] = {iL=0,  aT=0,   aL=127, dT=127, sL=127, r1T=30,  r1L=00, r2T=127, r2L=0 },
["tssFLTFENVcanv"] = {iL=0,  aT=127, aL=0,   dT=127, sL=0,   r1T=127, r1L=0,  r2T=127, r2L=0},
}
g_CANVdata	= {}
for o=1,6 do
for c,v in pairs(g_CANVdataInit) do g_CANVdata[c..'-'..o]={} for f,v in pairs(g_CANVdataInit[c]) do g_CANVdata[c..'-'..o][f]=v  end end
end


--
-- TSS waveform list numbers:
--
g_XWTssWf	 = {P1={}, G1={}}
g_XWTssWf.P1 = { [1] = 1, [2] = 1, [3] = 326, [4] = 326, [5] = 0, [6] = 312 }
g_XWTssWf.G1 = { [1] = 1, [2] = 1, [3] = 781, [4] = 781, [5] = 0, [6] = 767 }
g_tsswf		 = g_XWTssWf.P1


--
-- general casio 'strange midi calculating' functions
--
g_xwModCalc= {}

g_xwModCalc["V2SX"]	= {			-- (sign),msb,lsb (order will be reversed in sendSX)
nf 		= function(v) 	return v     end,														-- normal fader 0-127
cf 		= function(v) 	return v+64  end,														-- centered fader -64 +63
nF		= function(v) 	return bit.rshift(v,7), bit.band(v,0x7f) end,							-- double byte fader
cF		= function(v) 	v=v+128 ; return bit.rshift(v,7),bit.band(v,0x7f) end,					-- -128..+128, msb,lsb	// sysex: lsb, msb, 40 == 04 
-- Casio specials:
sw 		= function(v) 	return v*127 end,														-- switch 0/127
nfx		= function(v,n) return math.floor(v*126/n) end,											-- formula for '< 127' fader
db		= function(v) 	return (8-v) end,														-- drawbars (inverted)
wf		= function(v,o)	v=v+g_tsswf[o];	return 0,bit.rshift(v,7),bit.band(v,0x7f) end,			-- wave-form (3by, starts a 1)
tn		= function(v) 	v=2*(v+256); 	return bit.rshift(v,7), bit.band(v,0x7f) end,			-- tune/detune : -256..+256 x 2 (!)
pk		= function(v) 	local sgn=0;local sx=0x30*v;if v<0 then sgn=0x7f;sx=0x4000+sx;end; return sgn,bit.rshift(sx,7),bit.band(sx,0x7f) end,	-- pitch key: -256..+256 * 0x30 "signed"
dt		= function(v) 	v=v+0x80; return bit.rshift(v,7), bit.band(v,0x7f) end,					-- tss DSP type
}

g_xwModCalc["SX2v"]	= {			-- (sign),msb,lsb (order will be reversed in sendSX)
nf 		= function(v) 		 return v     end,													-- normal fader 0-127
cf 		= function(v) 		 return v-64  end,													-- centered fader -64 +63
nF		= function(v1,v2) 	 return v1 + bit.lshift(v2) end,									-- double byte fader
cF		= function(v1,v2) 	 return v1 + bit.lshift(v2,7) - 128 end,							-- -128..+128, msb,lsb
-- Casio specials:
sw 		= function(v) 	 	 return v%127 end,													-- switch 0/127
nfx		= function(v,n) 	 return math.floor(v*n/127) end,	
db		= function(v)     	 return (v-8) end,													-- drawbars (inverted)
wf		= function(v1,v2,v3,o) return (v1 + bit.lshift(v2,7)) - g_tsswf[o] end,					-- wave-form (3by, starts a 1)
tn		= function(v1,v2)    return (v1 + bit.lshift(v2,7))/2 - 256 end,						-- tune/detune : -256..+256 x 2 (!)
pk		= function(v1,v2,v3) if v3==0 then return (v1+bit.lshift(v2,7))/0x30 else return (v1+bit.lshift(v2,7)-0x4000)/0x30 end end, 	-- pitch key: -256..+256 * 0x30 "signed"
dt		= function(v1,v2)	 return v1 + bit.lshift(v2,7) - 0x80 end,							-- tss DSP type
}

g_xwModCalc["nrpn"]	= {
nf 		= function(v) return v     end,															-- normal fader 0-127
cf 		= function(v) return v+64  end,															-- centered fader -64 +63
sw 		= function(v) return v*127 end,															-- switch 0/127
nF		= function(v) return bit.rshift(v,7), bit.band(v,127) end,								-- double byte fader
db		= function(v) return (8-v)*15 end,														-- drawbars (inverted)
cf256	= function(v) return bit.rshift(v+128,1), bit.lshift(bit.band(v+128,1),6) end,			-- -128..+128  msb, lsb
cf512	= function(v) return bit.rshift(v+256,2), bit.lshift(bit.band(v+256,3),5) end,			-- -256..+256  msb, lsb
nfx		= function(v,n) return math.floor(v*126/n) end,											-- formula for '< 127' fader
}


--#=====================================================================================================================

local tssModSX		= {}

tssModSX["tssOSC"] = {
sx					= {ct=0x09, bn=6},							-- cat, block-no
blk 				= function(b) return "00 00 00 00 00 00 0"..b.." 00" end,	-- block
tssOSCsw			= {id=0x00, ai=0 , an=0, vt='nf'},			-- osc on/off
tssOSCwf			= {id=0x03, ai=0 , an=0, vt='wf'},			-- syn wave number / 'split ui number'
tssOSCPortaSw		= {id=0x04, ai=0 , an=0, vt='nf'},
tssOSCPortaTm		= {id=0x05, ai=0 , an=0, vt='nf'},
tssOSCLegatoSw 		= {id=0x06, ai=0 , an=0, vt='nf'},
-- pitch
tssOSCPENViL		= {id=0x0b, ai=0 , an=0, vt='nf'},			-- ENV
tssOSCPlfo1D		= {id=0x08, ai=0 , an=0, vt='cf'},
tssOSCPlfo2D 		= {id=0x08, ai=1 , an=0, vt='cf'},
tssOSCPoset			= {id=0x09, ai=0 , an=0, vt='pk'},			-- pitch key:	: -256 - 0 - 255  in steps of  ca '10'
tssOSCPdtne			= {id=0x0a, ai=0 , an=0, vt='tn'},			-- detune	: nrnp: -256 - 0 - 255
tssOSCPENViL		= {id=0x0b, ai=0 , an=0, vt='cf'},			-- ENV
tssOSCPENVaT		= {id=0x0c, ai=0 , an=0, vt='nf'},
tssOSCPENVaL		= {id=0x0d, ai=0 , an=0, vt='cf'},
tssOSCPENVdT		= {id=0x0e, ai=0 , an=0, vt='nf'},
tssOSCPENVsL		= {id=0x0f, ai=0 , an=0, vt='cf'},
tssOSCPENVr1T		= {id=0x10, ai=0 , an=0, vt='nf'},
tssOSCPENVr1L		= {id=0x11, ai=0 , an=0, vt='cf'},
tssOSCPENVr2T		= {id=0x12, ai=0 , an=0, vt='nf'},
tssOSCPENVr2L		= {id=0x13, ai=0 , an=0, vt='cf'},
tssOSCPEclk			= {id=0x14, ai=0 , an=0, vt='nf'},			-- Clock Trigger
tssOSCPEdep			= {id=0x15, ai=0 , an=0, vt='cf'},			-- ENV depth
tssOSCPkeyf			= {id=0x17, ai=0 , an=0, vt='cF'},		-- key follow, 2byte
tssOSCPkeyfB		= {id=0x18, ai=0 , an=0, vt='nf'},
-- filter
tssOSCFcoff			= {id=0x19, ai=0 , an=0, vt='nf'},			-- cutoff
tssOSCFgain			= {id=0x1a, ai=0 , an=0, vt='nf'},			-- gain
tssOSCFtch			= {id=0x1b, ai=0 , an=0, vt='cf'},			-- touch sens
tssOSCFkeyf			= {id=0x1c, ai=0 , an=0, vt='cF'},
tssOSCFkeyfB		= {id=0x1d, ai=0 , an=0, vt='nf'},
tssOSCFlfo1D		= {id=0x1e, ai=0 , an=0, vt='cf'},
tssOSCFlfo2D 		= {id=0x1e, ai=1 , an=0, vt='cf'},
tssOSCFENViL		= {id=0x1f, ai=0 , an=0, vt='nf'},			-- ENV
tssOSCFENVaT		= {id=0x20, ai=0 , an=0, vt='nf'},
tssOSCFENVaL		= {id=0x21, ai=0 , an=0, vt='nf'},
tssOSCFENVdT		= {id=0x22, ai=0 , an=0, vt='nf'},
tssOSCFENVsL		= {id=0x23, ai=0 , an=0, vt='nf'},
tssOSCFENVr1T		= {id=0x24, ai=0 , an=0, vt='nf'},
tssOSCFENVr1L		= {id=0x25, ai=0 , an=0, vt='nf'},
tssOSCFENVr2T		= {id=0x26, ai=0 , an=0, vt='nf'},
tssOSCFENVr2L		= {id=0x27, ai=0 , an=0, vt='nf'},
tssOSCFEclk			= {id=0x28, ai=0 , an=0, vt='nf'},
tssOSCFEdep			= {id=0x29, ai=0 , an=0, vt='cf'},			-- Env depth
-- AMP
tssOSCAlvl			= {id=0x2a, ai=0 , an=0, vt='nf'},			-- level
tssOSCAtch			= {id=0x2c, ai=0 , an=0, vt='cf'},			-- touch sens
tssOSCAkeyf			= {id=0x2d, ai=0 , an=0, vt='cF'},
tssOSCAkeyfB		= {id=0x2e, ai=0 , an=0, vt='nf'},
tssOSCAlfo1D		= {id=0x2f, ai=0 , an=0, vt='cf'},
tssOSCAlfo2D 		= {id=0x2f, ai=0 , an=0, vt='cf'},
tssOSCAENViL		= {id=0x30, ai=0 , an=0, vt='nf'},			-- ENV
tssOSCAENVaT		= {id=0x31, ai=0 , an=0, vt='nf'},
tssOSCAENVaL		= {id=0x32, ai=0 , an=0, vt='nf'},
tssOSCAENVdT		= {id=0x33, ai=0 , an=0, vt='nf'},
tssOSCAENVsL		= {id=0x34, ai=0 , an=0, vt='nf'},
tssOSCAENVr1T		= {id=0x35, ai=0 , an=0, vt='nf'},
tssOSCAENVr1L		= {id=0x36, ai=0 , an=0, vt='nf'},
tssOSCAENVr2T		= {id=0x37, ai=0 , an=0, vt='nf'},
tssOSCAENVr2L		= {id=0x38, ai=0 , an=0, vt='nf'},
tssOSCAEclk			= {id=0x39, ai=0 , an=0, vt='nf'},
}

tssModSX["tssPWM"] = {
sx					= {ct=0x09, bn=2},							-- cat, block-no
blk 				= function(b) return "00 00 00 00 00 00 0"..b.." 00" end,	-- block
tssOSCPWMpw			= {id=0x3a, ai=0 , an=0, vt='nf'},			-- PWM
tssOSCPWMlfo1D		= {id=0x3c, ai=0 , an=0, vt='cf'},
tssOSCPWMlfo2D		= {id=0x3c, ai=1 , an=0, vt='cf'},
}

tssModSX["tssETC"] = {
sx					= {ct=0x09, bn=1},							-- cat, block-no
blk 				= function(b) return "00 00 00 00 00 00 00 00" end,	-- block
tssOSC2sync			= {id=0x3d, ai=0 , an=0, vt='nf'},			-- Osc Sync (only syn2)
tssOSCXokey			= {id=0x3e, ai=0 , an=0, vt='nf'},			-- EXT P
tssOSCXPxtrg		= {id=0x3f, ai=0 , an=0, vt='nf'},			--     trig P
tssOSCXFxtrg		= {id=0x40, ai=0 , an=0, vt='nf'},			--     trig F
tssOSCXAxtrg		= {id=0x41, ai=0 , an=0, vt='nf'},			--     trig A
tssOSCXTFxtrg		= {id=0x42, ai=0 , an=0, vt='nf'},			--     EXT trig Tot. Filter => placed in block 'Total Filter'
tssOSCXinlvl		= {id=0x43, ai=0 , an=0, vt='nf'},			--     mic instr level P
tssOSCXngth			= {id=0x44, ai=0 , an=0, vt='nf'},			--     trig. thresh P
tssOSCXngrel		= {id=0x45, ai=0 , an=0, vt='nf'},			--     trig. rel. P
tssOSCXPshmode		= {id=0x46, ai=0 , an=0, vt='nf'},			--     P
tssOSCXPshmix		= {id=0x47, ai=0 , an=0, vt='nf'},			--     P
}

tssModSX["tssFLT"] = {
sx					= {ct=0x09, bn=1},							-- cat, block-no
blk 				= function(b) return "00 00 00 00 00 00 00 00" end,	-- block
tssFLTFtype			= {id=0x48, ai=0 , an=0, vt='nf'},
tssFLTFcoff			= {id=0x49, ai=0 , an=0, vt='nf'},			-- cutoff
tssFLTFreso			= {id=0x4a, ai=0 , an=0, vt='nf'},			-- release
tssFLTFtch			= {id=0x4b, ai=0 , an=0, vt='cf'},			-- touch sens
tssFLTFkeyf			= {id=0x4c, ai=0 , an=0, vt='cF'},
tssFLTFkeyfB		= {id=0x4d, ai=0 , an=0, vt='nf'},
tssFLTFlfo1D		= {id=0x4e, ai=0 , an=0, vt='cf'},
tssFLTFlfo2D 		= {id=0x4e, ai=0 , an=0, vt='cf'},
tssFLTFENViL		= {id=0x4f, ai=0 , an=0, vt='nf'},			-- ENV
tssFLTFENVaT		= {id=0x50, ai=0 , an=0, vt='nf'},
tssFLTFENVaL		= {id=0x51, ai=0 , an=0, vt='nf'},
tssFLTFENVdT		= {id=0x52, ai=0 , an=0, vt='nf'},
tssFLTFENVsL		= {id=0x53, ai=0 , an=0, vt='nf'},
tssFLTFENVr1T		= {id=0x54, ai=0 , an=0, vt='nf'},
tssFLTFENVr1L		= {id=0x55, ai=0 , an=0, vt='nf'},
tssFLTFENVr2T		= {id=0x56, ai=0 , an=0, vt='nf'},
tssFLTFENVr2L		= {id=0x57, ai=0 , an=0, vt='nf'},
tssFLTFEclk			= {id=0x58, ai=0 , an=0, vt='nf'},
tssFLTFEdep			= {id=0x59, ai=0 , an=0, vt='cf'},
tssFLTFErtrg		= {id=0x5a, ai=0 , an=0, vt='nf'},
}

tssModSX["tssLFO"] = {
sx					= {ct=0x09, bn=2},								-- cat, block-no
blk 				= function(b) return "00 00 00 00 00 00 0"..b.." 00" end,	-- block
tssLFOwf			= {id=0x5b, ai=0 , an=0, vt='nf'},
tssLFOsync			= {id=0x5c, ai=0 , an=0, vt='nf'},
tssLFOrate			= {id=0x5d, ai=0 , an=0, vt='nf'},
tssLFOdep			= {id=0x5e, ai=0 , an=0, vt='nf'},
tssLFOdelay			= {id=0x5f, ai=0 , an=0, vt='nf'},
tssLFOrise			= {id=0x60, ai=0 , an=0, vt='nf'},
tssLFOclk			= {id=0x61, ai=0 , an=0, vt='nf'},
tssLFOmdep			= {id=0x62, ai=0 , an=0, vt='nf'},
}

tssModSX["tssDSP"] = {
sx					= {ct=0x13, bn=0},							-- cat, block-no
blk 				= function(b) return "00 00 00 00 00 00 00 00" end,
tssDSPTab			= {id=0x02, ai=0 , an=0, vt='dt'},
tssDSPPANwf			= {id=0x03, ai=0 , an=0, vt='sw'},			-- tss Pan
tssDSPPANrate		= {id=0x03, ai=1 , an=0, vt='nf'},
tssDSPPANdep		= {id=0x03, ai=2 , an=0, vt='nf'},
tssDSPPANman		= {id=0x03, ai=3 , an=0, vt='cf'},
tssDSPDSTgain		= {id=0x03, ai=0 , an=0, vt='nf'},			-- tss Distortion
tssDSPDSTlvl		= {id=0x03, ai=1 , an=0, vt='nf'},
tssDSPFLGwf			= {id=0x03, ai=0 , an=0, vt='nf-2'},		-- tss Flanger
tssDSPFLGrate		= {id=0x03, ai=1 , an=0, vt='nf'},
tssDSPFLGdep		= {id=0x03, ai=2 , an=0, vt='nf'},
tssDSPCHOwf			= {id=0x03, ai=0 , an=0, vt='sw'},			-- tss Chorus
tssDSPCHOrate		= {id=0x03, ai=1 , an=0, vt='nf'},
tssDSPCHOdep		= {id=0x03, ai=2 , an=0, vt='nf'},
tssDSPDELtime		= {id=0x03, ai=0 , an=0, vt='nf'},			-- tss Delay
tssDSPDELfb			= {id=0x03, ai=1 , an=0, vt='nf'},
tssDSPDELdamp		= {id=0x03, ai=2 , an=0, vt='nf-3'},
tssDSPDELwet		= {id=0x03, ai=3 , an=0, vt='nf-5'},
tssDSPDELsync		= {id=0x03, ai=4 , an=0, vt='nf-10'},
tssDSPRMDfreq		= {id=0x03, ai=0 , an=0, vt='nf'},			-- tss RingMod
tssDSPRMDdry		= {id=0x03, ai=1 , an=0, vt='nf'},
tssDSPRMDwet		= {id=0x03, ai=2 , an=0, vt='nf'},
}



--
-- generated complete 'modulator-sysex-value' assignment-tables for send and receive: 
--
g_tssModSXrx = {}
g_tssModSXtx = {}


local function createSXtssArray(cat)
	local sysex, _a
	_a	= tssModSX[cat]

	for m, _m in pairs(_a) do
		if  m:match("tss") then
			if	_a.sx.bn == 0 then
				--        f0 44 16 03 7f 00 cat  me pset. bk prm.... idx..   arr.... f7
	 			sysex	=    string.format("%.2x 00 00 00 %s %.2x 00 %.2x 00 %.2x 00", _a.sx.ct, _a.blk(0), _m.id, _m.ai, _m.an )		-- ggf. umstellen auf blk(b), b=0...a.sx.bn
				g_tssModSXrx[sysex] 	= { id=m	,     vt=_m.vt }			-- <sysex> = mod-id, value
				g_tssModSXtx[m]			= { sx=sysex,     vt=_m.vt } 			-- <modid> = sysex, value
			else
			for b=1,_a.sx.bn do
				--        f0 44 16 03 7f 00 cat  me pset. bk prm.... idx..   arr.... f7
	 			sysex	=    string.format("%.2x 00 00 00 %s %.2x 00 %.2x 00 %.2x 00", _a.sx.ct, _a.blk(b-1), _m.id, _m.ai, _m.an )		-- ggf. umstellen auf blk(b), b=0...a.sx.bn
				g_tssModSXrx[sysex] 	= { id=m..'-'..b, vt=_m.vt }			-- <sysex> = mod-id, value
				g_tssModSXtx[m..'-'..b]	= { sx=sysex,     vt=_m.vt } 			-- <modid> = sysex, value
			end
			end
		end
	end
end


createSXtssArray("tssOSC")
createSXtssArray("tssPWM")
createSXtssArray("tssETC")
createSXtssArray("tssFLT")
createSXtssArray("tssLFO")
-- createSXtssArray("tssDSP")


--#==============================================================================================================


local dspModSX	= {}

dspModSX["tssDSP"] = {
sx					= {ct=0x13, bn=0},									-- cat, block-no
blk 				= function(b) return "00 00 00 00 00 00 00 00" end,	-- block
tssDSPTab			= {tid=0x80, id=0x02, ai=0 , an=0, vt='dt'},		-- tss Type
--tssDSPoff			= {tid=0x80, id=0x03, ai=0 , an=0, vt='sw'},		-- tss off pseudo-modulator
tssDSPPANwf			= {tid=0x81, id=0x03, ai=0 , an=0, vt='sw'},		-- tss Pan
tssDSPPANrate		= {tid=0x81, id=0x03, ai=1 , an=0, vt='nf'},
tssDSPPANdep		= {tid=0x81, id=0x03, ai=2 , an=0, vt='nf'},
tssDSPPANman		= {tid=0x81, id=0x03, ai=3 , an=0, vt='cf'},
tssDSPDSTgain		= {tid=0x82, id=0x03, ai=0 , an=0, vt='nf'},		-- tss Distortion
tssDSPDSTlvl		= {tid=0x82, id=0x03, ai=1 , an=0, vt='nf'},
tssDSPFLGwf			= {tid=0x83, id=0x03, ai=0 , an=0, vt='nf-2'},		-- tss Flanger
tssDSPFLGrate		= {tid=0x83, id=0x03, ai=1 , an=0, vt='nf'},
tssDSPFLGdep		= {tid=0x83, id=0x03, ai=2 , an=0, vt='nf'},
tssDSPCHOwf			= {tid=0x84, id=0x03, ai=0 , an=0, vt='sw'},		-- tss Chorus
tssDSPCHOrate		= {tid=0x84, id=0x03, ai=1 , an=0, vt='nf'},
tssDSPCHOdep		= {tid=0x84, id=0x03, ai=2 , an=0, vt='nf'},
tssDSPDELtime		= {tid=0x85, id=0x03, ai=0 , an=0, vt='nf'},		-- tss Delay
tssDSPDELfb			= {tid=0x85, id=0x03, ai=1 , an=0, vt='nf'},
tssDSPDELdamp		= {tid=0x85, id=0x03, ai=2 , an=0, vt='nf-3'},
tssDSPDELwet		= {tid=0x85, id=0x03, ai=3 , an=0, vt='nf-5'},
tssDSPDELsync		= {tid=0x85, id=0x03, ai=4 , an=0, vt='nf-10'},
tssDSPRMDfreq		= {tid=0x86, id=0x03, ai=0 , an=0, vt='nf'},		-- tss RingMod
tssDSPRMDdry		= {tid=0x86, id=0x03, ai=1 , an=0, vt='nf'},
tssDSPRMDwet		= {tid=0x86, id=0x03, ai=2 , an=0, vt='nf'},
}


--
-- generated complete 'modulator-sysex-value' assignment-tables for send and receive: 
--
g_dspModSXrx = {}
g_dspModSXtx = {}

local function createSXdspArray(cat)
	local sysex, _a
	_a	= dspModSX[cat]

	for m, _m in pairs(_a) do
		if  m:match("tss") then
			--        f0 44 16 03 7f 00 cat  me pset. bk prm.... idx..   arr.... f7
			sysex	=    string.format("%.2x 00 00 00 %s %.2x 00 %.2x 00 %.2x 00", _a.sx.ct, _a.blk(0), _m.id, _m.ai, _m.an)
			g_dspModSXrx[string.format("%s-%.2x",sysex,_m.tid)] 	= { id=m	,     vt=_m.vt }			-- <sysex> = mod-id, value
			g_dspModSXtx[m]											= { sx=sysex,     vt=_m.vt } 			-- <modid> = sysex, value
		end
	end	
end

createSXdspArray("tssDSP")




--#=====================================================================================================================
--
-- NRPN:
--
g_tssModMidi		= {}

g_tssModMidi["tssOSC"] = {
tssMIDI 			= 'nrnp',						-- type
tssMSBid 			= {0x30, 0x31, 0x32, 0x33, 0x34, 0x35},			-- msb
-- general			     lsb	value-type
tssOSCsw			= {id=0x00, vt='sw'},
tssOSCwf			= {id=0x01, vt='nF'},			-- syn wave number / 'split ui number'
tssOSCPortaSw		= {id=0x02, vt='sw'},
tssOSCPortaTm		= {id=0x03, vt='nf'},
tssOSCLegatoSw 		= {id=0x04, vt='sw'},
-- pitch
tssOSCPlfo1D		= {id=0x05, vt='cf'},
tssOSCPlfo2D 		= {id=0x06, vt='cf'},
tssOSCPoset			= {id=0x07, vt='cf512'},		-- pitch	: nrnp: -256 - 0 - 255 
tssOSCPdtne			= {id=0x08, vt='cf512'},		-- detune	: nrnp: -256 - 0 - 255
tssOSCPENViL		= {id=0x09, vt='cf'},			-- ENV
tssOSCPENVaT		= {id=0x0a, vt='nf'},
tssOSCPENVaL		= {id=0x0b, vt='cf'},
tssOSCPENVdT		= {id=0x0c, vt='nf'},
tssOSCPENVsL		= {id=0x0d, vt='cf'},
tssOSCPENVr1T		= {id=0x0e, vt='nf'},
tssOSCPENVr1L		= {id=0x0f, vt='cf'},
tssOSCPENVr2T		= {id=0x10, vt='nf'},
tssOSCPENVr2L		= {id=0x11, vt='cf'},
tssOSCPEclk			= {id=0x12, vt='nf-18'},		-- Clock Trigger
tssOSCPEdep			= {id=0x13, vt='cf'},			-- ENV depth
tssOSCPkeyf			= {id=0x15, vt='cf256'},		-- key follow
tssOSCPkeyfB		= {id=0x16, vt='nf'},
-- filter
tssOSCFcoff			= {id=0x17, vt='nf-15'},		-- cutoff
tssOSCFgain			= {id=0x18, vt='nf-4'},			-- gain
tssOSCFtch			= {id=0x19, vt='cf'},			-- touch sens
tssOSCFkeyf			= {id=0x1a, vt='cf256'},
tssOSCFkeyfB		= {id=0x1b, vt='nf'},
tssOSCFlfo1D		= {id=0x1c, vt='cf'},
tssOSCFlfo2D 		= {id=0x1d, vt='cf'},
tssOSCFENViL		= {id=0x1e, vt='nf'},			-- ENV
tssOSCFENVaT		= {id=0x1f, vt='nf'},
tssOSCFENVaL		= {id=0x20, vt='nf'},
tssOSCFENVdT		= {id=0x21, vt='nf'},
tssOSCFENVsL		= {id=0x22, vt='nf'},
tssOSCFENVr1T		= {id=0x23, vt='nf'},
tssOSCFENVr1L		= {id=0x24, vt='nf'},
tssOSCFENVr2T		= {id=0x25, vt='nf'},
tssOSCFENVr2L		= {id=0x26, vt='nf'},
tssOSCFEclk			= {id=0x27, vt='nf-18'},
tssOSCFEdep			= {id=0x28, vt='cf'},			-- Env depth
-- AMP
tssOSCAlvl			= {id=0x29, vt='nf'},			-- level
tssOSCAtch			= {id=0x2b, vt='cf'},			-- touch sens
tssOSCAkeyf			= {id=0x2c, vt='cf256'},
tssOSCAkeyfB		= {id=0x2d, vt='nf'},
tssOSCAlfo1D		= {id=0x2e, vt='cf'},
tssOSCAlfo2D 		= {id=0x2f, vt='cf'},
tssOSCAENViL		= {id=0x30, vt='nf'},			-- ENV
tssOSCAENVaT		= {id=0x31, vt='nf'},
tssOSCAENVaL		= {id=0x32, vt='nf'},
tssOSCAENVdT		= {id=0x33, vt='nf'},
tssOSCAENVsL		= {id=0x34, vt='nf'},
tssOSCAENVr1T		= {id=0x35, vt='nf'},
tssOSCAENVr1L		= {id=0x36, vt='nf'},
tssOSCAENVr2T		= {id=0x37, vt='nf'},
tssOSCAENVr2L		= {id=0x38, vt='nf'},
tssOSCAEclk			= {id=0x39, vt='nf-18'},
-- div
tssOSCPWMpw			= {id=0x3a, vt='nf'},			-- PWM
tssOSCPWMlfo1D		= {id=0x3c, vt='cf'},
tssOSCPWMlfo2D		= {id=0x3d, vt='cf'},
tssOSC2sync			= {id=0x3e, vt='sw'},			-- Osc Sync (only syn2)
tssOSCXokey			= {id=0x3f, vt='nf'},			-- EXT P
tssOSCXPxtrg		= {id=0x40, vt='sw'},			--     trig P
tssOSCXFxtrg		= {id=0x41, vt='sw'},			--     trig F
tssOSCXAxtrg		= {id=0x42, vt='sw'},			--     trig A
tssOSCXTFxtrg		= {id=0x43, vt='sw'},			--     EXT trig Tot. Filter => placed in block 'Total Filter'
tssOSCXinlvl		= {id=0x44, vt='nf'},			--     mic instr level P
tssOSCXngth			= {id=0x45, vt='nf'},			--     trig. thresh P
tssOSCXngrel		= {id=0x46, vt='nf'},			--     trig. rel. P
tssOSCXPshmode		= {id=0x47, vt='nf-3'},			--     P
tssOSCXPshmix		= {id=0x48, vt='nf-15'},		--     P
}
g_tssModMidi["tssLFO"] = {
tssMIDI 			= 'nrnp',						-- type
tssMSBid 			= {0x36, 0x37},					-- msb
tssLFOwf			= {id=0x00, vt='nf-7'},
tssLFOsync			= {id=0x01, vt='sw'},
tssLFOrate			= {id=0x02, vt='nf'},
tssLFOdep			= {id=0x03, vt='nf'},
tssLFOdelay			= {id=0x04, vt='nf'},
tssLFOrise			= {id=0x05, vt='nf'},
tssLFOclk			= {id=0x06, vt='nf-17'},
tssLFOmdep			= {id=0x07, vt='nf'},
}
g_tssModMidi["tssFLT"] = {
tssMIDI 			= 'nrnp',						-- type
tssMSBid 			= {0x38},						-- msb
tssFLTFtype			= {id=0x00, vt='nf-2'},
tssFLTFcoff			= {id=0x01, vt='nf'},			-- cutoff
tssFLTFreso			= {id=0x02, vt='nf'},			-- release
tssFLTFtch			= {id=0x03, vt='cf'},			-- touch sens
tssFLTFkeyf			= {id=0x04, vt='cf256'},
tssFLTFkeyfB		= {id=0x05, vt='nf'},
tssFLTFlfo1D		= {id=0x06, vt='cf'},
tssFLTFlfo2D 		= {id=0x07, vt='cf'},
tssFLTFENViL		= {id=0x08, vt='nf'},			-- ENV
tssFLTFENVaT		= {id=0x09, vt='nf'},
tssFLTFENVaL		= {id=0x0a, vt='nf'},
tssFLTFENVdT		= {id=0x0b, vt='nf'},
tssFLTFENVsL		= {id=0x0c, vt='nf'},
tssFLTFENVr1T		= {id=0x0d, vt='nf'},
tssFLTFENVr1L		= {id=0x0e, vt='nf'},
tssFLTFENVr2T		= {id=0x0f, vt='nf'},
tssFLTFENVr2L		= {id=0x10, vt='nf'},
tssFLTFEclk			= {id=0x11, vt='nf-18'},
tssFLTFEdep			= {id=0x12, vt='cf'},
tssFLTFErtrg		= {id=0x13, vt='sw'},
}
g_tssModMidi["tssDSP"] = {
tssMIDI 			= 'cc',							-- type
tssMSBid 			= {0x00},						-- not used
tssDSPPANwf			= {id=0x10, vt='sw'},			-- Pan
tssDSPPANrate		= {id=0x11, vt='nf'},
tssDSPPANdep		= {id=0x12, vt='nf'},
tssDSPPANman		= {id=0x13, vt='cf'},
tssDSPDSTgain		= {id=0x10, vt='nf'},			-- Distortion
tssDSPDSTlvl		= {id=0x11, vt='nf'},
tssDSPFLGwf			= {id=0x10, vt='nf-2'},			-- Flanger
tssDSPFLGrate		= {id=0x11, vt='nf'},
tssDSPFLGdep		= {id=0x12, vt='nf'},
tssDSPCHOwf			= {id=0x10, vt='sw'},			-- Chorus
tssDSPCHOrate		= {id=0x11, vt='nf'},
tssDSPCHOdep		= {id=0x12, vt='nf'},
tssDSPDELtime		= {id=0x10, vt='nf'},			-- Chorus
tssDSPDELfb			= {id=0x11, vt='nf'},
tssDSPDELdamp		= {id=0x12, vt='nf-3'},
tssDSPDELwet		= {id=0x13, vt='nf-5'},
tssDSPDELsync		= {id=0x50, vt='nf-10'},
tssDSPRMDfreq		= {id=0x10, vt='nf'},			-- RingMod
tssDSPRMDdry		= {id=0x11, vt='nf'},
tssDSPRMDwet		= {id=0x12, vt='nf'},
}
g_tssModMidi["tssCOM"] = {
tssMIDI 			= 'cc',							-- type
tssMSBid 			= {0x00},						-- not used
tssCOMvol			= {id=0x07, vt='nf'},
tssCOMrevb			= {id=0x5b, vt='nf'},
}



--
-- MIXER
--
g_mixModMidi = {}

g_mixModMidi['mixHEX'] = {
mixMIDI 			= 'nrnp',						-- type
mixMSBid 			= {0x3e},						-- msb
mixHEX1lvl			= {id=0x10, vt='cf256'},		-- level layer 1
mixHEX2lvl			= {id=0x11, vt='cf256'},
mixHEX3lvl			= {id=0x12, vt='cf256'},
mixHEX4lvl			= {id=0x13, vt='cf256'},
mixHEX5lvl			= {id=0x14, vt='cf256'},
mixHEX6lvl			= {id=0x15, vt='cf256'},
mixHEXAcoff			= {id=0x16, vt='cf256'},
mixHEXAdtne			= {id=0x17, vt='nf-32'},
mixHEXAatk			= {id=0x18, vt='cf256'},
mixHEXArel			= {id=0x19, vt='cf256'},
}
g_mixModMidi["mixDSP"] = {
mixMIDI 			= 'nrnp',						-- type
mixMSBid 			= {0x22},						-- msb
mixPARTsw			= {id=0x00, vt='sw'},			-- DSP switch
mixDSPsw			= {id=0x01, vt='sw'},			-- DSP switch
}



--
-- ORGAN
--
g_orgModMidi = {}

g_orgModMidi["orgTW"] = {
orgMIDI 			= 'nrnp',						-- type
orgMSBid 			= {0x40},						-- msb
orgTWdbar16			= {id=0x00, vt='db'},
orgTWdbar513		= {id=0x01, vt='db'},
orgTWdbar8			= {id=0x02, vt='db'},
orgTWdbar4			= {id=0x03, vt='db'},
orgTWdbar223		= {id=0x04, vt='db'},
orgTWdbar2			= {id=0x05, vt='db'},
orgTWdbar135		= {id=0x06, vt='db'},
orgTWdbar113		= {id=0x07, vt='db'},
orgTWdbar1			= {id=0x08, vt='db'},	
orgTWclckon			= {id=0x09, vt='sw'},
orgTWperc2			= {id=0x0a, vt='sw'},
orgTWperc3			= {id=0x0b, vt='sw'},
orgTWpercdec		= {id=0x0c, vt='nf'},
orgTWtype			= {id=0x0d, vt='sw'},
orgTWclckoff		= {id=0x0e, vt='sw'},
}
g_orgModMidi["orgVC"]  = {
orgMIDI 			= 'cc',							-- type
orgMSBid 			= {0x00},						-- not used
orgVCrate			= {id=0x59, vt='nf'},
orgVCdepth			= {id=0x5a, vt='nf'},
}
g_orgModMidi["orgROT"]  = {
orgMIDI 			= 'cc',							-- type
orgMSBid 			= {0x00},						-- not used
orgROTODgain		= {id=0x10, vt='nf-4'},			-- Gain
orgROTODlvl			= {id=0x11, vt='nf'},
orgROTspd			= {id=0x12, vt='sw'},			-- slow fast
orgROTbrk			= {id=0x13, vt='sw'},			-- brake
orgROTfacc			= {id=0x50, vt='nf'},
orgROTracc			= {id=0x51, vt='nf'},
orgROTsrate			= {id=0x52, vt='nf'},
orgROTfrate			= {id=0x53, vt='nf'},
}
g_orgModMidi["orgGEN"]  = {
orgMIDI 			= 'cc',							-- type
orgMSBid 			= {0x00},						-- not used
orgGENexpr			= {id=0x0b, vt='nf'},
orgGENrevb			= {id=0x5b, vt='nf'},
}


--
-- PCM
--
g_mixDrumKits = {
{ name="StandardSet1", prg=0, msb=120 }, 
{ name="StandardSet2", prg=1, msb=120 }, 
{ name="StandardSet3", prg=2, msb=120 }, 
{ name="StandardSet4", prg=3, msb=120 }, 
{ name="Room Set", prg=8, msb=120 }, 
{ name="Hip-Hop Set", prg=9, msb=120 }, 
{ name="Rock Set", prg=17, msb=120 }, 
{ name="Elec.Set", prg=24, msb=120 }, 
{ name="Synth Set 1", prg=25, msb=120 }, 
{ name="Synth Set 2", prg=30, msb=120 }, 
{ name="Trance Set", prg=31, msb=120 }, 
{ name="Dance Set 1", prg=29, msb=120 }, 
{ name="Dance Set 2", prg=28, msb=120 }, 
{ name="Dance Set 3", prg=27, msb=120 }, 
{ name="Jazz Set", prg=32, msb=120 }, 
{ name="Brush Set", prg=40, msb=120 }, 
{ name="OrchestraSet", prg=48, msb=120 }, 
{ name="Ethnic Set 1", prg=49, msb=120 }, 
{ name="Ethnic Set 2", prg=50, msb=120 }, 
}


end


-- ================================================================
-- METHOD 012: initWaves  (86455 bytes)
-- ================================================================

function initWaves()


g_tssSYNwave	= {}
g_tssPCMwave	= {}

g_tssSYNwave.P1 = "0.Sin Wave\n1.Sin Wave-L\n2.Sin Wave-B\n3.Triangle\n4.Triangle-L\n5.Triangle-B\n6.Sawtooth\n7.Sawtooth-L\n8.Sawtooth-B\n9.ReverseSaw\n10.ReverseSaw-L\n11.ReverseSaw-B\n12.Square Wave\n13.Square Wav-L\n14.Square Wav-B\n15.PWM\n16.PWM-L\n17.PWM-B\n18.MM Triangle\n19.MM Triangl-L\n20.MM Triangl-B\n21.MM Ramp\n22.MM Ramp-L\n23.MM Ramp-B\n24.MM Saw\n25.MM Saw-L\n26.MM Saw-B\n27.MM Square\n28.MM Square-L\n29.MM Square-B\n30.MM WidPulse\n31.MM WidPuls-L\n32.MM WidPuls-B\n33.MM NrwPulse\n34.MM NrwPuls-L\n35.MM NrwPuls-B\n36.MG Sin\n37.MG Sin-L\n38.MG Sin-B\n39.MG Triangle\n40.MG Triangl-L\n41.MG Triangl-B\n42.MG Saw\n43.MG Saw-L\n44.MG Saw-B\n45.MG Square\n46.MG Square-L\n47.MG Square-B\n48.MG Pulse1\n49.MG Pulse1-L\n50.MG Pulse1-B\n51.MG Pulse2\n52.MG Pulse2-L\n53.MG Pulse2-B\n54.AP1 Saw\n55.AP1 Saw-L\n56.AP1 Saw-B\n57.AP1 Square\n58.AP1 Square-L\n59.AP1 Square-B\n60.AP1 Triangle\n61.AP1 Triang-L\n62.AP1 Triang-B\n63.AP1 Pulse1\n64.AP1 Pulse1-L\n65.AP1 Pulse1-B\n66.AP1 Pulse2\n67.AP1 Pulse2-L\n68.AP1 Pulse2-B\n69.AP1 Pulse3\n70.AP1 Pulse3-L\n71.AP1 Pulse3-B\n72.AP2 Saw\n73.AP2 Saw-L\n74.AP2 Saw-B\n75.AP2 Pulse1\n76.AP2 Pulse1-L\n77.AP2 Pulse1-B\n78.AP2 Pulse2\n79.AP2 Pulse2-L\n80.AP2 Pulse2-B\n81.AP2 SyncSaw\n82.AP2 SycSaw-L\n83.AP2 SycSaw-B\n84.AP2 SyncPls\n85.AP2 SycPls-L\n86.AP2 SycPls-B\n87.OB Saw\n88.OB Saw-L\n89.OB Saw-B\n90.OB Pulse1\n91.OB Pulse1-L\n92.OB Pulse1-B\n93.OB Pulse2\n94.OB Pulse2-L\n95.OB Pulse2-B\n96.OB SyncSaw\n97.OB SyncSaw-L\n98.OB SyncSaw-B\n99.OB SyncPls\
100.OB SyncPls-L\n101.OB SyncPls-B\n102.P5 Triangle\n103.P5 Triangl-L\n104.P5 Triangl-B\n105.P5 Saw\n106.P5 Saw-L\n107.P5 Saw-B\n108.P5 Pulse1\n109.P5 Pulse1-L\n110.P5 Pulse1-B\n111.P5 Pulse2\n112.P5 Pulse2-L\n113.P5 Pulse2-B\n114.P5 Pulse3\n115.P5 Pulse3-L\n116.P5 Pulse3-B\n117.P5 Pulse4\n118.P5 Pulse4-L\n119.P5 Pulse4-B\n120.ND Saw\n121.ND Saw-L\n122.ND Saw-B\n123.ND Pulse1\n124.ND Pulse1-L\n125.ND Pulse1-B\n126.ND Pulse2\n127.ND Pulse2-L\n128.ND Pulse2-B\n129.ND Pulse3\n130.ND Pulse3-L\n131.ND Pulse3-B\n132.ND FM1\n133.ND FM1-L\n134.ND FM1-B\n135.ND FM2\n136.ND FM2-L\n137.ND FM2-B\n138.ND FM3\n139.ND FM3-L\n140.ND FM3-B\n141.JP Saw\n142.JP Saw-L\n143.JP Saw-B\n144.JP Square\n145.JP Square-L\n146.JP Square-B\n147.JP Pulse\n148.JP Pulse-L\n149.JP Pulse-B\n150.CZ Saw\n151.CZ Saw-L\n152.CZ Saw-B\n153.CZ Square\n154.CZ Square-L\n155.CZ Square-B\n156.CZ Pulse\n157.CZ Pulse-L\n158.CZ Pulse-B\n159.CZ DoubleSin\n160.CZ DoblSin-L\n161.CZ DoblSin-B\n162.CZ Saw Pulse\n163.CZ SawPuls-L\n164.CZ SawPuls-B\n165.CZ Saw Reso\n166.CZ SawReso-L\n167.CZ SawReso-B\n168.CZ Tri Reso\n169.CZ TriReso-L\n170.CZ TriReso-B\n171.CZ Tra Reso\n172.CZ TraReso-L\n173.CZ TraReso-B\n174.CZ-Wave9\n175.CZ-Wave9-L\n176.CZ-Wave9-B\n177.CZ-Wave10\n178.CZ-Wave10-L\n179.CZ-Wave10-B\n180.CZ-Wave11\n181.CZ-Wave11-L\n182.CZ-Wave11-B\n183.CZ-Wave12\n184.CZ-Wave12-L\n185.CZ-Wave12-B\n186.CZ-Wave13\n187.CZ-Wave13-L\n188.CZ-Wave13-B\n189.CZ-Wave14\n190.CZ-Wave14-L\n191.CZ-Wave14-B\n192.CZ-Wave15\n193.CZ-Wave15-L\n194.CZ-Wave15-B\n195.CZ-Wave16\n196.CZ-Wave16-L\n197.CZ-Wave16-B\n198.CZ-Wave17\n199.CZ-Wave17-L\
200.CZ-Wave17-B\n201.CZ-Wave18\n202.CZ-Wave18-L\n203.CZ-Wave18-B\n204.CZ-Wave19\n205.CZ-Wave19-L\n206.CZ-Wave19-B\n207.CZ-Wave20\n208.CZ-Wave20-L\n209.CZ-Wave20-B\n210.CZ-Wave21\n211.CZ-Wave21-L\n212.CZ-Wave21-B\n213.CZ-Wave22\n214.CZ-Wave22-L\n215.CZ-Wave22-B\n216.CZ-Wave23\n217.CZ-Wave23-L\n218.CZ-Wave23-B\n219.CZ-Wave24\n220.CZ-Wave24-L\n221.CZ-Wave24-B\n222.CZ-Wave25\n223.CZ-Wave25-L\n224.CZ-Wave25-B\n225.CZ-Wave26\n226.CZ-Wave26-L\n227.CZ-Wave26-B\n228.CZ-Wave27\n229.CZ-Wave27-L\n230.CZ-Wave27-B\n231.CZ-Wave28\n232.CZ-Wave28-L\n233.CZ-Wave28-B\n234.CZ-Wave29\n235.CZ-Wave29-L\n236.CZ-Wave29-B\n237.CZ-Wave30\n238.CZ-Wave30-L\n239.CZ-Wave30-B\n240.CZ-Wave31\n241.CZ-Wave31-L\n242.CZ-Wave31-B\n243.CZ-Wave32\n244.CZ-Wave32-L\n245.CZ-Wave32-B\n246.CZ-Wave33\n247.CZ-Wave33-L\n248.CZ-Wave33-B\n249.VA Synth1\n250.VA Synth2\n251.VA Synth3\n252.VA Synth4\n253.VA Synth5\n254.VA Synth6\n255.VA Synth7\n256.VA Synth8\n257.VA Synth9\n258.VA Synth10\n259.VA Synth11\n260.VA Synth12\n261.VA Synth13\n262.VA Synth14\n263.VA Synth15\n264.VA Synth16\n265.VA Synth17\n266.VA Synth18\n267.VA Synth19\n268.TB Saw1-L\n269.TB Saw1-B\n270.TB Saw2-L\n271.TB Saw2-B\n272.TB Saw3-L\n273.TB Saw3-B\n274.TB Pulse1-L\n275.TB Pulse1-B\n276.TB Pulse2-L\n277.TB Pulse2-B\n278.TB Pulse3-L\n279.TB Pulse3-B\n280.TB Bass 1A\n281.TB Bass 1B\n282.TB Bass 1C\n283.TB Bass 2A\n284.TB Bass 2B\n285.TB Bass 2C\n286.MG Bass 1A\n287.MG Bass 1B\n288.MG Bass 1C\n289.MG Bass 2A\n290.MG Bass 2B\n291.MG Bass 2C\n292.SH Saw-L\n293.SH Saw-B\n294.SH Pulse1-L\n295.SH Pulse1-B\n296.SH Pulse2-L\n297.SH Pulse2-B\n298.SH Pulse3-L\n299.SH Pulse3-B\
300.SH Sub OSC-L\n301.SH Sub OSC-B\n302.SH BASS 1\n303.SH BASS 2\n304.SH BASS 3\n305.SH BASS 4\n306.SH BASS 5\n307.SH BASS 6\n308.SH BASS 7\n309.SH BASS 8\n310.SH BASS 9\
"
g_tssSYNwave.G1	= "0.Sin Wave\n1.Sin Wave-L\n2.Sin Wave-B\n3.Triangle\n4.Triangle-L\n5.Triangle-B\n6.Sawtooth\n7.Sawtooth-L\n8.Sawtooth-B\n9.ReverseSaw\n10.ReverseSaw-L\n11.ReverseSaw-B\n12.Square Wave\n13.Square Wav-L\n14.Square Wav-B\n15.PWM\n16.PWM-L\n17.PWM-B\n18.MM Triangle\n19.MM Triangl-L\n20.MM Triangl-B\n21.MM Ramp\n22.MM Ramp-L\n23.MM Ramp-B\n24.MM Saw\n25.MM Saw-L\n26.MM Saw-B\n27.MM Square\n28.MM Square-L\n29.MM Square-B\n30.MM WidPulse\n31.MM WidPuls-L\n32.MM WidPuls-B\n33.MM NrwPulse\n34.MM NrwPuls-L\n35.MM NrwPuls-B\n36.MG Sin\n37.MG Sin-L\n38.MG Sin-B\n39.MG Triangle\n40.MG Triangl-L\n41.MG Triangl-B\n42.MG Saw\n43.MG Saw-L\n44.MG Saw-B\n45.MG Square\n46.MG Square-L\n47.MG Square-B\n48.MG Pulse1\n49.MG Pulse1-L\n50.MG Pulse1-B\n51.MG Pulse2\n52.MG Pulse2-L\n53.MG Pulse2-B\n54.AP1 Saw\n55.AP1 Saw-L\n56.AP1 Saw-B\n57.AP1 Square\n58.AP1 Square-L\n59.AP1 Square-B\n60.AP1 Triangle\n61.AP1 Triang-L\n62.AP1 Triang-B\n63.AP1 Pulse1\n64.AP1 Pulse1-L\n65.AP1 Pulse1-B\n66.AP1 Pulse2\n67.AP1 Pulse2-L\n68.AP1 Pulse2-B\n69.AP1 Pulse3\n70.AP1 Pulse3-L\n71.AP1 Pulse3-B\n72.AP2 Saw\n73.AP2 Saw-L\n74.AP2 Saw-B\n75.AP2 Pulse1\n76.AP2 Pulse1-L\n77.AP2 Pulse1-B\n78.AP2 Pulse2\n79.AP2 Pulse2-L\n80.AP2 Pulse2-B\n81.AP2 SyncSaw\n82.AP2 SycSaw-L\n83.AP2 SycSaw-B\n84.AP2 SyncPls\n85.AP2 SycPls-L\n86.AP2 SycPls-B\n87.OB Saw\n88.OB Saw-L\n89.OB Saw-B\n90.OB Pulse1\n91.OB Pulse1-L\n92.OB Pulse1-B\n93.OB Pulse2\n94.OB Pulse2-L\n95.OB Pulse2-B\n96.OB SyncSaw\n97.OB SyncSaw-L\n98.OB SyncSaw-B\n99.OB SyncPls\
100.OB SyncPls-L\n101.OB SyncPls-B\n102.P5 Triangle\n103.P5 Triangl-L\n104.P5 Triangl-B\n105.P5 Saw\n106.P5 Saw-L\n107.P5 Saw-B\n108.P5 Pulse1\n109.P5 Pulse1-L\n110.P5 Pulse1-B\n111.P5 Pulse2\n112.P5 Pulse2-L\n113.P5 Pulse2-B\n114.P5 Pulse3\n115.P5 Pulse3-L\n116.P5 Pulse3-B\n117.P5 Pulse4\n118.P5 Pulse4-L\n119.P5 Pulse4-B\n120.ND Saw\n121.ND Saw-L\n122.ND Saw-B\n123.ND Pulse1\n124.ND Pulse1-L\n125.ND Pulse1-B\n126.ND Pulse2\n127.ND Pulse2-L\n128.ND Pulse2-B\n129.ND Pulse3\n130.ND Pulse3-L\n131.ND Pulse3-B\n132.ND FM1\n133.ND FM1-L\n134.ND FM1-B\n135.ND FM2\n136.ND FM2-L\n137.ND FM2-B\n138.ND FM3\n139.ND FM3-L\n140.ND FM3-B\n141.JP Saw\n142.JP Saw-L\n143.JP Saw-B\n144.JP Square\n145.JP Square-L\n146.JP Square-B\n147.JP Pulse\n148.JP Pulse-L\n149.JP Pulse-B\n150.CZ Saw\n151.CZ Saw-L\n152.CZ Saw-B\n153.CZ Square\n154.CZ Square-L\n155.CZ Square-B\n156.CZ Pulse\n157.CZ Pulse-L\n158.CZ Pulse-B\n159.CZ DoubleSin\n160.CZ DoblSin-L\n161.CZ DoblSin-B\n162.CZ Saw Pulse\n163.CZ SawPuls-L\n164.CZ SawPuls-B\n165.CZ Saw Reso\n166.CZ SawReso-L\n167.CZ SawReso-B\n168.CZ Tri Reso\n169.CZ TriReso-L\n170.CZ TriReso-B\n171.CZ Tra Reso\n172.CZ TraReso-L\n173.CZ TraReso-B\n174.CZ-Wave9\n175.CZ-Wave9-L\n176.CZ-Wave9-B\n177.CZ-Wave10\n178.CZ-Wave10-L\n179.CZ-Wave10-B\n180.CZ-Wave11\n181.CZ-Wave11-L\n182.CZ-Wave11-B\n183.CZ-Wave12\n184.CZ-Wave12-L\n185.CZ-Wave12-B\n186.CZ-Wave13\n187.CZ-Wave13-L\n188.CZ-Wave13-B\n189.CZ-Wave14\n190.CZ-Wave14-L\n191.CZ-Wave14-B\n192.CZ-Wave15\n193.CZ-Wave15-L\n194.CZ-Wave15-B\n195.CZ-Wave16\n196.CZ-Wave16-L\n197.CZ-Wave16-B\n198.CZ-Wave17\n199.CZ-Wave17-L\
200.CZ-Wave17-B\n201.CZ-Wave18\n202.CZ-Wave18-L\n203.CZ-Wave18-B\n204.CZ-Wave19\n205.CZ-Wave19-L\n206.CZ-Wave19-B\n207.CZ-Wave20\n208.CZ-Wave20-L\n209.CZ-Wave20-B\n210.CZ-Wave21\n211.CZ-Wave21-L\n212.CZ-Wave21-B\n213.CZ-Wave22\n214.CZ-Wave22-L\n215.CZ-Wave22-B\n216.CZ-Wave23\n217.CZ-Wave23-L\n218.CZ-Wave23-B\n219.CZ-Wave24\n220.CZ-Wave24-L\n221.CZ-Wave24-B\n222.CZ-Wave25\n223.CZ-Wave25-L\n224.CZ-Wave25-B\n225.CZ-Wave26\n226.CZ-Wave26-L\n227.CZ-Wave26-B\n228.CZ-Wave27\n229.CZ-Wave27-L\n230.CZ-Wave27-B\n231.CZ-Wave28\n232.CZ-Wave28-L\n233.CZ-Wave28-B\n234.CZ-Wave29\n235.CZ-Wave29-L\n236.CZ-Wave29-B\n237.CZ-Wave30\n238.CZ-Wave30-L\n239.CZ-Wave30-B\n240.CZ-Wave31\n241.CZ-Wave31-L\n242.CZ-Wave31-B\n243.CZ-Wave32\n244.CZ-Wave32-L\n245.CZ-Wave32-B\n246.CZ-Wave33\n247.CZ-Wave33-L\n248.CZ-Wave33-B\n249.VA Synth1\n250.VA Synth2\n251.VA Synth3\n252.VA Synth4\n253.VA Synth5\n254.VA Synth6\n255.VA Synth7\n256.VA Synth8\n257.VA Synth9\n258.VA Synth10\n259.VA Synth11\n260.VA Synth12\n261.VA Synth13\n262.VA Synth14\n263.VA Synth15\n264.VA Synth16\n265.VA Synth17\n266.VA Synth18\n267.VA Synth19\n268.TB Saw1-L\n269.TB Saw1-B\n270.TB Saw2-L\n271.TB Saw2-B\n272.TB Saw3-L\n273.TB Saw3-B\n274.TB Pulse1-L\n275.TB Pulse1-B\n276.TB Pulse2-L\n277.TB Pulse2-B\n278.TB Pulse3-L\n279.TB Pulse3-B\n280.TB Bass 1A\n281.TB Bass 1B\n282.TB Bass 1C\n283.TB Bass 2A\n284.TB Bass 2B\n285.TB Bass 2C\n286.MG Bass 1A\n287.MG Bass 1B\n288.MG Bass 1C\n289.MG Bass 2A\n290.MG Bass 2B\n291.MG Bass 2C\n292.SH Saw-L\n293.SH Saw-B\n294.SH Pulse1-L\n295.SH Pulse1-B\n296.SH Pulse2-L\n297.SH Pulse2-B\n298.SH Pulse3-L\n299.SH Pulse3-B\
300.SH Sub OSC-L\n301.SH Sub OSC-B\n302.SH BASS 1\n303.SH BASS 2\n304.SH BASS 3\n305.SH BASS 4\n306.SH BASS 5\n307.SH BASS 6\n308.SH BASS 7\n309.SH BASS 8\n310.SH BASS 9\n311.G1_White Nz\n312.G1_Pink Nz\n313.G1_FilterNz1\n314.G1_FilterNz2\n315.G1_FilterNz3\n316.G1_FilterNz4\n317.G1_FilterNz5\n318.G1_FilterNz6\n319.G1_FilterNz7\n320.G1_FilterNz8\n321.G1_Smpl&Hld1\n322.G1_Smpl&Hld2\n323.G1_ScrtchNz1\n324.G1_ScrtchNz2\n325.G1_PercNoiz1\n326.G1_PercNoiz2\n327.G1_PercNoiz3\n328.G1_PercNoiz4\n329.G1_PercNoiz5\n330.G1_PercNoiz6\n331.G1_PercNoiz7\n332.G1_PercNoiz8\n333.G1_PercNoiz9\n334.G1_PercNz10\n335.G1_Kick 1\n336.G1_Kick 2\n337.G1_Kick 3\n338.G1_Kick 4\n339.G1_Kick 5\n340.G1_Kick 6\n341.G1_Kick 7\n342.G1_Kick 8\n343.G1_Kick 9\n344.G1_Kick 10\n345.G1_Kick 11\n346.G1_Kick 12\n347.G1_Kick 13\n348.G1_Kick 14\n349.G1_Kick 15\n350.G1_Kick 16\n351.G1_Kick 17\n352.G1_Kick 18\n353.G1_Kick 19\n354.G1_Kick 20\n355.G1_Kick 21\n356.G1_Kick 22\n357.G1_Kick 23\n358.G1_Kick 24\n359.G1_Kick 25\n360.G1_Kick 26\n361.G1_Kick 27\n362.G1_Kick 28\n363.G1_Kick 29\n364.G1_Kick 30\n365.G1_Kick 31\n366.G1_Kick 32\n367.G1_Kick 33\n368.G1_Kick 34\n369.G1_Kick 35\n370.G1_Kick 36\n371.G1_Kick 37\n372.G1_Kick 38\n373.G1_Kick 39\n374.G1_Kick 40\n375.G1_Kick 41\n376.G1_Kick 42\n377.G1_Kick 43\n378.G1_Kick 44\n379.G1_Kick 45\n380.G1_Kick 46\n381.G1_Kick 47\n382.G1_Kick 48\n383.G1_Kick 49\n384.G1_Kick 50\n385.G1_Kick 51\n386.G1_Kick 52\n387.G1_Kick 53\n388.G1_Kick 54\n389.G1_Kick 55\n390.G1_Kick 56\n391.G1_Kick 57\n392.G1_Kick 58\n393.G1_Kick 59\n394.G1_Kick 60\n395.G1_Kick 61\n396.G1_Kick 62\n397.G1_Kick 63\n398.G1_Kick 64\n399.G1_Kick 65\
400.G1_Kick 66\n401.G1_Kick 67\n402.G1_Kick 68\n403.G1_Kick 69\n404.G1_Kick 70\n405.G1_Kick 71\n406.G1_Kick 72\n407.G1_Kick 73\n408.G1_Kick 74\n409.G1_Kick 75\n410.G1_Kick 76\n411.G1_Kick 77\n412.G1_Kick 78\n413.G1_Kick 79\n414.G1_Kick 80\n415.G1_Kick 81\n416.G1_Kick 82\n417.G1_Kick 83\n418.G1_Kick 84\n419.G1_Kick 85\n420.G1_Kick 86\n421.G1_Kick 87\n422.G1_Kick 88\n423.G1_Kick 89\n424.G1_Kick 90\n425.G1_Kick 91\n426.G1_Kick 92\n427.G1_Kick 93\n428.G1_Kick 94\n429.G1_Kick 95\n430.G1_Kick 96\n431.G1_Kick 97\n432.G1_Kick 98\n433.G1_Kick 99\n434.G1_RZ1 Kick\n435.G1_Snare 1\n436.G1_Snare 2\n437.G1_Snare 3\n438.G1_Snare 4\n439.G1_Snare 5\n440.G1_Snare 6\n441.G1_Snare 7\n442.G1_Snare 8\n443.G1_Snare 9\n444.G1_Snare 10\n445.G1_Snare 11\n446.G1_Snare 12\n447.G1_Snare 13\n448.G1_Snare 14\n449.G1_Snare 15\n450.G1_Snare 16\n451.G1_Snare 17\n452.G1_Snare 18\n453.G1_Snare 19\n454.G1_Snare 20\n455.G1_Snare 21\n456.G1_Snare 22\n457.G1_Snare 23\n458.G1_Snare 24\n459.G1_Snare 25\n460.G1_Snare 26\n461.G1_Snare 27\n462.G1_Snare 28\n463.G1_Snare 29\n464.G1_Snare 30\n465.G1_Snare 31\n466.G1_Snare 32\n467.G1_Snare 33\n468.G1_Snare 34\n469.G1_Snare 35\n470.G1_Snare 36\n471.G1_Snare 37\n472.G1_Snare 38\n473.G1_Snare 39\n474.G1_Snare 40\n475.G1_Snare 41\n476.G1_Snare 42\n477.G1_Snare 43\n478.G1_Snare 44\n479.G1_Snare 45\n480.G1_Snare 46\n481.G1_Snare 47\n482.G1_Snare 48\n483.G1_Snare 49\n484.G1_Snare 50\n485.G1_Snare 51\n486.G1_Snare 52\n487.G1_Snare 53\n488.G1_Snare 54\n489.G1_Snare 55\n490.G1_Snare 56\n491.G1_Snare 57\n492.G1_Snare 58\n493.G1_Snare 59\n494.G1_Snare 60\n495.G1_Snare 61\n496.G1_Snare 62\n497.G1_Snare 63\n498.G1_Snare 64\n499.G1_Snare 65\
500.G1_Snare 66\n501.G1_Snare 67\n502.G1_Snare 68\n503.G1_Snare 69\n504.G1_Snare 70\n505.G1_Snare 71\n506.G1_Snare 72\n507.G1_Snare 73\n508.G1_Snare 74\n509.G1_Snare 75\n510.G1_Snare 76\n511.G1_Snare 77\n512.G1_Snare 78\n513.G1_Snare 79\n514.G1_Snare 80\n515.G1_Snare 81\n516.G1_Snare 82\n517.G1_Snare 83\n518.G1_Snare 84\n519.G1_Snare 85\n520.G1_Snare 86\n521.G1_Snare 87\n522.G1_Snare 88\n523.G1_Snare 89\n524.G1_Snare 90\n525.G1_Snare 91\n526.G1_Snare 92\n527.G1_Snare 93\n528.G1_Snare 94\n529.G1_Snare 95\n530.G1_Snare 96\n531.G1_Snare 97\n532.G1_Snare 98\n533.G1_Snare 99\n534.G1_RZ1 Snare\n535.G1_Tom 1\n536.G1_Tom 2\n537.G1_Tom 3\n538.G1_Tom 4\n539.G1_Tom 5\n540.G1_Tom 6\n541.G1_Tom 7\n542.G1_Tom 8\n543.G1_Tom 9\n544.G1_Tom 10\n545.G1_Tom 11\n546.G1_Tom 12\n547.G1_Tom 13\n548.G1_Tom 14\n549.G1_Tom 15\n550.G1_Tom 16\n551.G1_Tom 17\n552.G1_RZ1 Tom 1\n553.G1_RZ1 Tom 2\n554.G1_RZ1 Tom 3\n555.G1_Hi-Hat 1\n556.G1_Hi-Hat 2\n557.G1_Hi-Hat 3\n558.G1_Hi-Hat 4\n559.G1_Hi-Hat 5\n560.G1_Hi-Hat 6\n561.G1_Hi-Hat 7\n562.G1_Hi-Hat 8\n563.G1_Hi-Hat 9\n564.G1_Hi-Hat 10\n565.G1_Hi-Hat 11\n566.G1_Hi-Hat 12\n567.G1_Hi-Hat 13\n568.G1_Hi-Hat 14\n569.G1_Hi-Hat 15\n570.G1_Hi-Hat 16\n571.G1_Hi-Hat 17\n572.G1_Hi-Hat 18\n573.G1_Hi-Hat 19\n574.G1_Hi-Hat 20\n575.G1_Hi-Hat 21\n576.G1_Hi-Hat 22\n577.G1_Hi-Hat 23\n578.G1_Hi-Hat 24\n579.G1_Hi-Hat 25\n580.G1_Hi-Hat 26\n581.G1_Hi-Hat 27\n582.G1_Hi-Hat 28\n583.G1_Hi-Hat 29\n584.G1_Hi-Hat 30\n585.G1_Hi-Hat 31\n586.G1_Hi-Hat 32\n587.G1_Hi-Hat 33\n588.G1_Hi-Hat 34\n589.G1_Hi-Hat 35\n590.G1_Hi-Hat 36\n591.G1_Hi-Hat 37\n592.G1_RZ1HiHat1\n593.G1_RZ1HiHat2\n594.G1_RZ1HiHat3\n595.G1_Cymbal 1\n596.G1_Cymbal 2\n597.G1_Cymbal 3\n598.G1_Cymbal 4\n599.G1_Cymbal 5\
600.G1_Cymbal 6\n601.G1_Cymbal 7\n602.G1_Cymbal 8\n603.G1_Clap 1\n604.G1_Clap 2\n605.G1_Clap 3\n606.G1_Clap 4\n607.G1_Clap 5\n608.G1_Clap 6\n609.G1Tambourin1\n610.G1Tambourin2\n611.G1Tambourin3\n612.G1_Cowbell 1\n613.G1_Cowbell 2\n614.G1_Vibraslp1\n615.G1_Bongo 1\n616.G1_Bongo 2\n617.G1_Conga 1\n618.G1_Conga 2\n619.G1_Conga 3\n620.G1_Conga 4\n621.G1_Timbale 1\n622.G1_Maracas 1\n623.G1_Whistle 1\n624.G1_Guiro 1\n625.G1_Guiro 2\n626.G1_Claves 1\n627.G1_WodBlock1\n628.G1_Cuica 1\n629.G1_Cuica 2\n630.G1_Cabasa 1\n631.G1_Triangle1\n632.G1_JinglBel1\n633.G1_BellTree1\n634.G1_Castanet1\n635.G1_Applause2\n636.G1_Applause3\n637.G1_High Q 1\n638.G1_Slap 1\n639.G1_Scratch 1\n640.G1_Scratch 2\n641.G1_Scratch 3\n642.G1_Sticks 1\n643.G1_SqrClick1\n644.G1_SynClick1\n645.G1_SynClick2\n646.G1_Metronom1\n647.G1_Metronom2\n648.G1_Wadaiko 1\n649.G1_Wadaiko 2\n650.G1_Ban Gu 1\n651.G1_HuYinLuo1\n652.G1_Xiao Luo1\n653.G1_Xiao Bo 1\n654.G1_Tang Gu 1\n655.G1_Dholak 1\n656.G1_Dholak 2\n657.G1_Dholak 3\n658.G1_Dholak 4\n659.G1_Dholak 5\n660.G1_Dholak 6\n661.G1_Tabla 1\n662.G1_Tabla 2\n663.G1_Tabla 3\n664.G1_Tabla 4\n665.G1_Tabla 5\n666.G1_Mridangm1\n667.G1_Mridangm2\n668.G1_Mridangm3\n669.G1_Mridangm4\n670.G1_Mridangm5\n671.G1_Darbuka 1\n672.G1_Darbuka 2\n673.G1_Darbuka 3\n674.G1_Darbuka 4\n675.G1_Darbuka 5\n676.G1_Darbuka 6\n677.G1_Darbuka 7\n678.G1_Darbuka 8\n679.G1_Darbuka 9\n680.G1_Darbuka10\n681.G1_Bendir 1\n682.G1_Bendir 2\n683.G1_Bendir 3\n684.G1_Bendir 4\n685.G1_Bendir 5\n686.G1_Bendir 6\n687.G1_Daf 1\n688.G1_Daf 2\n689.G1_Daf 3\n690.G1_Daf 4\n691.G1_Daf 5\n692.G1_Daf 6\n693.G1_Riq 1\n694.G1_Riq 2\n695.G1_Riq 3\n696.G1_Riq 4\n697.G1_Riq 5\n698.G1_Riq 6\n699.G1_Riq 7\
700.G1_Riq 8\n701.G1_Riq 9\n702.G1_Riq 10\n703.G1_Riq 11\n704.G1_Tombak 1\n705.G1_Tombak 2\n706.G1_Tombak 3\n707.G1_Zill 1\n708.G1_Zill 2\n709.G1_Zill 3\n710.G1_Zill 4\n711.G1_Davul 1\n712.G1_Davul 2\n713.G1_Davul 3\n714.G1_Davul 4\n715.G1_Davul 5\n716.G1GroovPerc1\n717.G1GroovPerc2\n718.G1GroovPerc3\n719.G1GroovPerc4\n720.G1GroovPerc5\n721.G1GroovPerc6\n722.G1GroovPerc7\n723.G1GroovPerc8\n724.G1GroovPerc9\n725.G1GrovPerc10\n726.G1GrovPerc11\n727.G1GrovPerc12\n728.G1GrovPerc13\n729.G1GrovPerc14\n730.G1GrovPerc15\n731.G1GrovPerc16\n732.G1GrovPerc17\n733.G1GrovPerc18\n734.G1GrovPerc19\n735.G1GrovPerc20\n736.G1GrovPerc21\n737.G1GrovPerc22\n738.G1GrovPerc23\n739.G1GrovPerc24\n740.G1GrovPerc25\n741.G1GrovPerc26\n742.G1GrovPerc27\n743.G1GrovPerc28\n744.G1GrovPerc29\n745.G1GrovPerc30\n746.G1GrovPerc31\n747.G1GrovPerc32\n748.G1GrovPerc33\n749.G1GrovPerc34\n750.G1GrovPerc35\n751.G1GrovPerc36\n752.G1GrovPerc37\n753.G1GrovPerc38\n754.G1GrovPerc39\n755.G1GrovPerc40\n756.G1GrovPerc41\n757.G1GrovPerc42\n758.G1GrovPerc43\n759.G1GrovPerc44\n760.G1GrovPerc45\n761.G1GrovPerc46\n762.G1GrovPerc47\n763.G1GrovPerc48\n764.G1GrovPerc49\n765.G1GrovPerc50\
"
g_tssPCMwave.P1 = "0.St.GrPno-Lt1\n1.St.GrPno-Lt2\n2.St.GrPno-Lt3\n3.St.GrPno-Rt1\n4.St.GrPno-Rt2\n5.St.GrPno-Rt3\n6.St.BrPno-Lt1\n7.St.BrPno-Lt2\n8.St.BrPno-Lt3\n9.St.BrPno-Rt1\n10.St.BrPno-Rt2\n11.St.BrPno-Rt3\n12.ClascPno-Lt1\n13.ClascPno-Lt2\n14.ClascPno-Lt3\n15.ClascPno-Rt1\n16.ClascPno-Rt2\n17.ClascPno-Rt3\n18.Rock Pno-Lt\n19.Rock Pno-Rt\n20.MdernPno-Pn1\n21.MdernPno-Pn2\n22.MdernPno-Pn3\n23.MdernPno-EP\n24.Dance Piano\n25.StrPiano-Pn1\n26.StrPiano-Pn2\n27.StrPiano-Pn3\n28.StrPiano-Str\n29.PianoPad-Pn1\n30.PianoPad-Pn2\n31.PianoPad-Pn3\n32.PianoPad-Pad\n33.GM Piano 1-1\n34.GM Piano 1-2\n35.GM Piano 1-3\n36.GM Piano 2-1\n37.GM Piano 2-2\n38.GM Piano 2-3\n39.HnkyTonk-Lt1\n40.HnkyTonk-Lt2\n41.HnkyTonk-Lt3\n42.HnkyTonk-Rt1\n43.HnkyTonk-Rt2\n44.HnkyTonk-Rt3\n45.GM HnkyT-A1\n46.GM HnkyT-A2\n47.GM HnkyT-A3\n48.GM HnkyT-B1\n49.GM HnkyT-B2\n50.GM HnkyT-B3\n51.E.Grand-A\n52.E.Grand-B\n53.E.Grand 80\n54.GM Piano 3\n55.Harpsichord\n56.CouplHps-A\n57.CouplHps-B\n58.GM Harpsi.\n59.Elec.Piano-1\n60.Elec.Piano-2\n61.FM E.Piano\n62.60âs EP-1\n63.60âs EP-2\n64.Dyno EP1-1\n65.Dyno EP1-2\n66.Dyno EP2-A1\n67.Dyno EP2-B1\n68.Dyno EP2-A2\n69.Dyno EP2-B2\n70.MellowEP-A\n71.MellowEP-B\n72.Pop EP-A\n73.Pop EP-B\n74.Chorus EP\n75.Trem.EP1-1\n76.Trem.EP1-2\n77.Trem.EP2-1\n78.Trem.EP2-2\n79.ModernEP-A\n80.ModernEP-B1\n81.ModernEP-B2\n82.SynStrEP-EP1\n83.SynStrEP-EP2\n84.SynStrEP-Str\n85.GM E.Pno1-1\n86.GM E.Pno1-2\n87.GM E.Piano 2\n88.VintageClavi\n89.Clavi\n90.Wah Clavi\n91.Dist.Clavi\n92.GM Clavi\n93.Vibraphone\n94.TremoloVibes\n95.Marimba\n96.Glockenspiel\n97.Music Box-A\n98.Music Box-B\n99.Tubular Bell\
100.Dulcimer-A\n101.Dulcimer-B\n102.GM Celesta\n103.GM Glocken.\n104.GM MusicB-A\n105.GM MusicB-B\n106.GM Vibraphon\n107.GM Marimba\n108.GM Xylophone\n109.GM TublarBel\n110.GM Dulcimr-A\n111.GM Dulcimr-B\n112.DrawbarOrg 1\n113.DrawbarOrg 2\n114.Perc.Organ-A\n115.Perc.Organ-B\n116.Elec.Organ\n117.Jazz Organ-A\n118.Jazz Organ-B\n119.Rock Organ-A\n120.Rock Organ-B\n121.Dist.RockOrg\n122.70âs Organ\n123.Full Drawbar\n124.Rotary Organ\n125.TremoloOrgan\n126.ClickOrg-A\n127.ClickOrg-B\n128.8âOrgan-A\n129.8âOrgan-B\n130.ChrchOrg-A\n131.ChrchOrg-B\n132.ChaplOrg-A\n133.ChaplOrg-B\n134.Pipe Organ-A\n135.Pipe Organ-B\n136.Theater-A\n137.Theater-B\n138.Reed Organ\n139.Accordion-A\n140.Accordion-B\n141.Harmonica\n142.GM Organ 1\n143.GM Organ 2-A\n144.GM Organ 2-B\n145.GM Organ 3-A\n146.GM Organ 3-B\n147.GM PipeOrg-A\n148.GM PipeOrg-B\n149.GM ReedOrgan\n150.GM Acordon-A\n151.GM Acordon-B\n152.GM Harmonica\n153.GM Bndneon-A\n154.GM Bndneon-B\n155.St.Str 1-Lt\n156.St.Str 1-Rt\n157.St.Str 2-Lt\n158.St.Str 2-Rt\n159.Strings\n160.StrEnsemble1\n161.StrEnsemble2\n162.BriteStrings\n163.Slow Strings\n164.Wide Str-Lt\n165.Wide Str-Rt\n166.Chamber-A\n167.Chamber-B\n168.Syn-Strings1\n169.Syn-Strings2\n170.70âs Syn-Str\n171.Fast Syn-Str\n172.Slow Syn-Str\n173.PhaserSynStr\n174.Cho.Syn-Str\n175.Violin\n176.Harp\n177.Choir Aahs\n178.StrVoice-Cho\n179.StrVoice-Str\n180.Voice Doo\n181.Synth-Voice1\n182.Synth-Voice2\n183.Voi.Ens.-Voi\n184.Voi.Ens.-Str\n185.Orch.Hit 1-A\n186.Orch.Hit 1-B\n187.Orch.Hit 2-A\n188.Orch.Hit 2-B\n189.GM Violin\n190.GM Viola\n191.GM Cello\n192.GM Contrabas\n193.GM Trem.Str.\n194.GM Pizzicato\n195.GM Harp\n196.GM Timpani\n197.GM Strings 1\n198.GM Strings 2\n199.GM SynthStr1\
200.GM SynthStr2\n201.GM ChoirAahs\n202.GM Voice Doo\n203.GM Syn-Voice\n204.GM OrchHit-A\n205.GM OrchHit-B\n206.Trumpet\n207.Mute Trumpet\n208.Trombone\n209.Tuba\n210.Fr.Horn-A\n211.Fr.Horn-B\n212.St.Brass-Lt\n213.St.Brass-Rt\n214.Brass-A\n215.Brass-B\n216.BrasSect-A\n217.BrasSect-B\n218.Syn-Brs1-A\n219.Syn-Brs1-B\n220.Synth-Brass2\n221.WarmSyBr-A\n222.WarmSyBr-B\n223.80sSyBrs-A\n224.80sSyBrs-B\n225.A.Syn-Brass\n226.TrnceBrs-A\n227.TrnceBrs-B\n228.GM Trumpet\n229.GM Trombone\n230.GM Tuba\n231.GM MtTrumpet\n232.GM Fr.Horn-A\n233.GM Fr.Horn-B\n234.GM Brass-A\n235.GM Brass-B\n236.GM SynBrs1-A\n237.GM SynBrs1-B\n238.GM SynBrs2-A\n239.GM SynBrs2-B\n240.Alto Sax\n241.BrtyASax-A1\n242.BrtyASax-A2\n243.BrtyASax-Nz\n244.Tenor Sax\n245.BrtyTSax-A1\n246.BrtyTSax-A2\n247.BrtyTSax-Nz\n248.Soprano Sax\n249.Baritone Sax\n250.Hard A.Sax-A\n251.Hard A.Sax-B\n252.T.Saxys-A\n253.T.Saxys-B\n254.A.Saxys-A\n255.A.Saxys-B\n256.Clarinet\n257.Oboe\n258.Bassoon\n259.GM Sop.Sax\n260.GM Alto Sax\n261.GM Tenor Sax\n262.GM Bar.Sax\n263.GM Oboe\n264.GM Eng.Horn\n265.GM Bassoon\n266.GM Clarinet\n267.Flute\n268.Piccolo-A\n269.Piccolo-B\n270.Recorder\n271.Pan Flute\n272.BotlBlow-A\n273.BotlBlow-B\n274.Whistle\n275.Ocarina\n276.Shakuhachi-A\n277.Shakuhachi-B\n278.GM Piccolo\n279.GM Flute\n280.GM Recorder\n281.GM Pan Flute\n282.GM BotlBlw-A\n283.GM BotlBlw-B\n284.GM Shkhchi-A\n285.GM Shkhchi-B\n286.GM Whistle\n287.GM Ocarina\n288.Nylon Gt-1\n289.Nylon Gt-2\n290.AmbNylGt-1\n291.AmbNylGt-2\n292.Steel Gt-1\n293.Steel Gt-2\n294.AmbStlGt-1\n295.AmbStlGt-2\n296.12Str.Gt-A\n297.12Str.Gt-B\n298.ChoStlGt-1\n299.ChoStlGt-2\
300.Jazz Guitar\n301.CleanGt1-1\n302.CleanGt1-2\n303.CleanGuitar2\n304.ChoClean-1\n305.ChoClean-2\n306.Wah E.Guitar\n307.Mute Gt-1\n308.Mute Gt-2\n309.Mute Ovd Gt\n310.Crunch E.Gt1\n311.Crunch E.Gt2\n312.Overdrive Gt\n313.DistortionGt\n314.More Dist.Gt\n315.Power Gt-A\n316.Power Gt-B\n317.FlangrDistGt\n318.GM Nylon Gt\n319.GM Steel Gt\n320.GM Jazz Gt\n321.GM CleanGt-1\n322.GM CleanGt-2\n323.GM Mute Gt-1\n324.GM Mute Gt-2\n325.GM Overdrv-A\n326.GM Overdrv-B\n327.GM Dist.Gt\n328.GM Gt Harm.\n329.AcousticBass\n330.RideBass-Bs\n331.RideBass-Rid\n332.FingrBs1-1\n333.FingrBs1-2\n334.FingerBass 2\n335.MellowF.Bass\n336.Picked Bass\n337.RthmPickBass\n338.Fretless-1\n339.Fretless-2\n340.Slap Bass\n341.Synth-Bass 1\n342.SynBass2-1\n343.SynBass2-2\n344.SynBass3-1\n345.SynBass3-2\n346.SynBass3-3\n347.SynBass3-4\n348.SynBass4-1\n349.SynBass4-2\n350.Synth-Bass 5\n351.Synth-Bass 6\n352.Synth-Bass 7\n353.SawSyBs1-A\n354.SawSyBs1-B\n355.SawSyBs2-1\n356.SawSyBs2-2\n357.SawSyBs2-3\n358.SawSyBs2-4\n359.Sqr Syn-Bass\n360.DigiRockBass\n361.Trance Bass\n362.Bs&Kick-Bs\n363.Bs&Kick-Kck\n364.Vocoder Bass\n365.SoulSyn-Bass\n366.GM AcousBs-1\n367.GM AcousBs-2\n368.GM Fing.Bs-1\n369.GM Fing.Bs-2\n370.GM Pick Bs-1\n371.GM Pick Bs-2\n372.GM FrtlsBs-1\n373.GM FrtlsBs-2\n374.GM SlapBass1\n375.GM SlapBs2-1\n376.GM SlapBs2-2\n377.GM Syn-Bass1\n378.GM Syn-Bass2\n379.SqrLead1-A\n380.SqrLead1-B\n381.SqrLead2-A\n382.SqrLead2-B\n383.Square Lead3\n384.Square Lead4\n385.SqrPlsLd-A\n386.SqrPlsLd-B\n387.Sine Lead\n388.Saw Lead 1-A\n389.Saw Lead 1-B\n390.Saw Lead 2\n391.Saw Lead 3\n392.MlwSawLd-A\n393.MlwSawLd-B\n394.PlsSawLd-A\n395.PlsSawLd-B\n396.SS Lead-A\n397.SS Lead-B\n398.SlwSqrLd-A\n399.SlwSqrLd-B\
400.SlwSawLd-A\n401.SlwSawLd-B\n402.Seq.Square-A\n403.Seq.Square-B\n404.Seq.Pulse-A\n405.Seq.Pulse-B\n406.Seq.Sine-A\n407.Seq.Saw1-A\n408.Seq.Saw1-B\n409.Seq.Saw2-A\n410.Seq.Saw2-B\n411.TranceLd-A\n412.TranceLd-B\n413.VA SynthLead\n414.VA Synth 1\n415.VA Synth 2\n416.VA Synth 3\n417.VA Synth 4\n418.VA Synth 5\n419.VA Synth 6\n420.VA Syn-SqBs1\n421.VA Syn-SqBs2\n422.VA Syn-SqBs3\n423.VA Syn-SqBs4\n424.VA Syn-SqBs5\n425.VA Syn-SqBs6\n426.Calliope-A\n427.Calliope-B\n428.Vent Lead-A\n429.Vent Lead-B\n430.Vent Synth-A\n431.Vent Synth-B\n432.Seq.Lead-A\n433.Seq.Lead-B\n434.Drop Lead-A\n435.Drop Lead-B\n436.EP Lead-A\n437.EP Lead-B\n438.Voice Lead-A\n439.Voice Lead-B\n440.Vox Lead-A\n441.Vox Lead-B\n442.PluckLd1-A\n443.PluckLd1-B\n444.PluckLd2-A\n445.PluckLd2-B\n446.Gt SynLd-A\n447.Gt SynLd-B\n448.DblVoiLd-A\n449.DblVoiLd-B\n450.VoiChoLd-A\n451.VoiChoLd-B\n452.SynVoiLd-A\n453.SynVoiLd-B\n454.FifthSaw-A\n455.FifthSaw-B\n456.FifthSqr-A\n457.FifthSqr-B\n458.Fifth Seq.-A\n459.Fifth Seq.-B\n460.SynBs+Ld-A\n461.SynBs+Ld-B\n462.Reed Lead-A\n463.Reed Lead-B\n464.Fret Lead-A\n465.Fret Lead-B\n466.Fantasy 1-A\n467.Fantasy 1-B\n468.Fantasy 2-A\n469.Fantasy 2-B\n470.New Age-A\n471.New Age-B\n472.NewAgePd-A\n473.NewAgePd-B\n474.Warm Vox-A\n475.Warm Vox-B\n476.Thick Pad-A\n477.Thick Pad-B\n478.Warm Pad\n479.Sine Pad-A\n480.Sine Pad-B\n481.Soft Pad-A\n482.Soft Pad-B\n483.Horn Pad-A\n484.Horn Pad-B\n485.OldTpPad-A\n486.OldTpPad-B\n487.PolySyn1-A\n488.PolySyn1-B\n489.PolySyn2-A\n490.PolySyn2-B\n491.PolyPad1-A\n492.PolyPad1-B\n493.PolyPad2-A\n494.PolyPad2-B\n495.Poly Saw-A\n496.Poly Saw-B\n497.Heaven-A\n498.Heaven-B\n499.SpcStrPd-A\
500.SpcStrPd-B\n501.ChiffCho-A\n502.ChiffCho-B\n503.Star Voice-A\n504.Star Voice-B\n505.Square Pad-A\n506.Square Pad-B\n507.Glass Pad-A\n508.Glass Pad-B\n509.Bottle Pad-A\n510.Bottle Pad-B\n511.Ethnic Pad-A\n512.Ethnic Pad-B\n513.Metal Pad-A\n514.Metal Pad-B\n515.Halo Pad-A\n516.Halo Pad-B\n517.Chorus Pad-A\n518.Chorus Pad-B\n519.OrgChoPd-A\n520.OrgChoPd-B\n521.SweepCho-A\n522.SweepCho-B\n523.Rain Drop-A\n524.Rain Drop-B\n525.Wood Pad-A\n526.Wood Pad-B\n527.SpaceVoi-A\n528.SpaceVoi-B\n529.SoundTrk-A\n530.SoundTrk-B\n531.XmasBell-A\n532.XmasBell-B\n533.GlockChi-A\n534.GlockChi-B\n535.Vibes Bell-A\n536.Vibes Bell-B\n537.ChorBell-A\n538.ChorBell-B\n539.SyMallet-A\n540.SyMallet-B\n541.CelestPd-A\n542.CelestPd-B\n543.Steel Pad-A\n544.Steel Pad-B\n545.BrtBelPd-A\n546.BrtBelPd-B\n547.Britnes1-A\n548.Britnes1-B\n549.Britnes2-A\n550.Britnes2-B\n551.Echo Drop-A\n552.Echo Drop-B\n553.Poly Drop-A\n554.Poly Drop-B\n555.Star Theme-A\n556.Star Theme-B\n557.Space Pad-A\n558.Space Pad-B\n559.GM SqrLead-1\n560.GM SqrLead-2\n561.GM SawLead-1\n562.GM SawLead-2\n563.GM Caliope-1\n564.GM Caliope-2\n565.GM ChiffLd-1\n566.GM ChiffLd-2\n567.GM Charang-1\n568.GM Charang-2\n569.GM VoiceLd-1\n570.GM VoiceLd-2\n571.GM FifthLd-1\n572.GM FifthLd-2\n573.GM Bs+Lead-1\n574.GM Bs+Lead-2\n575.GM Fantasy-1\n576.GM Fantasy-2\n577.GM Warm Pad\n578.GM PolySyn-1\n579.GM PolySyn-2\n580.GM SpacCho-1\n581.GM SpacCho-2\n582.GM BowGlas-1\n583.GM BowGlas-2\n584.GM MetalPd-1\n585.GM MetalPd-2\n586.GM HaloPad-1\n587.GM HaloPad-2\n588.GM SweepPd-1\n589.GM SweepPd-2\n590.GM RainDrp-1\n591.GM RainDrp-2\n592.GM SoundTr-1\n593.GM SoundTr-2\n594.GM Crystal-1\n595.GM Crystal-2\n596.GM Atmosph-1\n597.GM Atmosph-2\n598.GM Britnes-1\n599.GM Britnes-2\
600.GM Goblins-1\n601.GM Goblins-2\n602.GM Echoes-1\n603.GM Echoes-2\n604.GM SF-1\n605.GM SF-2\n606.Er Hu-1\n607.Er Hu-2\n608.Yang Qin\n609.Di Zi\n610.Pi Pa\n611.Sitar\n612.Tanpura\n613.Harmonium-1\n614.Harmonium-2\n615.Santur-1\n616.Santur-2\n617.Kanun\n618.Oud\n619.Saz\n620.Bouzouki\n621.GM Sitar\n622.GM Banjo\n623.GM Shamisen\n624.GM Koto\n625.GM Thumb Pno\n626.GM Bagpipe-1\n627.GM Bagpipe-2\n628.GM Fiddle\n629.GM Shanai\n630.GM TinkleBel\n631.GM Agogo\n632.GM SteelDr-1\n633.GM SteelDr-2\n634.GM WoodBlock\n635.GM Taiko\n636.GM Melo.Tom\n637.GM SynthDrum\n638.GM RevCymbal\n639.GM GtFrNoise\n640.GM BrthNoise\n641.GM Seashor-1\n642.GM Seashor-2\n643.GM Bird-1\n644.GM Bird-2\n645.GM Telephone\n646.GM Helicoptr\n647.GM Aplause-1\n648.GM Aplause-2\n649.GM Gunshot\n650.LAY Saw Lead\n651.LAY Saw Wave\n652.Sy_Sin Wave\n653.Sy_Triangle\n654.Sy_Sawtooth\n655.Sy_ReversSaw\n656.Sy_Square\n657.Sy_MM Triang\n658.Sy_MM Ramp\n659.Sy_MM Saw\n660.Sy_MM Square\n661.Sy_MM WidPls\n662.Sy_MM NrwPls\n663.Sy_MG Sin\n664.Sy_MG Triang\n665.Sy_MG Saw\n666.Sy_MG Square\n667.Sy_MG Pulse1\n668.Sy_MG Pulse2\n669.Sy_AP1 Saw\n670.Sy_AP1 Squar\n671.Sy_AP1 Tri\n672.Sy_AP1 Puls1\n673.Sy_AP1 Puls2\n674.Sy_AP1 Puls3\n675.Sy_AP2 Saw\n676.Sy_AP2 Puls1\n677.Sy_AP2 Puls2\n678.Sy_AP2SycSaw\n679.Sy_AP2SycPls\n680.Sy_OB Saw\n681.Sy_OB Pulse1\n682.Sy_OB Pulse2\n683.Sy_OBSyncSaw\n684.Sy_OBSyncPls\n685.Sy_P5 Triang\n686.Sy_P5 Saw\n687.Sy_P5 Pulse1\n688.Sy_P5 Pulse2\n689.Sy_P5 Pulse3\n690.Sy_P5 Pulse4\n691.Sy_ND Saw\n692.Sy_ND Pulse1\n693.Sy_ND Pulse2\n694.Sy_ND Pulse3\n695.Sy_ND FM\n696.Sy_JP Saw\n697.Sy_JP Square\n698.Sy_JP Pulse\n699.Sy_CZ Saw\
700.Sy_CZ Square\n701.Sy_CZ Pulse\n702.Sy_CZ DblSin\n703.Sy_CZ SawPls\n704.Sy_CZ SawRes\n705.Sy_CZ TriRes\n706.Sy_CZ TraRes\n707.Sy_CZ Wave9\n708.Sy_CZ Wave10\n709.Sy_CZ Wave11\n710.Sy_CZ Wave12\n711.Sy_CZ Wave13\n712.Sy_CZ Wave14\n713.Sy_CZ Wave15\n714.Sy_CZ Wave16\n715.Sy_CZ Wave17\n716.Sy_CZ Wave18\n717.Sy_CZ Wave19\n718.Sy_CZ Wave20\n719.Sy_CZ Wave21\n720.Sy_CZ Wave22\n721.Sy_CZ Wave23\n722.Sy_CZ Wave24\n723.Sy_CZ Wave25\n724.Sy_CZ Wave26\n725.Sy_CZ Wave27\n726.Sy_CZ Wave28\n727.Sy_CZ Wave29\n728.Sy_CZ Wave30\n729.Sy_CZ Wave31\n730.Sy_CZ Wave32\n731.Sy_CZ Wave33\n732.Sy_VA Synth1\n733.Sy_VA Synth2\n734.Sy_VA Synth3\n735.Sy_VA Synth4\n736.Sy_VA Synth5\n737.Sy_VA Synth6\n738.Sy_VA Synth7\n739.Sy_VA Synth8\n740.Sy_VA Synth9\n741.Sy_VA Syn 10\n742.Sy_VA Syn 11\n743.Sy_VA Syn 12\n744.Sy_VA Syn 13\n745.Sy_VA Syn 14\n746.Sy_VA Syn 15\n747.Sy_VA Syn 16\n748.Sy_VA Syn 17\n749.Sy_VA Syn 18\n750.Sy_VA Syn 19\n751.Sy_TB Bass 1\n752.Sy_TB Bass 2\n753.Sy_MG Bass 1\n754.Sy_MG Bass 2\n755.Sy_TB Saw 1\n756.Sy_TB Saw 2\n757.Sy_TB Saw 3\n758.Sy_TB Pulse1\n759.Sy_TB Pulse2\n760.Sy_TB Pulse3\n761.Sy_SH Saw\n762.Sy_SH Pulse1\n763.Sy_SH Pulse2\n764.Sy_SH Pulse3\n765.Sy_SH SubOSC\n766.Sy_SH BASS 1\n767.Sy_SH BASS 2\n768.Sy_SH BASS 3\n769.Sy_SH BASS 4\n770.Sy_SH BASS 5\n771.Sy_SH BASS 6\n772.Sy_SH BASS 7\n773.Sy_SH BASS 8\n774.Sy_SH BASS 9\n775.Nz_WhiteNoiz\n776.Nz_PinkNoise\n777.Nz_FltrNoiz1\n778.Nz_FltrNoiz2\n779.Nz_FltrNoiz3\n780.Nz_FltrNoiz4\n781.Nz_FltrNoiz5\n782.Nz_FltrNoiz6\n783.Nz_FltrNoiz7\n784.Nz_FltrNoiz8\n785.Nz_Smpl&Hld1\n786.Nz_Smpl&Hld2\n787.Nz_Scratch1\n788.Nz_Scratch2\n789.Piano 1-B\n790.Piano 2-B\n791.Piano 3-B\n792.Piano 4-B\n793.Piano 5-B\n794.Piano 6-B\n795.Piano 7-B\n796.Piano 8-L\n797.Piano 9-L\n798.Piano 10-L\n799.Piano 11\
800.Piano 12\n801.Piano 13\n802.Piano 14\n803.Piano 15\n804.Piano 16\n805.Piano 17\n806.Piano 18\n807.Piano 19\n808.Piano 20\n809.Piano 21\n810.Piano 22\n811.Piano 23\n812.Piano 24\n813.Piano 25\n814.Piano 26-B\n815.Piano 27-B\n816.Piano 28-B\n817.Piano 29-B\n818.Piano 30-B\n819.Piano 31-B\n820.Piano 32-B\n821.Piano 33-L\n822.Piano 34-L\n823.Piano 35-L\n824.Piano 36\n825.Piano 37\n826.Piano 38\n827.Piano 39\n828.Piano 40\n829.Piano 41\n830.Piano 42\n831.Piano 43\n832.Piano 44\n833.Piano 45\n834.Piano 46\n835.Piano 47\n836.Piano 48\n837.Piano 49\n838.Piano 50\n839.Piano 51-B\n840.Piano 52-B\n841.Piano 53-B\n842.Piano 54-B\n843.Piano 55-B\n844.Piano 56-B\n845.Piano 57-B\n846.Piano 58-L\n847.Piano 59-L\n848.Piano 60-L\n849.Piano 61\n850.Piano 62\n851.Piano 63\n852.Piano 64\n853.Piano 65\n854.Piano 66\n855.Piano 67\n856.Piano 68\n857.Piano 69\n858.Piano 70\n859.Piano 71\n860.Piano 72\n861.Piano 73\n862.Piano 74\n863.Piano 75\n864.Piano 76-B\n865.Piano 77-B\n866.Piano 78-B\n867.Piano 79-B\n868.Piano 80-B\n869.Piano 81-B\n870.Piano 82-B\n871.Piano 83-L\n872.Piano 84-L\n873.Piano 85-L\n874.Piano 86\n875.Piano 87\n876.Piano 88\n877.Piano 89\n878.Piano 90\n879.Piano 91\n880.Piano 92\n881.Piano 93\n882.Piano 94\n883.Piano 95\n884.Piano 96\n885.Piano 97\n886.Piano 98\n887.Piano 99\n888.Piano 100\n889.Piano 101-B\n890.Piano 102-B\n891.Piano 103-B\n892.Piano 104-B\n893.Piano 105-B\n894.Piano 106-B\n895.Piano 107-B\n896.Piano 108-L\n897.Piano 109-L\n898.Piano 110-L\n899.Piano 111\
900.Piano 112\n901.Piano 113\n902.Piano 114\n903.Piano 115\n904.Piano 116\n905.Piano 117\n906.Piano 118\n907.Piano 119\n908.Piano 120\n909.Piano 121\n910.Piano 122\n911.Piano 123\n912.Piano 124\n913.Piano 125\n914.Piano 126-B\n915.Piano 127-B\n916.Piano 128-B\n917.Piano 129-B\n918.Piano 130-B\n919.Piano 131-B\n920.Piano 132-B\n921.Piano 133-L\n922.Piano 134-L\n923.Piano 135-L\n924.Piano 136\n925.Piano 137\n926.Piano 138\n927.Piano 139\n928.Piano 140\n929.Piano 141\n930.Piano 142\n931.Piano 143\n932.Piano 144\n933.Piano 145\n934.Piano 146\n935.Piano 147\n936.Piano 148\n937.Piano 149\n938.Piano 150\n939.E.Piano 1-B\n940.E.Piano 2-L\n941.E.Piano 3\n942.E.Piano 4\n943.E.Piano 5\n944.E.Piano 6-L\n945.E.Piano 7-L\n946.E.Piano 8\n947.E.Piano 9\n948.E.Piano 10\n949.E.Piano 11\n950.E.Piano 12\n951.E.Piano 13-B\n952.E.Piano 14-L\n953.E.Piano 15-L\n954.E.Piano 16\n955.E.Piano 17\n956.E.Piano 18\n957.E.Piano 19\n958.E.Piano 20\n959.E.Piano 21\n960.E.Piano 22\n961.E.Piano 23\n962.E.Piano 24\n963.E.Piano 25\n964.E.Piano 26\n965.E.Piano 27\n966.E.Piano 28-L\n967.E.Piano 29-L\n968.E.Piano 30\n969.E.Piano 31\n970.E.Piano 32\n971.E.Piano 33\n972.E.Piano 34\n973.E.Piano 35-L\n974.E.Piano 36-L\n975.E.Piano 37\n976.E.Piano 38\n977.E.Piano 39\n978.E.Piano 40\n979.E.Piano 41\n980.E.Piano 42\n981.E.Piano 43\n982.E.Piano 44-L\n983.E.Piano 45-L\n984.E.Piano 46\n985.E.Piano 47\n986.E.Piano 48\n987.E.Piano 49\n988.E.Piano 50\n989.E.Piano 51\n990.E.Piano 52-B\n991.E.Piano 53-L\n992.E.Piano 54-L\n993.E.Piano 55\n994.E.Piano 56\n995.E.Piano 57\n996.E.Piano 58\n997.Harpsi. 1-B\n998.Harpsi. 2-L\n999.Harpsi. 3-L\
1000.Harpsi. 4\n1001.Harpsi. 5\n1002.Harpsi. 6\n1003.Clavi 1-B\n1004.Clavi 2-L\n1005.Clavi 3\n1006.Clavi 4\n1007.Clavi 5\n1008.Clavi 6-B\n1009.Clavi 7-B\n1010.Clavi 8-B\n1011.Clavi 9-L\n1012.Clavi 10-L\n1013.Clavi 11-L\n1014.Clavi 12\n1015.Clavi 13\n1016.Clavi 14\n1017.Clavi 15\n1018.Clavi 16\n1019.Clavi 17\n1020.Celesta 1\n1021.Glocken. 1\n1022.Glocken. 2\n1023.Glocken. 3\n1024.Vibes 1\n1025.Vibes 2\n1026.Vibes 3\n1027.Vibes 4\n1028.Marimba 1\n1029.Marimba 2\n1030.Marimba 3\n1031.Marimba 4\n1032.Xylophon 1\n1033.Xylophon 2\n1034.Xylophon 3\n1035.Xylophon 4\n1036.Tubulbel 1\n1037.Tubulbel 2\n1038.Organ 1-L\n1039.Organ 2\n1040.Organ 3\n1041.Organ 4\n1042.Organ 5-L\n1043.Organ 6\n1044.Organ 7-B\n1045.Organ 8-L\n1046.Organ 9\n1047.Organ 10\n1048.Organ 11-L\n1049.Organ 12\n1050.Organ 13\n1051.Organ 14\n1052.Organ 15\n1053.Organ 16\n1054.Organ 17\n1055.Organ 18\n1056.Organ 19\n1057.Organ 20\n1058.Organ 21\n1059.Organ 22\n1060.Organ 23\n1061.Organ 24\n1062.Organ 25\n1063.Organ 26-L\n1064.Organ 27-L\n1065.Organ 28\n1066.Organ 29\n1067.Organ 30\n1068.Organ 31\n1069.Organ 32\n1070.Organ 33\n1071.Organ 34\n1072.Organ 35\n1073.Organ 36-B\n1074.Organ 37-L\n1075.Organ 38-L\n1076.Organ 39\n1077.Organ 40\n1078.Organ 41\n1079.Organ 42\n1080.Organ 43\n1081.Organ 44-B\n1082.Organ 45-L\n1083.Organ 46-L\n1084.Organ 47\n1085.Organ 48\n1086.Organ 49\n1087.Organ 50\n1088.Organ 51\n1089.Organ 52\n1090.Pipe Org 1-B\n1091.Pipe Org 2-L\n1092.Pipe Org 3\n1093.Pipe Org 4\n1094.Pipe Org 5\n1095.Pipe Org 6\n1096.Pipe Org 7\n1097.Pipe Org 8-L\n1098.Pipe Org 9\n1099.Pipe Org 10\
1100.Pipe Org 11\n1101.Pipe Org 12\n1102.Acordion 1-L\n1103.Acordion 2-L\n1104.Acordion 3\n1105.Acordion 4\n1106.Acordion 5\n1107.Bandneon 1-L\n1108.Bandneon 2\n1109.Bandneon 3\n1110.Harmnica 1-L\n1111.Harmnica 2-L\n1112.Harmnica 3\n1113.Harmnica 4\n1114.Nylon Gt 1-B\n1115.Nylon Gt 2-L\n1116.Nylon Gt 3-L\n1117.Nylon Gt 4-L\n1118.Nylon Gt 5\n1119.Nylon Gt 6\n1120.Steel Gt 1-B\n1121.Steel Gt 2-L\n1122.Steel Gt 3-L\n1123.Steel Gt 4\n1124.Steel Gt 5\n1125.Steel Gt 6\n1126.Steel Gt 7\n1127.Steel Gt 8\n1128.Steel Gt 9\n1129.Steel Gt 10\n1130.Jazz Gt 1-L\n1131.Jazz Gt 2\n1132.Jazz Gt 3\n1133.Jazz Gt 4\n1134.Jazz Gt 5\n1135.Elec.Gt 1-L\n1136.Elec.Gt 2-L\n1137.Elec.Gt 3-L\n1138.Elec.Gt 4\n1139.Elec.Gt 5\n1140.Elec.Gt 6\n1141.Elec.Gt 7\n1142.Elec.Gt 8\n1143.Elec.Gt 9-B\n1144.Elec.Gt 10-L\n1145.Elec.Gt 11-L\n1146.Elec.Gt 12-L\n1147.Elec.Gt 13\n1148.Elec.Gt 14\n1149.Elec.Gt 15\n1150.Elec.Gt 16\n1151.Mute Gt 1-L\n1152.Mute Gt 2\n1153.Mute Gt 3\n1154.Mute Gt 4\n1155.Ovrdrive 1-L\n1156.Ovrdrive 2-L\n1157.Ovrdrive 3\n1158.Ovrdrive 4\n1159.Ovrdrive 5\n1160.Ovrdrive 6\n1161.Ovrdrive 7-B\n1162.Ovrdrive 8-L\n1163.Ovrdrive 9-L\n1164.Ovrdrive 10\n1165.Ovrdrive 11\n1166.Ovrdrive 12\n1167.Ovrdrive 13\n1168.Ovrdrive 14\n1169.Ovrdrive15-L\n1170.Ovrdrive16-L\n1171.Ovrdrive17-L\n1172.Ovrdrive18-L\n1173.Ovrdrive19-L\n1174.Ovrdrive 20\n1175.Ovrdrive 21\n1176.Ovrdrive 22\n1177.Ovrdrive 23\n1178.Ovrdrive 24\n1179.Ovrdrive 25\n1180.Ovrdrive 26\n1181.Ovrdrive27-B\n1182.Ovrdrive28-B\n1183.Ovrdrive29-B\n1184.Ovrdrive30-L\n1185.Ovrdrive31-L\n1186.Ovrdrive32-L\n1187.Ovrdrive 33\n1188.Ovrdrive 34\n1189.Ovrdrive 35\n1190.Ovrdrive 36\n1191.Ovrdrive 37\n1192.Ovrdrive 38\n1193.Ovrdrive 39\n1194.Ovrdrive 40\n1195.Dist.Gt 1-B\n1196.Dist.Gt 2-L\n1197.Dist.Gt 3-L\n1198.Dist.Gt 4-L\n1199.Dist.Gt 5\
1200.Dist.Gt 6\n1201.Dist.Gt 7\n1202.Dist.Gt 8-B\n1203.Dist.Gt 9-L\n1204.Dist.Gt 10-L\n1205.Dist.Gt 11-L\n1206.Dist.Gt 12-L\n1207.Dist.Gt 13\n1208.Dist.Gt 14\n1209.Dist.Gt 15\n1210.GtHrmncs 1-L\n1211.GtHrmncs 2\n1212.Acous.Bs 1-B\n1213.Acous.Bs 2-B\n1214.Acous.Bs 3-B\n1215.Acous.Bs 4-B\n1216.Acous.Bs 5-B\n1217.Acous.Bs 6-L\n1218.Acous.Bs 7-L\n1219.Acous.Bs 8\n1220.FingerBs 1-B\n1221.FingerBs 2-B\n1222.FingerBs 3-B\n1223.FingerBs 4-B\n1224.FingerBs 5-B\n1225.FingerBs 6-L\n1226.FingerBs 7\n1227.FingerBs 8-B\n1228.FingerBs 9-B\n1229.FingerBs10-B\n1230.FingerBs11-B\n1231.FingerBs12-B\n1232.FingerBs13-L\n1233.FingerBs 14\n1234.FingerBs15-B\n1235.FingerBs16-B\n1236.FingerBs17-L\n1237.FingerBs 18\n1238.FingerBs 19\n1239.PickBass 1-B\n1240.PickBass 2-L\n1241.PickBass 3-L\n1242.PickBass 4\n1243.PickBass 5\n1244.PickBass 6-B\n1245.PickBass 7-B\n1246.PickBass 8-L\n1247.PickBass 9-L\n1248.PickBass 10\n1249.PickBass 11\n1250.PickBass 12\n1251.PickBass 13\n1252.Fretless 1-L\n1253.Fretless 2\n1254.Fretless 3\n1255.SlapBass 1-B\n1256.SlapBass 2-B\n1257.SlapBass 3-B\n1258.SlapBass 4\n1259.SlapBass 5\n1260.SlapBass 6-B\n1261.SlapBass 7-L\n1262.SlapBass 8\n1263.Syn-Bass 1-B\n1264.Syn-Bass 2-B\n1265.Syn-Bass 3-B\n1266.Syn-Bass 4-L\n1267.Syn-Bass 5\n1268.Syn-Bass 6\n1269.Syn-Bass 7-L\n1270.Syn-Bass 8\n1271.Syn-Bass 9-L\n1272.Syn-Bass10-L\n1273.Syn-Bass 11\n1274.Syn-Bass 12\n1275.Syn-Bass 13\n1276.Syn-Bass 14\n1277.Syn-Bass15-B\n1278.Syn-Bass 16\n1279.Syn-Bass 17\n1280.Syn-Bass 18\n1281.Syn-Bass 19\n1282.Syn-Bass20-L\n1283.Syn-Bass21-L\n1284.Syn-Bass 22\n1285.Syn-Bass 23\n1286.Syn-Bass 24\n1287.Syn-Bass25-L\n1288.Syn-Bass 26\n1289.Syn-Bass 27\n1290.Syn-Bass 28\n1291.Syn-Bass 29\n1292.Syn-Bass30-L\n1293.Syn-Bass 31\n1294.Syn-Bass 32\n1295.Syn-Bass 33\n1296.Syn-Bass 34\n1297.Syn-Bass35-L\n1298.Syn-Bass 36\n1299.Syn-Bass 37\
1300.Syn-Bass 38\n1301.Syn-Bass 39\n1302.Violin 1-L\n1303.Violin 2-L\n1304.Violin 3\n1305.Violin 4\n1306.Violin 5\n1307.Violin 6\n1308.Violin 7\n1309.Viola 1-L\n1310.Viola 2-L\n1311.Viola 3-L\n1312.Viola 4-L\n1313.Viola 5\n1314.Cello 1-B\n1315.Cello 2-L\n1316.Cello 3-L\n1317.Cello 4-L\n1318.Cello 5-L\n1319.Cello 6-L\n1320.Contrabs 1-B\n1321.Contrabs 2-B\n1322.Contrabs 3-L\n1323.Contrabs 4-L\n1324.Pizz.Str 1\n1325.Pizz.Str 2\n1326.Pizz.Str 3\n1327.Pizz.Str 4\n1328.Harp 1-L\n1329.Harp 2\n1330.Harp 3\n1331.Harp 4\n1332.Timpani 1-L\n1333.Timpani 2\n1334.Strings 1-L\n1335.Strings 2\n1336.Strings 3\n1337.Strings 4\n1338.Strings 5\n1339.Strings 6\n1340.Strings 7\n1341.Strings 8\n1342.Strings 9\n1343.Strings 10\n1344.Strings 11\n1345.Strings 12\n1346.SynthStr 1-B\n1347.SynthStr 2-L\n1348.SynthStr 3-L\n1349.SynthStr 4\n1350.SynthStr 5\n1351.SynthStr 6\n1352.SynthStr 7\n1353.SynthStr 8\n1354.SynthStr 9\n1355.SynthStr10-B\n1356.SynthStr11-B\n1357.SynthStr12-L\n1358.SynthStr13-L\n1359.SynthStr14-L\n1360.SynthStr 15\n1361.SynthStr 16\n1362.Choir 1-L\n1363.Choir 2-L\n1364.Choir 3-L\n1365.Choir 4\n1366.Choir 5\n1367.Choir 6\n1368.VoiceDoo 1-L\n1369.VoiceDoo 2\n1370.VoiceDoo 3\n1371.VoiceDoo 4\n1372.VoiceDoo 5\n1373.SynthVoi 1-L\n1374.SynthVoi 2\n1375.SynthVoi 3\n1376.Orch.Hit 1\n1377.Orch.Hit 2\n1378.Trumpet 1-L\n1379.Trumpet 2-L\n1380.Trumpet 3-L\n1381.Trumpet 4-L\n1382.Trumpet 5\n1383.Trumpet 6\n1384.Trumpet 7\n1385.Trumpet 8\n1386.Trombone 1-L\n1387.Trombone 2-L\n1388.Trombone 3\n1389.Trombone 4\n1390.Trombone 5-L\n1391.Trombone 6-L\n1392.Trombone 7-L\n1393.Trombone 8-L\n1394.Trombone 9\n1395.Tuba 1-B\n1396.Tuba 2-L\n1397.Tuba 3-L\n1398.Tuba 4-L\n1399.Mute Trp 1-B\
1400.Mute Trp 2-L\n1401.Mute Trp 3-L\n1402.Mute Trp 4-L\n1403.Mute Trp 5\n1404.Mute Trp 6\n1405.Fr.Horn 1-L\n1406.Fr.Horn 2-L\n1407.Fr.Horn 3-L\n1408.Fr.Horn 4\n1409.Fr.Horn 5\n1410.Fr.Horn 6\n1411.Brass 1-L\n1412.Brass 2\n1413.Brass 3\n1414.Brass 4\n1415.Brass 5\n1416.Sopr.Sax 1-L\n1417.Sopr.Sax 2-L\n1418.Sopr.Sax 3-L\n1419.Sopr.Sax 4-L\n1420.Sopr.Sax 5\n1421.Sopr.Sax 6\n1422.Sopr.Sax 7\n1423.Alto Sax 1-L\n1424.Alto Sax 2-L\n1425.Alto Sax 3-L\n1426.Alto Sax 4-L\n1427.Alto Sax 5\n1428.Alto Sax 6\n1429.Alto Sax 7\n1430.Alto Sax 8\n1431.Alto Sax 9\n1432.Alto Sax 10\n1433.TenorSax 1-B\n1434.TenorSax 2-B\n1435.TenorSax 3-L\n1436.TenorSax 4-L\n1437.TenorSax 5-L\n1438.TenorSax 6-L\n1439.TenorSax 7-L\n1440.TenorSax 8-L\n1441.TenorSax 9\n1442.TenorSax 10\n1443.TenorSax 11\n1444.Bari.Sax 1-B\n1445.Bari.Sax 2-L\n1446.Bari.Sax 3-L\n1447.Bari.Sax 4-L\n1448.Bari.Sax 5-L\n1449.Bari.Sax 6-L\n1450.Bari.Sax 7-L\n1451.Alto Sax11-L\n1452.Alto Sax 12\n1453.Alto Sax 13\n1454.Alto Sax 14\n1455.Alto Sax 15\n1456.Alto Sax 16\n1457.Alto Sax 17\n1458.Alto Sax 18\n1459.Alto Sax 19\n1460.Alto Sax 20\n1461.TenorSax12-L\n1462.TenorSax13-L\n1463.TenorSax14-L\n1464.TenorSax15-L\n1465.TenorSax 16\n1466.TenorSax 17\n1467.TenorSax 18\n1468.TenorSax 19\n1469.TenorSax 20\n1470.Oboe 1-L\n1471.Oboe 2\n1472.Oboe 3\n1473.Oboe 4\n1474.Eng.Horn 1-L\n1475.Bassoon 1-L\n1476.Clarinet 1\n1477.Clarinet 2\n1478.Clarinet 3\n1479.Piccolo 1\n1480.Piccolo 2\n1481.Piccolo 3\n1482.Flute 1\n1483.Flute 2\n1484.Flute 3\n1485.Flute 4\n1486.Flute 5\n1487.Recorder 1\n1488.PanFlute 1\n1489.PanFlute 2\n1490.BotlBlow 1-L\n1491.BotlBlow 2\n1492.BotlBlow 3\n1493.Shakhchi 1\n1494.Shakhchi 2\n1495.Ocarina 1\n1496.Sitar 1-L\n1497.Sitar 2\n1498.Sitar 3\n1499.Sitar 4-L\
1500.Sitar 5-L\n1501.Sitar 6\n1502.Sitar 7\n1503.Sitar 8\n1504.Tanpura 1-L\n1505.Tanpura 2-L\n1506.Tanpura 3\n1507.Harmnium 1-L\n1508.Harmnium 2-L\n1509.Harmnium 3-L\n1510.Harmnium 4-L\n1511.Harmnium 5\n1512.Santur 1\n1513.Santur 2\n1514.Er Hu 1\n1515.Er Hu 2\n1516.Er Hu 3\n1517.Er Hu 4\n1518.Er Hu 5\n1519.Er Hu 6\n1520.Di Zi 1\n1521.Di Zi 2\n1522.Di Zi 3\n1523.Pi Pa 1-L\n1524.Pi Pa 2-L\n1525.Pi Pa 3\n1526.Pi Pa 4-L\n1527.Pi Pa 5\n1528.Yang Qin 1\n1529.Kanun 1\n1530.Kanun 2\n1531.Kanun 3\n1532.Kanun 4\n1533.Kanun 5\n1534.Kanun 6\n1535.Oud 1\n1536.Oud 2\n1537.Oud 3\n1538.Oud 4\n1539.Oud 5\n1540.Saz 1\n1541.Saz 2\n1542.Saz 3\n1543.Bouzouki 1\n1544.Bouzouki 2\n1545.Bouzouki 3\n1546.Bouzouki 4\n1547.Banjo 1-L\n1548.Banjo 2-L\n1549.Banjo 3-L\n1550.Banjo 4\n1551.Ukulele 1\n1552.Shamisen 1-L\n1553.Shamisen 2-L\n1554.Shamisen 3\n1555.Shamisen 4\n1556.Koto 1\n1557.Koto 2\n1558.ThumbPno 1\n1559.Syn-Lead 1-B\n1560.Syn-Lead 2-B\n1561.Syn-Lead 3-B\n1562.Syn-Lead 4-B\n1563.Syn-Lead 5-B\n1564.Syn-Lead 6-L\n1565.Syn-Lead 7-L\n1566.Syn-Lead 8-L\n1567.Syn-Lead 9-L\n1568.Syn-Lead 10\n1569.Syn-Lead 11\n1570.Syn-Lead 12\n1571.Syn-Lead13-B\n1572.Syn-Lead14-B\n1573.Syn-Lead15-B\n1574.Syn-Lead16-L\n1575.Syn-Lead 17\n1576.Syn-Lead 18\n1577.Syn-Lead19-B\n1578.Syn-Lead20-B\n1579.Syn-Lead21-B\n1580.Syn-Lead22-B\n1581.Syn-Lead23-B\n1582.Syn-Lead24-L\n1583.Syn-Lead25-L\n1584.Syn-Lead26-L\n1585.Syn-Lead27-L\n1586.Syn-Lead 28\n1587.Syn-Lead 29\n1588.Syn-Lead 30\n1589.SynthPad 1-B\n1590.SynthPad 2-L\n1591.SynthPad 3\n1592.SynthPad 4\n1593.SynthPad 5\n1594.SynthPad 6\n1595.SynthPad 7\n1596.SynthPad 8\n1597.SynthPad 9\n1598.TinklBell 1\n1599.SteelDrm 1\
1600.SteelDrm 2\n1601.GtFrNoiz 1\n1602.Breath 1\n1603.Seashore 1\n1604.Bird 1\n1605.Telephon 1\n1606.Helcpter 1\n1607.Applause 1\n1608.Gunshot 1\n1609.Kick 1\n1610.Kick 2\n1611.Kick 3\n1612.Kick 4\n1613.Kick 5\n1614.Kick 6\n1615.Kick 7\n1616.Kick 8\n1617.Kick 9\n1618.Kick 10\n1619.Kick 11\n1620.Kick 12\n1621.Kick 13\n1622.Kick 14\n1623.Kick 15\n1624.Kick 16\n1625.Kick 17\n1626.Kick 18\n1627.Kick 19\n1628.Kick 20\n1629.Kick 21\n1630.Kick 22\n1631.Kick 23\n1632.Kick 24\n1633.Kick 25\n1634.Kick 26\n1635.Kick 27\n1636.Kick 28\n1637.Kick 29\n1638.Kick 30\n1639.Kick 31\n1640.Kick 32\n1641.Kick 33\n1642.Snare 1\n1643.Snare 2\n1644.Snare 3\n1645.Snare 4\n1646.Snare 5\n1647.Snare 6\n1648.Snare 7\n1649.Snare 8\n1650.Snare 9\n1651.Snare 10\n1652.Snare 11\n1653.Snare 12\n1654.Snare 13\n1655.Snare 14\n1656.Snare 15\n1657.Snare 16\n1658.Snare 17\n1659.Snare 18\n1660.Snare 19\n1661.Snare 20\n1662.Snare 21\n1663.Snare 22\n1664.Snare 23\n1665.Snare 24\n1666.Snare 25\n1667.Snare 26\n1668.Snare 27\n1669.Snare 28\n1670.Snare 29\n1671.Snare 30\n1672.Snare 31\n1673.Snare 32\n1674.Snare 33\n1675.Snare 34\n1676.Snare 35\n1677.Snare 36\n1678.Snare 37\n1679.Snare 38\n1680.Snare 39\n1681.Snare 40\n1682.Snare 41\n1683.Snare 42\n1684.Snare 43\n1685.Snare 44\n1686.Snare 45\n1687.Snare 46\n1688.Snare 47\n1689.Snare 48\n1690.Snare 49\n1691.Snare 50\n1692.Snare 51\n1693.Tom 1\n1694.Tom 2\n1695.Tom 3\n1696.Tom 4\n1697.Tom 5\n1698.Tom 6\n1699.Tom 7\
1700.Tom 8\n1701.Tom 9\n1702.Tom 10\n1703.Tom 11\n1704.Hi-Hat 1\n1705.Hi-Hat 2\n1706.Hi-Hat 3\n1707.Hi-Hat 4\n1708.Hi-Hat 5\n1709.Hi-Hat 6\n1710.Hi-Hat 7\n1711.Hi-Hat 8\n1712.Hi-Hat 9\n1713.Hi-Hat 10\n1714.Hi-Hat 11\n1715.Hi-Hat 12\n1716.Hi-Hat 13\n1717.Hi-Hat 14\n1718.Hi-Hat 15\n1719.Hi-Hat 16\n1720.Hi-Hat 17\n1721.Hi-Hat 18\n1722.Hi-Hat 19\n1723.Hi-Hat 20\n1724.Hi-Hat 21\n1725.Hi-Hat 22\n1726.Hi-Hat 23\n1727.Hi-Hat 24\n1728.Hi-Hat 25\n1729.Cymbal 1\n1730.Cymbal 2\n1731.Cymbal 3\n1732.Cymbal 4\n1733.Cymbal 5\n1734.Cymbal 6\n1735.Cymbal 7\n1736.Cymbal 8\n1737.Clap 1\n1738.Clap 2\n1739.Clap 3\n1740.Clap 4\n1741.Clap 5\n1742.Clap 6\n1743.Tambourine 1\n1744.Tambourine 2\n1745.Tambourine 3\n1746.Cowbell 1\n1747.Cowbell 2\n1748.Vibraslap 1\n1749.Bongo 1\n1750.Bongo 2\n1751.Conga 1\n1752.Conga 2\n1753.Conga 3\n1754.Conga 4\n1755.Timbale 1\n1756.Maracas 1\n1757.Whistle 1\n1758.Guiro 1\n1759.Guiro 2\n1760.Claves 1\n1761.WodBlock 1\n1762.Cuica 1\n1763.Cuica 2\n1764.Cabasa 1\n1765.Triangle 1\n1766.JingleBell 1\n1767.BellTree 1\n1768.Castanet 1\n1769.Applause 2\n1770.Applause 3\n1771.High Q 1\n1772.Slap 1\n1773.Scratch 1\n1774.Scratch 2\n1775.Scratch 3\n1776.Sticks 1\n1777.SqrClick 1\n1778.SynClick 1\n1779.SynClick 2\n1780.Metronome 1\n1781.Metronome 2\n1782.Wadaiko 1\n1783.Wadaiko 2\n1784.Ban Gu 1\n1785.HuYinLuo 1\n1786.Xiao Luo 1\n1787.Xiao Bo 1\n1788.Tang Gu 1\n1789.Dholak 1\n1790.Dholak 2\n1791.Dholak 3\n1792.Dholak 4\n1793.Dholak 5\n1794.Dholak 6\n1795.Tabla 1\n1796.Tabla 2\n1797.Tabla 3\n1798.Tabla 4\n1799.Tabla 5\
1800.Mridangm 1\n1801.Mridangm 2\n1802.Mridangm 3\n1803.Mridangm 4\n1804.Mridangm 5\n1805.Darbuka 1\n1806.Darbuka 2\n1807.Darbuka 3\n1808.Darbuka 4\n1809.Darbuka 5\n1810.Darbuka 6\n1811.Darbuka 7\n1812.Darbuka 8\n1813.Darbuka 9\n1814.Darbuka 10\n1815.Bendir 1\n1816.Bendir 2\n1817.Bendir 3\n1818.Bendir 4\n1819.Bendir 5\n1820.Bendir 6\n1821.Daf 1\n1822.Daf 2\n1823.Daf 3\n1824.Daf 4\n1825.Daf 5\n1826.Daf 6\n1827.Riq 1\n1828.Riq 2\n1829.Riq 3\n1830.Riq 4\n1831.Riq 5\n1832.Riq 6\n1833.Riq 7\n1834.Riq 8\n1835.Riq 9\n1836.Riq 10\n1837.Riq 11\n1838.Tombak 1\n1839.Tombak 2\n1840.Tombak 3\n1841.Zill 1\n1842.Zill 2\n1843.Zill 3\n1844.Zill 4\n1845.Davul 1\n1846.Davul 2\n1847.Davul 3\n1848.Davul 4\n1849.Davul 5\n1850.#Sin Wave\n1851.#Sin Wave-L\n1852.#Sin Wave-B\n1853.#Triangle\n1854.#Triangle-L\n1855.#Triangle-B\n1856.#Sawtooth\n1857.#Sawtooth-L\n1858.#Sawtooth-B\n1859.#ReverseSaw\n1860.#ReverseSawL\n1861.#ReverseSawB\n1862.#SquareWave\n1863.#SquareWav-L\n1864.#SquareWav-B\n1865.#MM Triangle\n1866.#MM TrianglL\n1867.#MM TrianglB\n1868.#MM Ramp\n1869.#MM Ramp-L\n1870.#MM Ramp-B\n1871.#MM Saw\n1872.#MM Saw-L\n1873.#MM Saw-B\n1874.#MM Square\n1875.#MM Square-L\n1876.#MM Square-B\n1877.#MM WidPulse\n1878.#MM WidPls-L\n1879.#MM WidPls-B\n1880.#MM NrwPulse\n1881.#MM NrwPls-L\n1882.#MM NrwPls-B\n1883.#MG Sin\n1884.#MG Sin-L\n1885.#MG Sin-B\n1886.#MG Triangle\n1887.#MG TrianglL\n1888.#MG TrianglB\n1889.#MG Saw\n1890.#MG Saw-L\n1891.#MG Saw-B\n1892.#MG Square\n1893.#MG Square-L\n1894.#MG Square-B\n1895.#MG Pulse1\n1896.#MG Pulse1-L\n1897.#MG Pulse1-B\n1898.#MG Pulse2\n1899.#MG Pulse2-L\
1900.#MG Pulse2-B\n1901.#AP1 Saw\n1902.#AP1 Saw-L\n1903.#AP1 Saw-B\n1904.#AP1 Square\n1905.#AP1 SquareL\n1906.#AP1 SquareB\n1907.#AP1 Triang\n1908.#AP1 TriangL\n1909.#AP1 TriangB\n1910.#AP1 Pulse1\n1911.#AP1 Pulse1L\n1912.#AP1 Pulse1B\n1913.#AP1 Pulse2\n1914.#AP1 Pulse2L\n1915.#AP1 Pulse2B\n1916.#AP1 Pulse3\n1917.#AP1 Pulse3L\n1918.#AP1 Pulse3B\n1919.#AP2 Saw\n1920.#AP2 Saw-L\n1921.#AP2 Saw-B\n1922.#AP2 Pulse1\n1923.#AP2 Pulse1L\n1924.#AP2 Pulse1B\n1925.#AP2 Pulse2\n1926.#AP2 Pulse2L\n1927.#AP2 Pulse2B\n1928.#AP2 SyncSaw\n1929.#AP2 SycSawL\n1930.#AP2 SycSawB\n1931.#AP2 SyncPls\n1932.#AP2 SycPlsL\n1933.#AP2 SycPlsB\n1934.#OB Saw\n1935.#OB Saw-L\n1936.#OB Saw-B\n1937.#OB Pulse1\n1938.#OB Pulse1-L\n1939.#OB Pulse1-B\n1940.#OB Pulse2\n1941.#OB Pulse2-L\n1942.#OB Pulse2-B\n1943.#OB SyncSaw\n1944.#OB SyncSawL\n1945.#OB SyncSawB\n1946.#OB SyncPls\n1947.#OB SyncPlsL\n1948.#OB SyncPlsB\n1949.#P5 Triangle\n1950.#P5 TrianglL\n1951.#P5 TrianglB\n1952.#P5 Saw\n1953.#P5 Saw-L\n1954.#P5 Saw-B\n1955.#P5 Pulse1\n1956.#P5 Pulse1-L\n1957.#P5 Pulse1-B\n1958.#P5 Pulse2\n1959.#P5 Pulse2-L\n1960.#P5 Pulse2-B\n1961.#P5 Pulse3\n1962.#P5 Pulse3-L\n1963.#P5 Pulse3-B\n1964.#P5 Pulse4\n1965.#P5 Pulse4-L\n1966.#P5 Pulse4-B\n1967.#ND Saw\n1968.#ND Saw-L\n1969.#ND Saw-B\n1970.#ND Pulse1\n1971.#ND Pulse1-L\n1972.#ND Pulse1-B\n1973.#ND Pulse2\n1974.#ND Pulse2-L\n1975.#ND Pulse2-B\n1976.#ND Pulse3\n1977.#ND Pulse3-L\n1978.#ND Pulse3-B\n1979.#ND FM1\n1980.#ND FM1-L\n1981.#ND FM1-B\n1982.#ND FM2\n1983.#ND FM2-L\n1984.#ND FM2-B\n1985.#ND FM3\n1986.#ND FM3-L\n1987.#ND FM3-B\n1988.#JP Saw\n1989.#JP Saw-L\n1990.#JP Saw-B\n1991.#JP Suare\n1992.#JP Suare-L\n1993.#JP Suare-B\n1994.#JP Pulse\n1995.#JP Pulse-L\n1996.#JP Pulse-B\n1997.#CZ Saw\n1998.#CZ Saw-L\n1999.#CZ Saw-B\
2000.#CZ Square\n2001.#CZ Square-L\n2002.#CZ Square-B\n2003.#CZ Pulse\n2004.#CZ Pulse-L\n2005.#CZ Pulse-B\n2006.#CZ DoublSin\n2007.#CZ DoblSinL\n2008.#CZ DoblSinB\n2009.#CZ SawPulse\n2010.#CZ SawPulsL\n2011.#CZ SawPulsB\n2012.#CZ Saw Reso\n2013.#CZ SawResoL\n2014.#CZ SawResoB\n2015.#CZ Tri Reso\n2016.#CZ TriResoL\n2017.#CZ TriResoB\n2018.#CZ Tra Reso\n2019.#CZ TraResoL\n2020.#CZ TraResoB\n2021.#CZ-Wave9\n2022.#CZ-Wave9-L\n2023.#CZ-Wave9-B\n2024.#CZ-Wave10\n2025.#CZ-Wave10-L\n2026.#CZ-Wave10-B\n2027.#CZ-Wave11\n2028.#CZ-Wave11-L\n2029.#CZ-Wave11-B\n2030.#CZ-Wave12\n2031.#CZ-Wave12-L\n2032.#CZ-Wave12-B\n2033.#CZ-Wave13\n2034.#CZ-Wave13-L\n2035.#CZ-Wave13-B\n2036.#CZ-Wave14\n2037.#CZ-Wave14-L\n2038.#CZ-Wave14-B\n2039.#CZ-Wave15\n2040.#CZ-Wave15-L\n2041.#CZ-Wave15-B\n2042.#CZ-Wave16\n2043.#CZ-Wave16-L\n2044.#CZ-Wave16-B\n2045.#CZ-Wave17\n2046.#CZ-Wave17-L\n2047.#CZ-Wave17-B\n2048.#CZ-Wave18\n2049.#CZ-Wave18-L\n2050.#CZ-Wave18-B\n2051.#CZ-Wave19\n2052.#CZ-Wave19-L\n2053.#CZ-Wave19-B\n2054.#CZ-Wave20\n2055.#CZ-Wave20-L\n2056.#CZ-Wave20-B\n2057.#CZ-Wave21\n2058.#CZ-Wave21-L\n2059.#CZ-Wave21-B\n2060.#CZ-Wave22\n2061.#CZ-Wave22-L\n2062.#CZ-Wave22-B\n2063.#CZ-Wave23\n2064.#CZ-Wave23-L\n2065.#CZ-Wave23-B\n2066.#CZ-Wave24\n2067.#CZ-Wave24-L\n2068.#CZ-Wave24-B\n2069.#CZ-Wave25\n2070.#CZ-Wave25-L\n2071.#CZ-Wave25-B\n2072.#CZ-Wave26\n2073.#CZ-Wave26-L\n2074.#CZ-Wave26-B\n2075.#CZ-Wave27\n2076.#CZ-Wave27-L\n2077.#CZ-Wave27-B\n2078.#CZ-Wave28\n2079.#CZ-Wave28-L\n2080.#CZ-Wave28-B\n2081.#CZ-Wave29\n2082.#CZ-Wave29-L\n2083.#CZ-Wave29-B\n2084.#CZ-Wave30\n2085.#CZ-Wave30-L\n2086.#CZ-Wave30-B\n2087.#CZ-Wave31\n2088.#CZ-Wave31-L\n2089.#CZ-Wave31-B\n2090.#CZ-Wave32\n2091.#CZ-Wave32-L\n2092.#CZ-Wave32-B\n2093.#CZ-Wave33\n2094.#CZ-Wave33-L\n2095.#CZ-Wave33-B\n2096.#VA Synth1\n2097.#VA Synth2\n2098.#VA Synth3\n2099.#VA Synth4\
2100.#VA Synth5\n2101.#VA Synth6\n2102.#VA Synth7\n2103.#VA Synth8\n2104.#VA Synth9\n2105.#VA Synth10\n2106.#VA Synth11\n2107.#VA Synth12\n2108.#VA Synth13\n2109.#VA Synth14\n2110.#VA Synth15\n2111.#VA Synth16\n2112.#VA Synth17\n2113.#VA Synth18\n2114.#VA Synth19\n2115.#TB Saw1-L\n2116.#TB Saw1-B\n2117.#TB Saw2-L\n2118.#TB Saw2-B\n2119.#TB Saw3-L\n2120.#TB Saw3-B\n2121.#TB Pulse1-L\n2122.#TB Pulse1-B\n2123.#TB Pulse2-L\n2124.#TB Pulse2-B\n2125.#TB Pulse3-L\n2126.#TB Pulse3-B\n2127.#TB Bass 1A\n2128.#TB Bass 1B\n2129.#TB Bass 1C\n2130.#TB Bass 2A\n2131.#TB Bass 2B\n2132.#TB Bass 2C\n2133.#MG Bass 1A\n2134.#MG Bass 1B\n2135.#MG Bass 1C\n2136.#MG Bass 2A\n2137.#MG Bass 2B\n2138.#MG Bass 2C\n2139.#SH Saw-L\n2140.#SH Saw-B\n2141.#SH Pulse1-L\n2142.#SH Pulse1-B\n2143.#SH Pulse2-L\n2144.#SH Pulse2-B\n2145.#SH Pulse3-L\n2146.#SH Pulse3-B\n2147.#SH SubOSC-L\n2148.#SH SubOSC-B\n2149.#SH BASS 1\n2150.#SH BASS 2\n2151.#SH BASS 3\n2152.#SH BASS 4\n2153.#SH BASS 5\n2154.#SH BASS 6\n2155.#SH BASS 7\n2156.#SH BASS 8\n2157.#SH BASS 9\
"
g_tssPCMwave.G1 = "0.St.GrPno-Lt1\n1.St.GrPno-Lt2\n2.St.GrPno-Rt1\n3.St.GrPno-Rt2\n4.St.BrPno-Lt1\n5.St.BrPno-Lt2\n6.St.BrPno-Rt1\n7.St.BrPno-Rt2\n8.Rock Pno-Lt\n9.Rock Pno-Rt\n10.MdernPno-Pn1\n11.MdernPno-Pn2\n12.MdernPno-EP\n13.Dance Piano\n14.StrPiano-Pn1\n15.StrPiano-Pn2\n16.StrPiano-Str\n17.PianoPad-Pn1\n18.PianoPad-Pn2\n19.PianoPad-Pad\n20.GM Piano 1-1\n21.GM Piano 1-2\n22.GM Piano 2-1\n23.GM Piano 2-2\n24.GM HnkyT-A1\n25.GM HnkyT-A2\n26.GM HnkyT-B1\n27.GM HnkyT-B2\n28.GM Piano 3\n29.GM Harpsi.\n30.Elec.Piano-1\n31.Elec.Piano-2\n32.FM E.Piano\n33.60âs EP-1\n34.60âs EP-2\n35.Dyno EP1-1\n36.Dyno EP1-2\n37.Dyno EP2-A1\n38.Dyno EP2-B1\n39.Dyno EP2-A2\n40.Dyno EP2-B2\n41.MellowEP-A\n42.MellowEP-B\n43.GM E.Pno1-1\n44.GM E.Pno1-2\n45.GM E.Piano 2\n46.VintageClavi\n47.Clavi\n48.Wah Clavi\n49.GM Clavi\n50.Vibraphone\n51.GM Vibraphon\n52.GM Celesta\n53.GM Glocken.\n54.GM MusicB-A\n55.GM MusicB-B\n56.GM Marimba\n57.GM Xylophone\n58.GM TublarBel\n59.GM Dulcimr-A\n60.GM Dulcimr-B\n61.DrawbarOrg 1\n62.DrawbarOrg 2\n63.Perc.Organ-A\n64.Perc.Organ-B\n65.Elec.Organ\n66.Jazz Organ-A\n67.Jazz Organ-B\n68.Rock Organ-A\n69.Rock Organ-B\n70.Dist.RockOrg\n71.Full Drawbar\n72.Rotary Organ\n73.GM Organ 1\n74.GM Organ 2-A\n75.GM Organ 2-B\n76.GM Organ 3-A\n77.GM Organ 3-B\n78.GM PipeOrg-A\n79.GM PipeOrg-B\n80.GM ReedOrgan\n81.GM Acordon-A\n82.GM Acordon-B\n83.GM Harmonica\n84.GM Bndneon-A\n85.GM Bndneon-B\n86.St.Str 1-Lt\n87.St.Str 1-Rt\n88.St.Str 2-Lt\n89.St.Str 2-Rt\n90.Strings\n91.StrEnsemble1\n92.StrEnsemble2\n93.GM Strings 1\n94.GM Strings 2\n95.Syn-Strings1\n96.Syn-Strings2\n97.70âs Syn-Str\n98.Fast Syn-Str\n99.Slow Syn-Str\
100.PhaserSynStr\n101.Cho.Syn-Str\n102.GM SynthStr1\n103.GM SynthStr2\n104.StrVoice-Cho\n105.StrVoice-Str\n106.Synth-Voice1\n107.Synth-Voice2\n108.Voi.Ens.-Voi\n109.Voi.Ens.-Str\n110.GM Syn-Voice\n111.GM ChoirAahs\n112.GM Voice Doo\n113.GM OrchHit-A\n114.GM OrchHit-B\n115.GM Violin\n116.GM Viola\n117.GM Cello\n118.GM Contrabas\n119.GM Trem.Str.\n120.GM Pizzicato\n121.GM Harp\n122.GM Timpani\n123.St.Brass-Lt\n124.St.Brass-Rt\n125.Brass-A\n126.Brass-B\n127.BrasSect-A\n128.BrasSect-B\n129.Syn-Brs1-A\n130.Syn-Brs1-B\n131.Synth-Brass2\n132.WarmSyBr-A\n133.WarmSyBr-B\n134.80sSyBrs-A\n135.80sSyBrs-B\n136.A.Syn-Brass\n137.TrnceBrs-A\n138.TrnceBrs-B\n139.GM Brass-A\n140.GM Brass-B\n141.GM SynBrs1-A\n142.GM SynBrs1-B\n143.GM SynBrs2-A\n144.GM SynBrs2-B\n145.GM Trumpet\n146.GM Trombone\n147.GM Tuba\n148.GM MtTrumpet\n149.GM Fr.Horn-A\n150.GM Fr.Horn-B\n151.GM Sop.Sax\n152.GM Alto Sax\n153.GM Tenor Sax\n154.GM Bar.Sax\n155.GM Oboe\n156.GM Eng.Horn\n157.GM Bassoon\n158.GM Clarinet\n159.GM Piccolo\n160.GM Flute\n161.GM Recorder\n162.GM Pan Flute\n163.GM BotlBlw-A\n164.GM BotlBlw-B\n165.GM Shkhchi-A\n166.GM Shkhchi-B\n167.GM Whistle\n168.GM Ocarina\n169.Nylon Gt-1\n170.Nylon Gt-2\n171.Steel Gt-1\n172.Steel Gt-2\n173.Clean Gt-1\n174.Clean Gt-2\n175.Wah E.Guitar\n176.Mute Gt-1\n177.Mute Gt-2\n178.Mute Ovd Gt\n179.Crunch E.Gt1\n180.Crunch E.Gt2\n181.DistortionGt\n182.More Dist.Gt\n183.GM Nylon Gt\n184.GM Steel Gt\n185.GM Jazz Gt\n186.GM CleanGt-1\n187.GM CleanGt-2\n188.GM Mute Gt-1\n189.GM Mute Gt-2\n190.GM Overdrv-A\n191.GM Overdrv-B\n192.GM Dist.Gt\n193.GM Gt Harm.\n194.AcousticBass\n195.FingrBs1-1\n196.FingrBs1-2\n197.FingerBass 2\n198.Picked Bass\n199.Fretless-1\
200.Fretless-2\n201.Synth-Bass 1\n202.SynBass2-1\n203.SynBass2-2\n204.SynBass3-1\n205.SynBass3-2\n206.SynBass3-3\n207.SynBass3-4\n208.SynBass4-1\n209.SynBass4-2\n210.Synth-Bass 5\n211.Synth-Bass 6\n212.Synth-Bass 7\n213.Synth-Bass 8\n214.Synth-Bass 9\n215.SawSyBs1-A\n216.SawSyBs1-B\n217.SawSyBs2-1\n218.SawSyBs2-2\n219.SawSyBs2-3\n220.SawSyBs2-4\n221.Sqr Syn-Bass\n222.DigiRockBass\n223.Trance Bass\n224.Bs&Kick-Bs\n225.Bs&Kick-Kck\n226.Vocoder Bass\n227.SoulSyn-Bass\n228.GM AcousBs-1\n229.GM AcousBs-2\n230.GM Fing.Bs-1\n231.GM Fing.Bs-2\n232.GM Pick Bs-1\n233.GM Pick Bs-2\n234.GM FrtlsBs-1\n235.GM FrtlsBs-2\n236.GM SlapBass1\n237.GM SlapBs2-1\n238.GM SlapBs2-2\n239.GM Syn-Bass1\n240.GM Syn-Bass2\n241.SqrLead1-A\n242.SqrLead1-B\n243.SqrLead2-A\n244.SqrLead2-B\n245.Square Lead3\n246.Square Lead4\n247.SqrPlsLd-A\n248.SqrPlsLd-B\n249.Sine Lead\n250.Saw Lead 1-A\n251.Saw Lead 1-B\n252.Saw Lead 2\n253.Saw Lead 3\n254.MlwSawLd-A\n255.MlwSawLd-B\n256.PlsSawLd-A\n257.PlsSawLd-B\n258.SS Lead-A\n259.SS Lead-B\n260.SlwSqrLd-A\n261.SlwSqrLd-B\n262.SlwSawLd-A\n263.SlwSawLd-B\n264.WahSawLd-A\n265.WahSawLd-B\n266.Seq.Square-A\n267.Seq.Square-B\n268.Seq.Pulse-A\n269.Seq.Pulse-B\n270.Seq.Sine-A\n271.Seq.Saw1-A\n272.Seq.Saw1-B\n273.Seq.Saw2-A\n274.Seq.Saw2-B\n275.TranceLd-A\n276.TranceLd-B\n277.VA SynthLead\n278.VA Synth 1\n279.VA Synth 2\n280.VA Synth 3\n281.VA Synth 4\n282.VA Synth 5\n283.VA Synth 6\n284.VA Synth 7\n285.VA Synth 8\n286.VA Syn-SqBs1\n287.VA Syn-SqBs2\n288.VA Syn-SqBs3\n289.VA Syn-SqBs4\n290.VA Syn-SqBs5\n291.VA Syn-SqBs6\n292.VA Syn-Hit 1\n293.VA Syn-Hit 2\n294.Vent Lead-A\n295.Vent Lead-B\n296.Vent Synth-A\n297.Vent Synth-B\n298.Seq.Lead-A\n299.Seq.Lead-B\
300.Drop Lead-A\n301.Drop Lead-B\n302.EP Lead-A\n303.EP Lead-B\n304.Voice Lead-A\n305.Voice Lead-B\n306.Vox Lead-A\n307.Vox Lead-B\n308.PluckLd1-A\n309.PluckLd1-B\n310.PluckLd2-A\n311.PluckLd2-B\n312.Gt SynLd-A\n313.Gt SynLd-B\n314.DblVoiLd-A\n315.DblVoiLd-B\n316.VoiChoLd-A\n317.VoiChoLd-B\n318.SynVoiLd-A\n319.SynVoiLd-B\n320.FifthSaw-A\n321.FifthSaw-B\n322.FifthSqr-A\n323.FifthSqr-B\n324.Fifth Seq.-A\n325.Fifth Seq.-B\n326.SynBs+Ld-A\n327.SynBs+Ld-B\n328.Fantasy 1-A\n329.Fantasy 1-B\n330.Fantasy 2-A\n331.Fantasy 2-B\n332.New Age-A\n333.New Age-B\n334.NewAgePd-A\n335.NewAgePd-B\n336.Warm Vox-A\n337.Warm Vox-B\n338.Thick Pad-A\n339.Thick Pad-B\n340.Warm Pad\n341.Sine Pad-A\n342.Sine Pad-B\n343.Soft Pad-A\n344.Soft Pad-B\n345.Horn Pad-A\n346.Horn Pad-B\n347.PolySyn1-A\n348.PolySyn1-B\n349.PolySyn2-A\n350.PolySyn2-B\n351.PolyPad1-A\n352.PolyPad1-B\n353.PolyPad2-A\n354.PolyPad2-B\n355.Poly Saw-A\n356.Poly Saw-B\n357.Heaven-A\n358.Heaven-B\n359.SpcStrPd-A\n360.SpcStrPd-B\n361.ChiffCho-A\n362.ChiffCho-B\n363.Star Voice-A\n364.Star Voice-B\n365.Glass Pad-A\n366.Glass Pad-B\n367.Bottle Pad-A\n368.Bottle Pad-B\n369.Halo Pad-A\n370.Halo Pad-B\n371.Chorus Pad-A\n372.Chorus Pad-B\n373.SweepCho-A\n374.SweepCho-B\n375.Rain Drop-A\n376.Rain Drop-B\n377.SoundTrk-A\n378.SoundTrk-B\n379.XmasBell-A\n380.XmasBell-B\n381.Vibes Bell-A\n382.Vibes Bell-B\n383.BrtBelPd-A\n384.BrtBelPd-B\n385.Britnes1-A\n386.Britnes1-B\n387.Britnes2-A\n388.Britnes2-B\n389.Echo Drop-A\n390.Echo Drop-B\n391.Poly Drop-A\n392.Poly Drop-B\n393.GM SqrLead-1\n394.GM SqrLead-2\n395.GM SawLead-1\n396.GM SawLead-2\n397.GM Caliope-1\n398.GM Caliope-2\n399.GM ChiffLd-1\
400.GM ChiffLd-2\n401.GM Charang-1\n402.GM Charang-2\n403.GM VoiceLd-1\n404.GM VoiceLd-2\n405.GM FifthLd-1\n406.GM FifthLd-2\n407.GM Bs+Lead-1\n408.GM Bs+Lead-2\n409.GM Fantasy-1\n410.GM Fantasy-2\n411.GM Warm Pad\n412.GM PolySyn-1\n413.GM PolySyn-2\n414.GM SpacCho-1\n415.GM SpacCho-2\n416.GM BowGlas-1\n417.GM BowGlas-2\n418.GM MetalPd-1\n419.GM MetalPd-2\n420.GM HaloPad-1\n421.GM HaloPad-2\n422.GM SweepPd-1\n423.GM SweepPd-2\n424.GM RainDrp-1\n425.GM RainDrp-2\n426.GM SoundTr-1\n427.GM SoundTr-2\n428.GM Crystal-1\n429.GM Crystal-2\n430.GM Atmosph-1\n431.GM Atmosph-2\n432.GM Britnes-1\n433.GM Britnes-2\n434.GM Goblins-1\n435.GM Goblins-2\n436.GM Echoes-1\n437.GM Echoes-2\n438.GM SF-1\n439.GM SF-2\n440.GM Sitar\n441.Tanpura\n442.GM Banjo\n443.GM Shamisen\n444.GM Koto\n445.GM Thumb Pno\n446.GM Bagpipe-1\n447.GM Bagpipe-2\n448.GM Fiddle\n449.GM Shanai\n450.GM TinkleBel\n451.GM Agogo\n452.GM SteelDr-1\n453.GM SteelDr-2\n454.GM WoodBlock\n455.GM Taiko\n456.GM Melo.Tom\n457.GM SynthDrum\n458.GM RevCymbal\n459.GM GtFrNoise\n460.GM BrthNoise\n461.GM Seashor-1\n462.GM Seashor-2\n463.GM Bird-1\n464.GM Bird-2\n465.GM Telephone\n466.GM Helicoptr\n467.GM Aplause-1\n468.GM Aplause-2\n469.GM Gunshot\n470.Sy_Sin Wave\n471.Sy_Triangle\n472.Sy_Sawtooth\n473.Sy_ReversSaw\n474.Sy_Square\n475.Sy_MM Triang\n476.Sy_MM Ramp\n477.Sy_MM Saw\n478.Sy_MM Square\n479.Sy_MM WidPls\n480.Sy_MM NrwPls\n481.Sy_MG Sin\n482.Sy_MG Triang\n483.Sy_MG Saw\n484.Sy_MG Square\n485.Sy_MG Pulse1\n486.Sy_MG Pulse2\n487.Sy_AP1 Saw\n488.Sy_AP1 Squar\n489.Sy_AP1 Tri\n490.Sy_AP1 Puls1\n491.Sy_AP1 Puls2\n492.Sy_AP1 Puls3\n493.Sy_AP2 Saw\n494.Sy_AP2 Puls1\n495.Sy_AP2 Puls2\n496.Sy_AP2SycSaw\n497.Sy_AP2SycPls\n498.Sy_OB Saw\n499.Sy_OB Pulse1\
500.Sy_OB Pulse2\n501.Sy_OBSyncSaw\n502.Sy_OBSyncPls\n503.Sy_P5 Triang\n504.Sy_P5 Saw\n505.Sy_P5 Pulse1\n506.Sy_P5 Pulse2\n507.Sy_P5 Pulse3\n508.Sy_P5 Pulse4\n509.Sy_ND Saw\n510.Sy_ND Pulse1\n511.Sy_ND Pulse2\n512.Sy_ND Pulse3\n513.Sy_ND FM\n514.Sy_JP Saw\n515.Sy_JP Square\n516.Sy_JP Pulse\n517.Sy_CZ Saw\n518.Sy_CZ Square\n519.Sy_CZ Pulse\n520.Sy_CZ DblSin\n521.Sy_CZ SawPls\n522.Sy_CZ SawRes\n523.Sy_CZ TriRes\n524.Sy_CZ TraRes\n525.Sy_CZ Wave9\n526.Sy_CZ Wave10\n527.Sy_CZ Wave11\n528.Sy_CZ Wave12\n529.Sy_CZ Wave13\n530.Sy_CZ Wave14\n531.Sy_CZ Wave15\n532.Sy_CZ Wave16\n533.Sy_CZ Wave17\n534.Sy_CZ Wave18\n535.Sy_CZ Wave19\n536.Sy_CZ Wave20\n537.Sy_CZ Wave21\n538.Sy_CZ Wave22\n539.Sy_CZ Wave23\n540.Sy_CZ Wave24\n541.Sy_CZ Wave25\n542.Sy_CZ Wave26\n543.Sy_CZ Wave27\n544.Sy_CZ Wave28\n545.Sy_CZ Wave29\n546.Sy_CZ Wave30\n547.Sy_CZ Wave31\n548.Sy_CZ Wave32\n549.Sy_CZ Wave33\n550.Sy_VA Synth1\n551.Sy_VA Synth2\n552.Sy_VA Synth3\n553.Sy_VA Synth4\n554.Sy_VA Synth5\n555.Sy_VA Synth6\n556.Sy_VA Synth7\n557.Sy_VA Synth8\n558.Sy_VA Synth9\n559.Sy_VA Syn 10\n560.Sy_VA Syn 11\n561.Sy_VA Syn 12\n562.Sy_VA Syn 13\n563.Sy_VA Syn 14\n564.Sy_VA Syn 15\n565.Sy_VA Syn 16\n566.Sy_VA Syn 17\n567.Sy_VA Syn 18\n568.Sy_VA Syn 19\n569.Sy_TB Bass 1\n570.Sy_TB Bass 2\n571.Sy_MG Bass 1\n572.Sy_MG Bass 2\n573.Sy_TB Saw 1\n574.Sy_TB Saw 2\n575.Sy_TB Saw 3\n576.Sy_TB Pulse1\n577.Sy_TB Pulse2\n578.Sy_TB Pulse3\n579.Sy_SH Saw\n580.Sy_SH Pulse1\n581.Sy_SH Pulse2\n582.Sy_SH Pulse3\n583.Sy_SH SubOSC\n584.Sy_SH BASS 1\n585.Sy_SH BASS 2\n586.Sy_SH BASS 3\n587.Sy_SH BASS 4\n588.Sy_SH BASS 5\n589.Sy_SH BASS 6\n590.Sy_SH BASS 7\n591.Sy_SH BASS 8\n592.Sy_SH BASS 9\n593.Nz_WhiteNoiz\n594.Nz_PinkNoise\n595.Nz_FltrNoiz1\n596.Nz_FltrNoiz2\n597.Nz_FltrNoiz3\n598.Nz_FltrNoiz4\n599.Nz_FltrNoiz5\
600.Nz_FltrNoiz6\n601.Nz_FltrNoiz7\n602.Nz_FltrNoiz8\n603.Nz_Smpl&Hld1\n604.Nz_Smpl&Hld2\n605.Nz_Scratch1\n606.Nz_Scratch2\n607.Piano 1-B\n608.Piano 2-L\n609.Piano 3-L\n610.Piano 4-L\n611.Piano 5\n612.Piano 6\n613.Piano 7\n614.Piano 8\n615.Piano 9\n616.Piano 10\n617.Piano 11\n618.Piano 12\n619.Piano 13-B\n620.Piano 14-L\n621.Piano 15-L\n622.Piano 16-L\n623.Piano 17\n624.Piano 18\n625.Piano 19\n626.Piano 20\n627.Piano 21\n628.Piano 22\n629.Piano 23\n630.Piano 24\n631.Piano 25-B\n632.Piano 26-L\n633.Piano 27-L\n634.Piano 28-L\n635.Piano 29\n636.Piano 30\n637.Piano 31\n638.Piano 32\n639.Piano 33\n640.Piano 34\n641.Piano 35-B\n642.Piano 36-L\n643.Piano 37-L\n644.Piano 38-L\n645.Piano 39\n646.Piano 40\n647.Piano 41\n648.Piano 42\n649.Piano 43\n650.Piano 44\n651.E.Piano 1-B\n652.E.Piano 2-L\n653.E.Piano 3\n654.E.Piano 4\n655.E.Piano 5\n656.E.Piano 6-L\n657.E.Piano 7-L\n658.E.Piano 8\n659.E.Piano 9\n660.E.Piano 10\n661.E.Piano 11\n662.E.Piano 12\n663.E.Piano 13-B\n664.E.Piano 14-L\n665.E.Piano 15-L\n666.E.Piano 16\n667.E.Piano 17\n668.E.Piano 18\n669.E.Piano 19\n670.E.Piano 20\n671.E.Piano 21\n672.E.Piano 22\n673.E.Piano 23\n674.E.Piano 24\n675.E.Piano 25\n676.E.Piano 26\n677.E.Piano 27\n678.E.Piano 28-L\n679.E.Piano 29-L\n680.E.Piano 30\n681.E.Piano 31\n682.E.Piano 32\n683.E.Piano 33\n684.E.Piano 34\n685.E.Piano 35-L\n686.E.Piano 36-L\n687.E.Piano 37\n688.E.Piano 38\n689.E.Piano 39\n690.E.Piano 40\n691.E.Piano 41\n692.E.Piano 42\n693.E.Piano 43\n694.E.Piano 44-L\n695.E.Piano 45-L\n696.E.Piano 46\n697.E.Piano 47\n698.E.Piano 48\n699.E.Piano 49\
700.E.Piano 50\n701.E.Piano 51\n702.E.Piano 52-B\n703.E.Piano 53-L\n704.E.Piano 54-L\n705.E.Piano 55\n706.E.Piano 56\n707.E.Piano 57\n708.E.Piano 58\n709.Harpsi. 1-B\n710.Harpsi. 2-L\n711.Harpsi. 3-L\n712.Harpsi. 4\n713.Harpsi. 5\n714.Harpsi. 6\n715.Clavi 1-B\n716.Clavi 2-L\n717.Clavi 3\n718.Clavi 4\n719.Clavi 5\n720.Clavi 6-B\n721.Clavi 7-B\n722.Clavi 8-B\n723.Clavi 9-L\n724.Clavi 10-L\n725.Clavi 11-L\n726.Clavi 12\n727.Clavi 13\n728.Clavi 14\n729.Clavi 15\n730.Clavi 16\n731.Clavi 17\n732.Celesta 1\n733.Glocken. 1\n734.Glocken. 2\n735.Glocken. 3\n736.Vibes 1\n737.Vibes 2\n738.Vibes 3\n739.Vibes 4\n740.Marimba 1\n741.Marimba 2\n742.Marimba 3\n743.Marimba 4\n744.Xylophon 1\n745.Xylophon 2\n746.Xylophon 3\n747.Xylophon 4\n748.Tubulbel 1\n749.Tubulbel 2\n750.Organ 1-L\n751.Organ 2\n752.Organ 3\n753.Organ 4\n754.Organ 5-L\n755.Organ 6\n756.Organ 7-B\n757.Organ 8-L\n758.Organ 9\n759.Organ 10\n760.Organ 11-L\n761.Organ 12\n762.Organ 13\n763.Organ 14\n764.Organ 15\n765.Organ 16\n766.Organ 17\n767.Organ 18\n768.Organ 19\n769.Organ 20\n770.Organ 21\n771.Organ 22\n772.Organ 23\n773.Organ 24\n774.Organ 25\n775.Organ 26-L\n776.Organ 27-L\n777.Organ 28\n778.Organ 29\n779.Organ 30\n780.Organ 31\n781.Organ 32\n782.Organ 33\n783.Organ 34\n784.Organ 35\n785.Pipe Org 1-B\n786.Pipe Org 2-L\n787.Pipe Org 3\n788.Pipe Org 4\n789.Pipe Org 5\n790.Pipe Org 6\n791.Pipe Org 7\n792.Pipe Org 8-L\n793.Pipe Org 9\n794.Pipe Org 10\n795.Pipe Org 11\n796.Pipe Org 12\n797.Acordion 1-L\n798.Acordion 2-L\n799.Acordion 3\
800.Acordion 4\n801.Acordion 5\n802.Bandneon 1-L\n803.Bandneon 2\n804.Bandneon 3\n805.Harmnica 1-L\n806.Harmnica 2-L\n807.Harmnica 3\n808.Harmnica 4\n809.Nylon Gt 1-B\n810.Nylon Gt 2-L\n811.Nylon Gt 3-L\n812.Nylon Gt 4-L\n813.Nylon Gt 5\n814.Nylon Gt 6\n815.Steel Gt 1-B\n816.Steel Gt 2-L\n817.Steel Gt 3-L\n818.Steel Gt 4\n819.Steel Gt 5\n820.Steel Gt 6\n821.Steel Gt 7\n822.Steel Gt 8\n823.Steel Gt 9\n824.Steel Gt 10\n825.Jazz Gt 1-L\n826.Jazz Gt 2\n827.Jazz Gt 3\n828.Jazz Gt 4\n829.Jazz Gt 5\n830.Elec.Gt 1-L\n831.Elec.Gt 2-L\n832.Elec.Gt 3-L\n833.Elec.Gt 4\n834.Elec.Gt 5\n835.Elec.Gt 6\n836.Elec.Gt 7\n837.Elec.Gt 8\n838.Elec.Gt 9-B\n839.Elec.Gt 10-L\n840.Elec.Gt 11-L\n841.Elec.Gt 12-L\n842.Elec.Gt 13\n843.Elec.Gt 14\n844.Elec.Gt 15\n845.Elec.Gt 16\n846.Mute Gt 1-L\n847.Mute Gt 2\n848.Mute Gt 3\n849.Mute Gt 4\n850.Ovrdrive 1-L\n851.Ovrdrive 2-L\n852.Ovrdrive 3\n853.Ovrdrive 4\n854.Ovrdrive 5\n855.Ovrdrive 6\n856.Ovrdrive 7-B\n857.Ovrdrive 8-L\n858.Ovrdrive 9-L\n859.Ovrdrive 10\n860.Ovrdrive 11\n861.Ovrdrive 12\n862.Ovrdrive 13\n863.Ovrdrive 14\n864.Ovrdrive15-B\n865.Ovrdrive16-B\n866.Ovrdrive17-B\n867.Ovrdrive18-L\n868.Ovrdrive19-L\n869.Ovrdrive20-L\n870.Ovrdrive 21\n871.Ovrdrive 22\n872.Ovrdrive 23\n873.Ovrdrive 24\n874.Ovrdrive 25\n875.Ovrdrive 26\n876.Ovrdrive 27\n877.Ovrdrive 28\n878.Dist.Gt 1-B\n879.Dist.Gt 2-L\n880.Dist.Gt 3-L\n881.Dist.Gt 4-L\n882.Dist.Gt 5\n883.Dist.Gt 6\n884.Dist.Gt 7\n885.Dist.Gt 8-B\n886.Dist.Gt 9-L\n887.Dist.Gt 10-L\n888.Dist.Gt 11-L\n889.Dist.Gt 12-L\n890.Dist.Gt 13\n891.Dist.Gt 14\n892.Dist.Gt 15\n893.GtHrmncs 1-L\n894.GtHrmncs 2\n895.Acous.Bs 1-B\n896.Acous.Bs 2-B\n897.Acous.Bs 3-B\n898.Acous.Bs 4-B\n899.Acous.Bs 5-B\
900.Acous.Bs 6-L\n901.Acous.Bs 7-L\n902.Acous.Bs 8\n903.FingerBs 1-B\n904.FingerBs 2-B\n905.FingerBs 3-B\n906.FingerBs 4-B\n907.FingerBs 5-B\n908.FingerBs 6-L\n909.FingerBs 7\n910.FingerBs 8-B\n911.FingerBs 9-B\n912.FingerBs10-B\n913.FingerBs11-B\n914.FingerBs12-B\n915.FingerBs13-L\n916.FingerBs 14\n917.FingerBs15-B\n918.FingerBs16-B\n919.FingerBs17-L\n920.FingerBs 18\n921.FingerBs 19\n922.PickBass 1-B\n923.PickBass 2-L\n924.PickBass 3-L\n925.PickBass 4\n926.PickBass 5\n927.Fretless 1-L\n928.Fretless 2\n929.Fretless 3\n930.SlapBass 1-B\n931.SlapBass 2-B\n932.SlapBass 3-B\n933.SlapBass 4\n934.SlapBass 5\n935.SlapBass 6-B\n936.SlapBass 7-L\n937.SlapBass 8\n938.Syn-Bass 1-B\n939.Syn-Bass 2-B\n940.Syn-Bass 3-B\n941.Syn-Bass 4-L\n942.Syn-Bass 5\n943.Syn-Bass 6\n944.Syn-Bass 7-L\n945.Syn-Bass 8\n946.Syn-Bass 9-L\n947.Syn-Bass10-L\n948.Syn-Bass 11\n949.Syn-Bass 12\n950.Syn-Bass 13\n951.Syn-Bass 14\n952.Syn-Bass15-B\n953.Syn-Bass 16\n954.Syn-Bass 17\n955.Syn-Bass 18\n956.Syn-Bass 19\n957.Syn-Bass20-L\n958.Syn-Bass21-L\n959.Syn-Bass 22\n960.Syn-Bass 23\n961.Syn-Bass 24\n962.Syn-Bass25-L\n963.Syn-Bass 26\n964.Syn-Bass 27\n965.Syn-Bass 28\n966.Syn-Bass 29\n967.Syn-Bass30-L\n968.Syn-Bass 31\n969.Syn-Bass 32\n970.Syn-Bass 33\n971.Syn-Bass 34\n972.Syn-Bass35-L\n973.Syn-Bass 36\n974.Syn-Bass 37\n975.Syn-Bass 38\n976.Syn-Bass 39\n977.Violin 1-L\n978.Violin 2-L\n979.Violin 3\n980.Violin 4\n981.Violin 5\n982.Violin 6\n983.Violin 7\n984.Viola 1-L\n985.Viola 2-L\n986.Viola 3-L\n987.Viola 4-L\n988.Viola 5\n989.Cello 1-B\n990.Cello 2-L\n991.Cello 3-L\n992.Cello 4-L\n993.Cello 5-L\n994.Cello 6-L\n995.Contrabs 1-B\n996.Contrabs 2-B\n997.Contrabs 3-L\n998.Contrabs 4-L\n999.Pizz.Str 1\
1000.Pizz.Str 2\n1001.Pizz.Str 3\n1002.Pizz.Str 4\n1003.Harp 1-L\n1004.Harp 2\n1005.Harp 3\n1006.Harp 4\n1007.Timpani 1-L\n1008.Timpani 2\n1009.Strings 1-L\n1010.Strings 2\n1011.Strings 3\n1012.Strings 4\n1013.Strings 5\n1014.Strings 6\n1015.Strings 7\n1016.Strings 8\n1017.Strings 9\n1018.Strings 10\n1019.Strings 11\n1020.Strings 12\n1021.SynthStr 1-B\n1022.SynthStr 2-L\n1023.SynthStr 3-L\n1024.SynthStr 4\n1025.SynthStr 5\n1026.SynthStr 6\n1027.SynthStr 7\n1028.SynthStr 8\n1029.SynthStr 9\n1030.SynthStr10-B\n1031.SynthStr11-B\n1032.SynthStr12-L\n1033.SynthStr13-L\n1034.SynthStr14-L\n1035.SynthStr 15\n1036.SynthStr 16\n1037.Choir 1-L\n1038.Choir 2-L\n1039.Choir 3-L\n1040.Choir 4\n1041.Choir 5\n1042.Choir 6\n1043.VoiceDoo 1-L\n1044.VoiceDoo 2\n1045.VoiceDoo 3\n1046.VoiceDoo 4\n1047.VoiceDoo 5\n1048.SynthVoi 1-L\n1049.SynthVoi 2\n1050.SynthVoi 3\n1051.Orch.Hit 1\n1052.Orch.Hit 2\n1053.Trumpet 1-L\n1054.Trumpet 2-L\n1055.Trumpet 3-L\n1056.Trumpet 4-L\n1057.Trumpet 5\n1058.Trumpet 6\n1059.Trumpet 7\n1060.Trumpet 8\n1061.Trombone 1-L\n1062.Trombone 2-L\n1063.Trombone 3\n1064.Trombone 4\n1065.Trombone 5-L\n1066.Trombone 6-L\n1067.Trombone 7-L\n1068.Trombone 8-L\n1069.Trombone 9\n1070.Tuba 1-B\n1071.Tuba 2-L\n1072.Tuba 3-L\n1073.Tuba 4-L\n1074.Mute Trp 1-B\n1075.Mute Trp 2-L\n1076.Mute Trp 3-L\n1077.Mute Trp 4-L\n1078.Mute Trp 5\n1079.Mute Trp 6\n1080.Fr.Horn 1-L\n1081.Fr.Horn 2-L\n1082.Fr.Horn 3-L\n1083.Fr.Horn 4\n1084.Fr.Horn 5\n1085.Fr.Horn 6\n1086.Brass 1-L\n1087.Brass 2\n1088.Brass 3\n1089.Brass 4\n1090.Brass 5\n1091.Sopr.Sax 1-L\n1092.Sopr.Sax 2-L\n1093.Sopr.Sax 3-L\n1094.Sopr.Sax 4-L\n1095.Sopr.Sax 5\n1096.Sopr.Sax 6\n1097.Sopr.Sax 7\n1098.Alto Sax 1-L\n1099.Alto Sax 2-L\
1100.Alto Sax 3-L\n1101.Alto Sax 4-L\n1102.Alto Sax 5\n1103.Alto Sax 6\n1104.Alto Sax 7\n1105.Alto Sax 8\n1106.Alto Sax 9\n1107.Alto Sax 10\n1108.TenorSax 1-B\n1109.TenorSax 2-B\n1110.TenorSax 3-L\n1111.TenorSax 4-L\n1112.TenorSax 5-L\n1113.TenorSax 6-L\n1114.TenorSax 7-L\n1115.TenorSax 8-L\n1116.TenorSax 9\n1117.TenorSax 10\n1118.TenorSax 11\n1119.Bari.Sax 1-B\n1120.Bari.Sax 2-L\n1121.Bari.Sax 3-L\n1122.Bari.Sax 4-L\n1123.Bari.Sax 5-L\n1124.Bari.Sax 6-L\n1125.Bari.Sax 7-L\n1126.Oboe 1-L\n1127.Oboe 2\n1128.Oboe 3\n1129.Oboe 4\n1130.Eng.Horn 1-L\n1131.Bassoon 1-L\n1132.Clarinet 1\n1133.Clarinet 2\n1134.Clarinet 3\n1135.Piccolo 1\n1136.Piccolo 2\n1137.Piccolo 3\n1138.Flute 1\n1139.Flute 2\n1140.Flute 3\n1141.Flute 4\n1142.Flute 5\n1143.Recorder 1\n1144.PanFlute 1\n1145.PanFlute 2\n1146.BotlBlow 1-L\n1147.BotlBlow 2\n1148.BotlBlow 3\n1149.Shakhchi 1\n1150.Shakhchi 2\n1151.Ocarina 1\n1152.Sitar 1-L\n1153.Sitar 2\n1154.Sitar 3\n1155.Sitar 4-L\n1156.Sitar 5-L\n1157.Sitar 6\n1158.Sitar 7\n1159.Sitar 8\n1160.Tanpura 1-L\n1161.Tanpura 2-L\n1162.Tanpura 3\n1163.Harmnium 1-L\n1164.Harmnium 2-L\n1165.Harmnium 3-L\n1166.Harmnium 4-L\n1167.Harmnium 5\n1168.Banjo 1-L\n1169.Banjo 2-L\n1170.Banjo 3-L\n1171.Banjo 4\n1172.Ukulele 1\n1173.Shamisen 1-L\n1174.Shamisen 2-L\n1175.Shamisen 3\n1176.Shamisen 4\n1177.Koto 1\n1178.Koto 2\n1179.ThumbPno 1\n1180.Syn-Lead 1-B\n1181.Syn-Lead 2-B\n1182.Syn-Lead 3-B\n1183.Syn-Lead 4-B\n1184.Syn-Lead 5-B\n1185.Syn-Lead 6-L\n1186.Syn-Lead 7-L\n1187.Syn-Lead 8-L\n1188.Syn-Lead 9-L\n1189.Syn-Lead 10\n1190.Syn-Lead 11\n1191.Syn-Lead 12\n1192.Syn-Lead13-B\n1193.Syn-Lead14-B\n1194.Syn-Lead15-B\n1195.Syn-Lead16-L\n1196.Syn-Lead 17\n1197.Syn-Lead 18\n1198.Syn-Lead19-B\n1199.Syn-Lead20-B\
1200.Syn-Lead21-B\n1201.Syn-Lead22-B\n1202.Syn-Lead23-B\n1203.Syn-Lead24-L\n1204.Syn-Lead25-L\n1205.Syn-Lead26-L\n1206.Syn-Lead27-L\n1207.Syn-Lead 28\n1208.Syn-Lead 29\n1209.Syn-Lead 30\n1210.SynthPad 1-B\n1211.SynthPad 2-L\n1212.SynthPad 3\n1213.SynthPad 4\n1214.SynthPad 5\n1215.SynthPad 6\n1216.SynthPad 7\n1217.TinklBell 1\n1218.SteelDrm 1\n1219.SteelDrm 2\n1220.GtFrNoiz 1\n1221.Breath 1\n1222.Seashore 1\n1223.Bird 1\n1224.Telephon 1\n1225.Helcpter 1\n1226.Applause 1\n1227.Gunshot 1\n1228.Kick 1\n1229.Kick 2\n1230.Kick 3\n1231.Kick 4\n1232.Kick 5\n1233.Kick 6\n1234.Kick 7\n1235.Kick 8\n1236.Kick 9\n1237.Kick 10\n1238.Kick 11\n1239.Kick 12\n1240.Kick 13\n1241.Kick 14\n1242.Kick 15\n1243.Kick 16\n1244.Kick 17\n1245.Kick 18\n1246.Kick 19\n1247.Kick 20\n1248.Kick 21\n1249.Kick 22\n1250.Kick 23\n1251.Kick 24\n1252.Kick 25\n1253.Kick 26\n1254.Kick 27\n1255.Kick 28\n1256.Kick 29\n1257.Kick 30\n1258.Kick 31\n1259.Kick 32\n1260.Kick 33\n1261.Kick 34\n1262.Kick 35\n1263.Kick 36\n1264.Kick 37\n1265.Kick 38\n1266.Kick 39\n1267.Kick 40\n1268.Kick 41\n1269.Kick 42\n1270.Kick 43\n1271.Kick 44\n1272.Kick 45\n1273.Kick 46\n1274.Kick 47\n1275.Kick 48\n1276.Kick 49\n1277.Kick 50\n1278.Kick 51\n1279.Kick 52\n1280.Kick 53\n1281.Kick 54\n1282.Kick 55\n1283.Kick 56\n1284.Kick 57\n1285.Kick 58\n1286.Kick 59\n1287.Kick 60\n1288.Kick 61\n1289.Kick 62\n1290.Kick 63\n1291.Kick 64\n1292.Kick 65\n1293.Kick 66\n1294.Kick 67\n1295.Kick 68\n1296.Kick 69\n1297.Kick 70\n1298.Kick 71\n1299.Kick 72\
1300.Kick 73\n1301.Kick 74\n1302.Kick 75\n1303.Kick 76\n1304.Kick 77\n1305.Kick 78\n1306.Kick 79\n1307.Kick 80\n1308.Kick 81\n1309.Kick 82\n1310.Kick 83\n1311.Kick 84\n1312.Kick 85\n1313.Kick 86\n1314.Kick 87\n1315.Kick 88\n1316.Kick 89\n1317.Kick 90\n1318.Kick 91\n1319.Kick 92\n1320.Kick 93\n1321.Kick 94\n1322.Kick 95\n1323.Kick 96\n1324.Kick 97\n1325.Kick 98\n1326.Kick 99\n1327.RZ-1 Kick\n1328.Snare 1\n1329.Snare 2\n1330.Snare 3\n1331.Snare 4\n1332.Snare 5\n1333.Snare 6\n1334.Snare 7\n1335.Snare 8\n1336.Snare 9\n1337.Snare 10\n1338.Snare 11\n1339.Snare 12\n1340.Snare 13\n1341.Snare 14\n1342.Snare 15\n1343.Snare 16\n1344.Snare 17\n1345.Snare 18\n1346.Snare 19\n1347.Snare 20\n1348.Snare 21\n1349.Snare 22\n1350.Snare 23\n1351.Snare 24\n1352.Snare 25\n1353.Snare 26\n1354.Snare 27\n1355.Snare 28\n1356.Snare 29\n1357.Snare 30\n1358.Snare 31\n1359.Snare 32\n1360.Snare 33\n1361.Snare 34\n1362.Snare 35\n1363.Snare 36\n1364.Snare 37\n1365.Snare 38\n1366.Snare 39\n1367.Snare 40\n1368.Snare 41\n1369.Snare 42\n1370.Snare 43\n1371.Snare 44\n1372.Snare 45\n1373.Snare 46\n1374.Snare 47\n1375.Snare 48\n1376.Snare 49\n1377.Snare 50\n1378.Snare 51\n1379.Snare 52\n1380.Snare 53\n1381.Snare 54\n1382.Snare 55\n1383.Snare 56\n1384.Snare 57\n1385.Snare 58\n1386.Snare 59\n1387.Snare 60\n1388.Snare 61\n1389.Snare 62\n1390.Snare 63\n1391.Snare 64\n1392.Snare 65\n1393.Snare 66\n1394.Snare 67\n1395.Snare 68\n1396.Snare 69\n1397.Snare 70\n1398.Snare 71\n1399.Snare 72\
1400.Snare 73\n1401.Snare 74\n1402.Snare 75\n1403.Snare 76\n1404.Snare 77\n1405.Snare 78\n1406.Snare 79\n1407.Snare 80\n1408.Snare 81\n1409.Snare 82\n1410.Snare 83\n1411.Snare 84\n1412.Snare 85\n1413.Snare 86\n1414.Snare 87\n1415.Snare 88\n1416.Snare 89\n1417.Snare 90\n1418.Snare 91\n1419.Snare 92\n1420.Snare 93\n1421.Snare 94\n1422.Snare 95\n1423.Snare 96\n1424.Snare 97\n1425.Snare 98\n1426.Snare 99\n1427.RZ-1 Snare\n1428.Tom 1\n1429.Tom 2\n1430.Tom 3\n1431.Tom 4\n1432.Tom 5\n1433.Tom 6\n1434.Tom 7\n1435.Tom 8\n1436.Tom 9\n1437.Tom 10\n1438.Tom 11\n1439.Tom 12\n1440.Tom 13\n1441.Tom 14\n1442.Tom 15\n1443.Tom 16\n1444.Tom 17\n1445.RZ-1 Tom 1\n1446.RZ-1 Tom 2\n1447.RZ-1 Tom 3\n1448.Hi-Hat 1\n1449.Hi-Hat 2\n1450.Hi-Hat 3\n1451.Hi-Hat 4\n1452.Hi-Hat 5\n1453.Hi-Hat 6\n1454.Hi-Hat 7\n1455.Hi-Hat 8\n1456.Hi-Hat 9\n1457.Hi-Hat 10\n1458.Hi-Hat 11\n1459.Hi-Hat 12\n1460.Hi-Hat 13\n1461.Hi-Hat 14\n1462.Hi-Hat 15\n1463.Hi-Hat 16\n1464.Hi-Hat 17\n1465.Hi-Hat 18\n1466.Hi-Hat 19\n1467.Hi-Hat 20\n1468.Hi-Hat 21\n1469.Hi-Hat 22\n1470.Hi-Hat 23\n1471.Hi-Hat 24\n1472.Hi-Hat 25\n1473.Hi-Hat 26\n1474.Hi-Hat 27\n1475.Hi-Hat 28\n1476.Hi-Hat 29\n1477.Hi-Hat 30\n1478.Hi-Hat 31\n1479.Hi-Hat 32\n1480.Hi-Hat 33\n1481.Hi-Hat 34\n1482.Hi-Hat 35\n1483.Hi-Hat 36\n1484.Hi-Hat 37\n1485.RZ1_Hi-Hat 1\n1486.RZ1_Hi-Hat 2\n1487.RZ1_Hi-Hat 3\n1488.Cymbal 1\n1489.Cymbal 2\n1490.Cymbal 3\n1491.Cymbal 4\n1492.Cymbal 5\n1493.Cymbal 6\n1494.Cymbal 7\n1495.Cymbal 8\n1496.Clap 1\n1497.Clap 2\n1498.Clap 3\n1499.Clap 4\
1500.Clap 5\n1501.Clap 6\n1502.Tambourine 1\n1503.Tambourine 2\n1504.Tambourine 3\n1505.Cowbell 1\n1506.Cowbell 2\n1507.Vibraslap 1\n1508.Bongo 1\n1509.Bongo 2\n1510.Conga 1\n1511.Conga 2\n1512.Conga 3\n1513.Conga 4\n1514.Timbale 1\n1515.Maracas 1\n1516.Whistle 1\n1517.Guiro 1\n1518.Guiro 2\n1519.Claves 1\n1520.WodBlock 1\n1521.Cuica 1\n1522.Cuica 2\n1523.Cabasa 1\n1524.Triangle 1\n1525.JingleBell 1\n1526.BellTree 1\n1527.Castanet 1\n1528.Applause 2\n1529.Applause 3\n1530.High Q 1\n1531.Slap 1\n1532.Scratch 1\n1533.Scratch 2\n1534.Scratch 3\n1535.Sticks 1\n1536.SqrClick 1\n1537.SynClick 1\n1538.SynClick 2\n1539.Metronome 1\n1540.Metronome 2\n1541.Wadaiko 1\n1542.Wadaiko 2\n1543.Ban Gu 1\n1544.HuYinLuo 1\n1545.Xiao Luo 1\n1546.Xiao Bo 1\n1547.Tang Gu 1\n1548.Dholak 1\n1549.Dholak 2\n1550.Dholak 3\n1551.Dholak 4\n1552.Dholak 5\n1553.Dholak 6\n1554.Tabla 1\n1555.Tabla 2\n1556.Tabla 3\n1557.Tabla 4\n1558.Tabla 5\n1559.Mridangm 1\n1560.Mridangm 2\n1561.Mridangm 3\n1562.Mridangm 4\n1563.Mridangm 5\n1564.Darbuka 1\n1565.Darbuka 2\n1566.Darbuka 3\n1567.Darbuka 4\n1568.Darbuka 5\n1569.Darbuka 6\n1570.Darbuka 7\n1571.Darbuka 8\n1572.Darbuka 9\n1573.Darbuka 10\n1574.Bendir 1\n1575.Bendir 2\n1576.Bendir 3\n1577.Bendir 4\n1578.Bendir 5\n1579.Bendir 6\n1580.Daf 1\n1581.Daf 2\n1582.Daf 3\n1583.Daf 4\n1584.Daf 5\n1585.Daf 6\n1586.Riq 1\n1587.Riq 2\n1588.Riq 3\n1589.Riq 4\n1590.Riq 5\n1591.Riq 6\n1592.Riq 7\n1593.Riq 8\n1594.Riq 9\n1595.Riq 10\n1596.Riq 11\n1597.Tombak 1\n1598.Tombak 2\n1599.Tombak 3\
1600.Zill 1\n1601.Zill 2\n1602.Zill 3\n1603.Zill 4\n1604.Davul 1\n1605.Davul 2\n1606.Davul 3\n1607.Davul 4\n1608.Davul 5\n1609.GroovePerc 1\n1610.GroovePerc 2\n1611.GroovePerc 3\n1612.GroovePerc 4\n1613.GroovePerc 5\n1614.GroovePerc 6\n1615.GroovePerc 7\n1616.GroovePerc 8\n1617.GroovePerc 9\n1618.GroovePerc10\n1619.GroovePerc11\n1620.GroovePerc12\n1621.GroovePerc13\n1622.GroovePerc14\n1623.GroovePerc15\n1624.GroovePerc16\n1625.GroovePerc17\n1626.GroovePerc18\n1627.GroovePerc19\n1628.GroovePerc20\n1629.GroovePerc21\n1630.GroovePerc22\n1631.GroovePerc23\n1632.GroovePerc24\n1633.GroovePerc25\n1634.GroovePerc26\n1635.GroovePerc27\n1636.GroovePerc28\n1637.GroovePerc29\n1638.GroovePerc30\n1639.GroovePerc31\n1640.GroovePerc32\n1641.GroovePerc33\n1642.GroovePerc34\n1643.GroovePerc35\n1644.GroovePerc36\n1645.GroovePerc37\n1646.GroovePerc38\n1647.GroovePerc39\n1648.GroovePerc40\n1649.GroovePerc41\n1650.GroovePerc42\n1651.GroovePerc43\n1652.GroovePerc44\n1653.GroovePerc45\n1654.GroovePerc46\n1655.GroovePerc47\n1656.GroovePerc48\n1657.GroovePerc49\n1658.GroovePerc50\n1659.Perc.Noise 1\n1660.Perc.Noise 2\n1661.Perc.Noise 3\n1662.Perc.Noise 4\n1663.Perc.Noise 5\n1664.Perc.Noise 6\n1665.Perc.Noise 7\n1666.Perc.Noise 8\n1667.Perc.Noise 9\n1668.Perc.Noise10\n1669.#White Noise\n1670.#Pink Noise\n1671.#FilterNoiz1\n1672.#FilterNoiz2\n1673.#FilterNoiz3\n1674.#FilterNoiz4\n1675.#FilterNoiz5\n1676.#FilterNoiz6\n1677.#FilterNoiz7\n1678.#FilterNoiz8\n1679.#Sampl&Hold1\n1680.#Sampl&Hold2\n1681.#ScratchNz1\n1682.#ScratchNz2\n1683.#Sin Wave\n1684.#Sin Wave-L\n1685.#Sin Wave-B\n1686.#Triangle\n1687.#Triangle-L\n1688.#Triangle-B\n1689.#Sawtooth\n1690.#Sawtooth-L\n1691.#Sawtooth-B\n1692.#ReverseSaw\n1693.#ReverseSawL\n1694.#ReverseSawB\n1695.#SquareWave\n1696.#SquareWav-L\n1697.#SquareWav-B\n1698.#MM Triangle\n1699.#MM TrianglL\
1700.#MM TrianglB\n1701.#MM Ramp\n1702.#MM Ramp-L\n1703.#MM Ramp-B\n1704.#MM Saw\n1705.#MM Saw-L\n1706.#MM Saw-B\n1707.#MM Square\n1708.#MM Square-L\n1709.#MM Square-B\n1710.#MM WidPulse\n1711.#MM WidPls-L\n1712.#MM WidPls-B\n1713.#MM NrwPulse\n1714.#MM NrwPls-L\n1715.#MM NrwPls-B\n1716.#MG Sin\n1717.#MG Sin-L\n1718.#MG Sin-B\n1719.#MG Triangle\n1720.#MG TrianglL\n1721.#MG TrianglB\n1722.#MG Saw\n1723.#MG Saw-L\n1724.#MG Saw-B\n1725.#MG Square\n1726.#MG Square-L\n1727.#MG Square-B\n1728.#MG Pulse1\n1729.#MG Pulse1-L\n1730.#MG Pulse1-B\n1731.#MG Pulse2\n1732.#MG Pulse2-L\n1733.#MG Pulse2-B\n1734.#AP1 Saw\n1735.#AP1 Saw-L\n1736.#AP1 Saw-B\n1737.#AP1 Square\n1738.#AP1 SquareL\n1739.#AP1 SquareB\n1740.#AP1 Triang\n1741.#AP1 TriangL\n1742.#AP1 TriangB\n1743.#AP1 Pulse1\n1744.#AP1 Pulse1L\n1745.#AP1 Pulse1B\n1746.#AP1 Pulse2\n1747.#AP1 Pulse2L\n1748.#AP1 Pulse2B\n1749.#AP1 Pulse3\n1750.#AP1 Pulse3L\n1751.#AP1 Pulse3B\n1752.#AP2 Saw\n1753.#AP2 Saw-L\n1754.#AP2 Saw-B\n1755.#AP2 Pulse1\n1756.#AP2 Pulse1L\n1757.#AP2 Pulse1B\n1758.#AP2 Pulse2\n1759.#AP2 Pulse2L\n1760.#AP2 Pulse2B\n1761.#AP2 SyncSaw\n1762.#AP2 SycSawL\n1763.#AP2 SycSawB\n1764.#AP2 SyncPls\n1765.#AP2 SycPlsL\n1766.#AP2 SycPlsB\n1767.#OB Saw\n1768.#OB Saw-L\n1769.#OB Saw-B\n1770.#OB Pulse1\n1771.#OB Pulse1-L\n1772.#OB Pulse1-B\n1773.#OB Pulse2\n1774.#OB Pulse2-L\n1775.#OB Pulse2-B\n1776.#OB SyncSaw\n1777.#OB SyncSawL\n1778.#OB SyncSawB\n1779.#OB SyncPls\n1780.#OB SyncPlsL\n1781.#OB SyncPlsB\n1782.#P5 Triangle\n1783.#P5 TrianglL\n1784.#P5 TrianglB\n1785.#P5 Saw\n1786.#P5 Saw-L\n1787.#P5 Saw-B\n1788.#P5 Pulse1\n1789.#P5 Pulse1-L\n1790.#P5 Pulse1-B\n1791.#P5 Pulse2\n1792.#P5 Pulse2-L\n1793.#P5 Pulse2-B\n1794.#P5 Pulse3\n1795.#P5 Pulse3-L\n1796.#P5 Pulse3-B\n1797.#P5 Pulse4\n1798.#P5 Pulse4-L\n1799.#P5 Pulse4-B\
1800.#ND Saw\n1801.#ND Saw-L\n1802.#ND Saw-B\n1803.#ND Pulse1\n1804.#ND Pulse1-L\n1805.#ND Pulse1-B\n1806.#ND Pulse2\n1807.#ND Pulse2-L\n1808.#ND Pulse2-B\n1809.#ND Pulse3\n1810.#ND Pulse3-L\n1811.#ND Pulse3-B\n1812.#ND FM1\n1813.#ND FM1-L\n1814.#ND FM1-B\n1815.#ND FM2\n1816.#ND FM2-L\n1817.#ND FM2-B\n1818.#ND FM3\n1819.#ND FM3-L\n1820.#ND FM3-B\n1821.#JP Saw\n1822.#JP Saw-L\n1823.#JP Saw-B\n1824.#JP Suare\n1825.#JP Suare-L\n1826.#JP Suare-B\n1827.#JP Pulse\n1828.#JP Pulse-L\n1829.#JP Pulse-B\n1830.#CZ Saw\n1831.#CZ Saw-L\n1832.#CZ Saw-B\n1833.#CZ Square\n1834.#CZ Square-L\n1835.#CZ Square-B\n1836.#CZ Pulse\n1837.#CZ Pulse-L\n1838.#CZ Pulse-B\n1839.#CZ DoublSin\n1840.#CZ DoblSinL\n1841.#CZ DoblSinB\n1842.#CZ SawPulse\n1843.#CZ SawPulsL\n1844.#CZ SawPulsB\n1845.#CZ Saw Reso\n1846.#CZ SawResoL\n1847.#CZ SawResoB\n1848.#CZ Tri Reso\n1849.#CZ TriResoL\n1850.#CZ TriResoB\n1851.#CZ Tra Reso\n1852.#CZ TraResoL\n1853.#CZ TraResoB\n1854.#CZ-Wave9\n1855.#CZ-Wave9-L\n1856.#CZ-Wave9-B\n1857.#CZ-Wave10\n1858.#CZ-Wave10-L\n1859.#CZ-Wave10-B\n1860.#CZ-Wave11\n1861.#CZ-Wave11-L\n1862.#CZ-Wave11-B\n1863.#CZ-Wave12\n1864.#CZ-Wave12-L\n1865.#CZ-Wave12-B\n1866.#CZ-Wave13\n1867.#CZ-Wave13-L\n1868.#CZ-Wave13-B\n1869.#CZ-Wave14\n1870.#CZ-Wave14-L\n1871.#CZ-Wave14-B\n1872.#CZ-Wave15\n1873.#CZ-Wave15-L\n1874.#CZ-Wave15-B\n1875.#CZ-Wave16\n1876.#CZ-Wave16-L\n1877.#CZ-Wave16-B\n1878.#CZ-Wave17\n1879.#CZ-Wave17-L\n1880.#CZ-Wave17-B\n1881.#CZ-Wave18\n1882.#CZ-Wave18-L\n1883.#CZ-Wave18-B\n1884.#CZ-Wave19\n1885.#CZ-Wave19-L\n1886.#CZ-Wave19-B\n1887.#CZ-Wave20\n1888.#CZ-Wave20-L\n1889.#CZ-Wave20-B\n1890.#CZ-Wave21\n1891.#CZ-Wave21-L\n1892.#CZ-Wave21-B\n1893.#CZ-Wave22\n1894.#CZ-Wave22-L\n1895.#CZ-Wave22-B\n1896.#CZ-Wave23\n1897.#CZ-Wave23-L\n1898.#CZ-Wave23-B\n1899.#CZ-Wave24\
1900.#CZ-Wave24-L\n1901.#CZ-Wave24-B\n1902.#CZ-Wave25\n1903.#CZ-Wave25-L\n1904.#CZ-Wave25-B\n1905.#CZ-Wave26\n1906.#CZ-Wave26-L\n1907.#CZ-Wave26-B\n1908.#CZ-Wave27\n1909.#CZ-Wave27-L\n1910.#CZ-Wave27-B\n1911.#CZ-Wave28\n1912.#CZ-Wave28-L\n1913.#CZ-Wave28-B\n1914.#CZ-Wave29\n1915.#CZ-Wave29-L\n1916.#CZ-Wave29-B\n1917.#CZ-Wave30\n1918.#CZ-Wave30-L\n1919.#CZ-Wave30-B\n1920.#CZ-Wave31\n1921.#CZ-Wave31-L\n1922.#CZ-Wave31-B\n1923.#CZ-Wave32\n1924.#CZ-Wave32-L\n1925.#CZ-Wave32-B\n1926.#CZ-Wave33\n1927.#CZ-Wave33-L\n1928.#CZ-Wave33-B\n1929.#VA Synth1\n1930.#VA Synth2\n1931.#VA Synth3\n1932.#VA Synth4\n1933.#VA Synth5\n1934.#VA Synth6\n1935.#VA Synth7\n1936.#VA Synth8\n1937.#VA Synth9\n1938.#VA Synth10\n1939.#VA Synth11\n1940.#VA Synth12\n1941.#VA Synth13\n1942.#VA Synth14\n1943.#VA Synth15\n1944.#VA Synth16\n1945.#VA Synth17\n1946.#VA Synth18\n1947.#VA Synth19\n1948.#TB Saw1-L\n1949.#TB Saw1-B\n1950.#TB Saw2-L\n1951.#TB Saw2-B\n1952.#TB Saw3-L\n1953.#TB Saw3-B\n1954.#TB Pulse1-L\n1955.#TB Pulse1-B\n1956.#TB Pulse2-L\n1957.#TB Pulse2-B\n1958.#TB Pulse3-L\n1959.#TB Pulse3-B\n1960.#TB Bass 1A\n1961.#TB Bass 1B\n1962.#TB Bass 1C\n1963.#TB Bass 2A\n1964.#TB Bass 2B\n1965.#TB Bass 2C\n1966.#MG Bass 1A\n1967.#MG Bass 1B\n1968.#MG Bass 1C\n1969.#MG Bass 2A\n1970.#MG Bass 2B\n1971.#MG Bass 2C\n1972.#SH Saw-L\n1973.#SH Saw-B\n1974.#SH Pulse1-L\n1975.#SH Pulse1-B\n1976.#SH Pulse2-L\n1977.#SH Pulse2-B\n1978.#SH Pulse3-L\n1979.#SH Pulse3-B\n1980.#SH SubOSC-L\n1981.#SH SubOSC-B\n1982.#SH BASS 1\n1983.#SH BASS 2\n1984.#SH BASS 3\n1985.#SH BASS 4\n1986.#SH BASS 5\n1987.#SH BASS 6\n1988.#SH BASS 7\n1989.#SH BASS 8\n1990.#SH BASS 9\
"


end

-- ================================================================
-- METHOD 013: initPresets  (5122 bytes)
-- ================================================================

function initPresets()

g_tssFactPreset	= {}
g_tssUserPreset	= {}

g_tssFactPreset.P1 = "P0-0.XW SoloSynth\nP0-1.XW SoloSyn 1\nP0-2.XW SoloSyn 2\nP0-3.XW SoloSyn 3\nP0-4.MM Raw Lead\nP0-5.XW SoloSyn 4\nP0-6.XW SoloSyn 5\nP0-7.XW SoloSyn 6\nP0-8.XW SoloSyn 7\nP0-9.MG Raw Lead\nP1-0.XW LeadSynth\nP1-1.XW LeadSyn 1\nP1-2.XW LeadSyn 2\nP1-3.XW LeadSyn 3\nP1-4.XW LeadSyn 4\nP1-5.XW LeadSyn 5\nP1-6.XW LeadSyn 6\nP1-7.CZ RawLead1\nP1-8.CZ RawLead2\nP1-9.CZ RawLead3\nP2-0.ChrisMayLead\nP2-1.Whistler\nP2-2.Theremin Oct\nP2-3.SnakeCharmin\nP2-4.Ody Raw Lead\nP2-5.Gaa-Gaa Saw\nP2-6.HiPasfitLead\nP2-7.Transistory\nP2-8.TrezBandPass\nP2-9.AP Raw Lead\nP3-0.HiTresMotion\nP3-1.Rezonant 5th\nP3-2.FuzzyHarmnix\nP3-3.SyncOsc Lead\nP3-4.OB Raw Lead\nP3-5.Harmoniclime\nP3-6.WishYouHorn\nP3-7.Asian Brass\nP3-8.Lucky Square\nP3-9.P5 Raw Lead\nP4-0.Triangle XMW\nP4-1.Woody Lead\nP4-2.Xfadly Saw\nP4-3.Flutron Lead\nP4-4.ND Raw Lead\nP4-5.FretlessLead\nP4-6.AndYou&Lead\nP4-7.SynthitarLed\nP4-8.Motion Vox\nP4-9.JP Raw Lead\nP5-0.ElectroChoir\nP5-1.BellSineLead\nP5-2.Octrian Lead\nP5-3.OctSawCzLead\nP5-4.Pinched 4ths\nP5-5.Open Fifths\nP5-6.OctPulseLead\nP5-7.Transistord\nP5-8.Poncom Synth\nP5-9.DelaySawBass\nP6-0.XW SynthBass\nP6-1.XW SynBass 1\nP6-2.XW SynBass 2\nP6-3.XW SynBass 3\nP6-4.XW SynBass 4\nP6-5.XW SynBass 5\nP6-6.XW SynBass 6\nP6-7.XW SynBass 7\nP6-8.Drama-Kehew\nP6-9.TransAmbient\nP7-0.RetroSynBass\nP7-1.Retro-Bass 1\nP7-2.Retro-Bass 2\nP7-3.Retro-Bass 3\nP7-4.Retro-Bass 4\nP7-5.Retro-Bass 5\nP7-6.Retro-Bass 6\nP7-7.Retro-Bass 7\nP7-8.Synth Hit 1\nP7-9.Synth Hit 2\nP8-0.VirtualWaves\nP8-1.WaveNation 1\nP8-2.WaveNation 2\nP8-3.WaveNation 3\nP8-4.WaveNation 4\nP8-5.WaveNation 5\nP8-6.WaveNation 6\nP8-7.ModWheel S&H\nP8-8.Sonar Ping\nP8-9.DribbleSpace\nP9-0.MG KnobSlidr\nP9-1.MM KnobSlidr\nP9-2.Od KnobSlidr\nP9-3.26 KnobSlidr\nP9-4.OB KnobSlidr\nP9-5.P5 KnobSlidr\nP9-6.JP KnobSlidr\nP9-7.ND KnobSlidr\nP9-8.CZ KnobSlidr\nP9-9.Basic&Mic IN"
g_tssUserPreset.P1 = "P0-0\nP0-1\nP0-2\nP0-3\nP0-4\nP0-5\nP0-6\nP0-7\nP0-8\nP0-9\nP1-0\nP1-1\nP1-2\nP1-3\nP1-4\nP1-5\nP1-6\nP1-7\nP1-8\nP1-9\nP2-0\nP2-1\nP2-2\nP2-3\nP2-4\nP2-5\nP2-6\nP2-7\nP2-8\nP2-9\nP3-0\nP3-1\nP3-2\nP3-3\nP3-4\nP3-5\nP3-6\nP3-7\nP3-8\nP3-9\nP4-0\nP4-1\nP4-2\nP4-3\nP4-4\nP4-5\nP4-6\nP4-7\nP4-8\nP4-9\nP5-0\nP5-1\nP5-2\nP5-3\nP5-4\nP5-5\nP5-6\nP5-7\nP5-8\nP5-9\nP6-0\nP6-1\nP6-2\nP6-3\nP6-4\nP6-5\nP6-6\nP6-7\nP6-8\nP6-9\nP7-0\nP7-1\nP7-2\nP7-3\nP7-4\nP7-5\nP7-6\nP7-7\nP7-8\nP7-9\nP8-0\nP8-1\nP8-2\nP8-3\nP8-4\nP8-5\nP8-6\nP8-7\nP8-8\nP8-9\nP9-0\nP9-1\nP9-2\nP9-3\nP9-4\nP9-5\nP9-6\nP9-7\nP9-8\nP9-9"
g_tssFactPreset.G1 = "P000.XW SoloSynth\nP001.XW SoloSyn 1\nP002.XW SoloSyn 2\nP003.XW SoloSyn 3\nP004.MM Raw Lead\nP005.XW SoloSyn 4\nP006.XW SoloSyn 5\nP007.XW SoloSyn 6\nP008.XW SoloSyn 7\nP009.MG Raw Lead\nP010.XW LeadSynth\nP011.XW LeadSyn 1\nP012.XW LeadSyn 2\nP013.XW LeadSyn 3\nP014.XW LeadSyn 4\nP015.XW LeadSyn 5\nP016.XW LeadSyn 6\nP017.CZ RawLead 1\nP018.CZ RawLead 2\nP019.CZ RawLead 3\nP020.G1 Solo Drum\nP021.G1 SoloKick1\nP022.G1 SoloKick2\nP023.G1 SoloKick3\nP024.G1 SoloKick4\nP025.G1 SoloKick5\nP026.G1 SoloKick6\nP027.G1 SoloKick7\nP028.G1 SoloKick8\nP029.RZ1 SoloKick\nP030.G1 SoloSnare\nP031.G1 Solo SD 1\nP032.G1 Solo SD 2\nP033.G1 Solo SD 3\nP034.G1 Solo SD 4\nP035.G1 Solo SD 5\nP036.G1 Solo SD 6\nP037.G1 Solo SD 7\nP038.G1 Solo SD 8\nP039.RZ1 Solo SD\nP040.G1Percussion\nP041.G1 SoloPerc1\nP042.G1 SoloPerc2\nP043.G1 SoloPerc3\nP044.G1 SoloPerc4\nP045.G1 SoloPerc5\nP046.G1 SoloPerc6\nP047.G1 SoloPerc7\nP048.G1 SoloPerc8\nP049.G1 SoloPerc9\nP050.XW SynthBass\nP051.XW SynBass 1\nP052.XW SynBass 2\nP053.XW SynBass 3\nP054.XW SynBass 4\nP055.XW SynBass 5\nP056.XW SynBass 6\nP057.XW SynBass 7\nP058.DelaySawBass\nP059.TransAmbient\nP060.RetroSynBass\nP061.Retro Bass 1\nP062.Retro Bass 2\nP063.Retro Bass 3\nP064.Retro Bass 4\nP065.Retro Bass 5\nP066.Retro Bass 6\nP067.Retro Bass 7\nP068.Synth Hit 1\nP069.Synth Hit 2\nP070.VirtualWaves\nP071.WaveNation 1\nP072.WaveNation 2\nP073.WaveNation 3\nP074.WaveNation 4\nP075.WaveNation 5\nP076.WaveNation 6\nP077.HiPasfitLead\nP078.HiTresMotion\nP079.Motion Vox\nP080.AP Raw Lead\nP081.OB Raw Lead\nP082.P5 Raw Lead\nP083.ND Raw Lead\nP084.Whistler\nP085.Theremin Oct\nP086.Gaa-Gaa Saw\nP087.Transistory\nP088.TrezBandPass\nP089.Poncom Synth\nP090.FuzzyHarmnix\nP091.Triangle XMW\nP092.BellSineLead\nP093.G1 NoiseEFX1\nP094.G1 NoiseEFX2\nP095.G1 NoiseEFX3\nP096.G1 NoiseEFX4\nP097.G1 NoiseEFX5\nP098.G1 NoiseEFX6\nP099.Basic&Mic IN"
g_tssUserPreset.G1 = "P000\nP001\nP002\nP003\nP004\nP005\nP006\nP007\nP008\nP009\nP010\nP011\nP012\nP013\nP014\nP015\nP016\nP017\nP018\nP019\nP020\nP021\nP022\nP023\nP024\nP025\nP026\nP027\nP028\nP029\nP030\nP031\nP032\nP033\nP034\nP035\nP036\nP037\nP038\nP039\nP040\nP041\nP042\nP043\nP044\nP045\nP046\nP047\nP048\nP049\nP050\nP051\nP052\nP053\nP054\nP055\nP056\nP057\nP058\nP059\nP060\nP061\nP062\nP063\nP064\nP065\nP066\nP067\nP068\nP069\nP070\nP071\nP072\nP073\nP074\nP075\nP076\nP077\nP078\nP079\nP080\nP081\nP082\nP083\nP084\nP085\nP086\nP087\nP088\nP089\nP090\nP091\nP092\nP093\nP094\nP095\nP096\nP097\nP098\nP099\n"

end

-- ================================================================
-- METHOD 014: globalFunctions  (12392 bytes)
-- ================================================================

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


-- ================================================================
-- METHOD 015: midiReceive  (8033 bytes)
-- ================================================================

--
-- midiReceive: handler for midi receive
--

local setModNameVal			= setModNameVal
local getModNameVal			= getModNameVal
local setModNameIntProperty	= setModNameIntProperty
local getModNameIntProperty	= getModNameIntProperty


--
-- receive and process messages for TSS
--
local function rxTSS(midi)
	local sysex	= {sx, mid, db, mval, vt, _f, osc}
	local v		= {}

	sysex.sx	= midi.msg:getData():getRange(6,18):toHexString(1)

	if g_tssModSXrx[sysex.sx] then 
		sysex.mid		= g_tssModSXrx[sysex.sx].id
		sysex.vt		= g_tssModSXrx[sysex.sx].vt 
		sysex.db		= midi.size-25												-- number of data bytes
		sysex._f		= g_xwModCalc["SX2v"][sysex.vt]

		-- calculate modulator values:
		for b=1,sysex.db do 	v[b] 	= midi.msg:getData():getByte(23+b) end						-- calculate modulator value
		if sysex.mid:match("tssOSCwf") then	v[4] = tonumber(sysex.mid:sub(-1,-1),10) or 1 end 		-- special case oscillator waveforms

		sysex.mval	= sysex._f(v[1],v[2],v[3],v[4])

		-- set modulator:
		setModNameVal(sysex.mid, sysex.mval)

		-- draw ADSRR graph:
		if sysex.mid:match('tss.*ENV') then
			local ENVCanvas
			local gvar	= 	sysex.mid:gsub('.*ENV',''):gsub("-.*","")		-- eg 'r1T'
			local gmod	=	sysex.mid:gsub(gvar,'canv')						-- eg 'tssOSCPENVcanv-1'
			g_CANVdata[gmod][gvar] = sysex.mval								-- assign slider value to global data
			ENVCanvas = panel:getComponent(gmod) ; if ENVCanvas then ENVCanvas:repaint() end
		end
	end
end

--
-- receive and process messages for DSP
--
local function rxDSP(midi)
	local sysex	= {sxr, sx, tid, mid, db, mval, vt, _f}
	local v		= {}
	local mcalc	= g_xwModCalc["SX2v"]

	sysex.sxr	= midi.msg:getData():getRange(6,18):toHexString(1)
	sysex.tid 	= getModNameIntProperty("tssDSPTab", "uiTabsCurrentTab") + 0x80

	for i,t in ipairs({0x80, sysex.tid}) do 
		sysex.sx	= string.format("%s-%.2x",sysex.sxr,t)

		if g_dspModSXrx[sysex.sx] then
			sysex.mid		= g_dspModSXrx[sysex.sx].id
			sysex.vt		= g_dspModSXrx[sysex.sx].vt
			sysex.db		= midi.size-25												-- number of data bytes

			for b=1,sysex.db do 	v[b] 	= midi.msg:getData():getByte(23+b) end		-- calculate modulator value

			if 		mcalc[sysex.vt] 		then sysex._f	= mcalc[sysex.vt]
			elseif 	sysex.vt:match("nf-")	then sysex._f	= mcalc['nfx'] ; v[2] = sysex.vt:sub(4)
			end
			sysex.mval	= sysex._f(v[1],v[2],v[3])

			-- set DSP Tab:
			if  sysex.mid 	== "tssDSPTab" then
				setModNameIntProperty("tssDSPTab", "uiTabsCurrentTab", sysex.mval)
			else
				setModNameVal(sysex.mid, sysex.mval)
			end
		end
	end
end




--[[
#### BASE FUNCTIONS 'PROCESS MIDI SIGNAL'  #################################################################################
--]]


--
-- process sysEx Messages
--
local function procSysEx(midi)
	if midi==nil then do return end ; end

	-- data dump: 1 data-byte: sysex = 26by
	local	midicat	 = midi.msg:getData():getRange(0,7):toHexString(1)



	if 		midicat	 == g_XWSysEx.syn then 				rxTSS(midi)			-- solo synth
	elseif 	midicat	 == g_XWSysEx.dsp then 				rxDSP(midi)			-- DSP
	else
		-- console("SysEx received but not for me")
	end
end


--
-- process control change messages
--
local function procCC(midi, midiMSG)
	midi.Ch		= midiMSG:getData():getByte(0) - 0xb0
	midi.CC 	= midiMSG:getData():getByte(1)
	midi.val 	= midiMSG:getData():getByte(2)

	-- 1) always discard those CC: 

	if   midi.CC==0x64 or midi.CC==0x65 or midi.CC==0x06 or midi.CC==0x26 then do return end ; end 		-- discard RPN 

	-- 2) these CC are always captured:

	if		midi.CC == 0x40 and getModNameVal("KPedDmpBass") == 1 then									-- sustain foot pedal to 'bass'
		local ccmsg={0xb1, midi.CC, midi.val} ; 	sendMidiMsg(ccmsg)
	end

	if		midi.CC == 0x40 and midi.Ch == 3 and getModNameVal("KVpFPed") == 1 and getModNameVal("KVpSw") == 1 then		-- sustain foot pedal + V-Piano (sends CC64 for low and upp)
		if		midi.val == 0	then  	KVpPSustainPedal(0) ; setModNameVal('KVpPSustain',0) 
		elseif  midi.val == 127	then	KVpPSustainPedal(1) ; setModNameVal('KVpPSustain',1) 
		end
	elseif  midi.CC == 0x00 or midi.CC == 0x20 then 													-- (registration) bank select msb/lsb
		if		midi.CC == 0x00  then 	g_regPCH[midi.Ch].msb = midi.val
		elseif	midi.CC == 0x20  then 	g_regPCH[midi.Ch].lsb = midi.val
		end
	end

	-- 3) these CC are captured if CC 'rules' are active:

	if getModNameVal("KMMCCrcv") == 0 then do return end ; end

	-- apply the midi mapper rules:

	for i,rule in pairs(g_midiMapTmp.cc) do
		if (midi.CC == rule.cc and midi.Ch == rule.ch) and rule.sx and rule.act then
 			if 		rule.id == 'vrsx' 				then MMaction[rule.act](rule.sx, midi.val)
			elseif	string.sub(rule.id,1,2)	== 'vo' then MMaction[rule.act](rule.sx, midi.val)
			end
		end
	end
end



--
-- PC receive timer
--
function procPCrcv_t(tid)
	timer:stopTimer(tid) ; setModNameVal("STGMMPcLED", 0)
end

--
-- process registration program change messages
-- NOTE: VR sends 'program change' messages only for VR registrations: a captured PC is always from a registr.
--
local function procPC(midi, midiMSG)
	local pcrcv, VRMode, VPMode

	if getModNameVal("KMMPCrcv") == 0 then do return end ; end

	midi.Ch		= midiMSG:getData():getByte(0) - 0xc0					-- capture channel
	g_regPCH[midi.Ch].prg = midiMSG:getData():getByte(1)				-- capture prg to temp. variable

	-- check if bank-select + program is complete, otherwise return:

	if	g_regPCH[midi.Ch].msb and g_regPCH[midi.Ch].lsb and g_regPCH[midi.Ch].prg then
		pcrcv	= bit.lshift(g_regPCH[midi.Ch].msb,16) + bit.lshift(g_regPCH[midi.Ch].lsb,8) + g_regPCH[midi.Ch].prg
	else 
		do return end
	end
			
	-- init the VR (reset to 'defaults'):

	VPMode	= getModNameVal("KVpSw")
	if 		VPMode == 1 then setModNameVal("KVpSw", 0, true) end	-- 'Reset' V-Piano

	-- now apply the midi mapper rules:

	for i,rule in pairs(g_midiMapTmp.pc) do
		if (pcrcv == rule.pc and midi.Ch == rule.ch)  and rule.act then
			if 		rule.id	== 'vpsw' then MMaction[rule.act](rule.rv, rule.rp, 1, rule.rv2)-- V-Piano ON
			elseif	rule.id == 'girl' then MMaction[rule.act](rule.rv, rule.rg, rule.sn)	-- GM2 sound load to KBD
			elseif	rule.id == 'vsrl' then MMaction[rule.act](rule.rv, rule.rg, rule.sn)	-- (V-SYNTH REG patch load)
			elseif	rule.id == 'gmrl' then MMaction[rule.act](rule.rv, rule.rg, rule.sn)	-- (GM2 REG  patch load)
			end
		end
	end

	-- blink LED & clean
	
	setModNameVal("STGMMPcLED", 1) ; timer:setCallback(13001, procPCrcv_t) ; timer:stopTimer(13001) ; timer:startTimer(13001, 300) 	-- blink receive LED

	g_regPCH[midi.Ch]	= {}						-- empty the temporary bank-select-program storage triple
end



--[[
##############################################################################################################
####  MAIN  
##############################################################################################################
--]]
--
-- MAIN method for received midi messages
--
midiReceive = function(midiMSG)
	if not isPanelReady() then do return end ; end
	if midiMSG==nil then do return end ; end

	local midiHBy1		= bit.rshift( midiMSG:getData():getByte(0), 4 )	; if midiHBy1 == 8 or midiHBy1 == 9	then return end		-- exit for 'notes'

	-- eventually kick 'request reject' answer: f0 44 16 03 7f 0b 00 00 00 00 f7


	local midi 		  	= {}											-- midi message array: address Highest, High, Mid, Low; msg-size, msg-value
	midi.msg			= midiMSG
	midi.size 			= midiMSG:getSize() 							-- Size of the midi dump received
	midi.byte			= midiMSG:getData():getByte(0)					-- first byte

	-- MONITOR  console('midi msg in: '..midiMSG:getData():toHexString(1))	-- uncomment for testing or monitoring

	if 		midi.size >= 20 and midi.byte == 0xf0						then procSysEx(midi)
--	elseif	midi.size == 3 and bit.rshift(midi.byte,4) == 0xB  			then procCC(midi)
--	elseif	midi.size == 2 and bit.rshift(midi.byte,4) == 0xC			then procPC(midi)
	else	do return end
	end
end


-- ================================================================
-- METHOD 016: LayerHandler  (2019 bytes)
-- ================================================================

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


-- ================================================================
-- METHOD 017: ENVpaint  (2539 bytes)
-- ================================================================

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


-- ================================================================
-- METHOD 018: SYSControllers  (8825 bytes)
-- ================================================================

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


-- ================================================================
-- METHOD 019: ToneSoloSynth  (4502 bytes)
-- ================================================================

--
-- ToneSoloSynth: handler for Solo Synth layer
--

local setModNameVal			= setModNameVal
local setModNameMVal		= setModNameMVal
local getModNameIntProperty	= getModNameIntProperty
local sendCC 				= sendCC
local sendNRNP				= sendNRNP
local sendXWSX				= sendXWSX

--
-- Set/Init TSS params:
--
local function resetTSSParam(mid, osc)
	local osc		= tonumber(mid:gsub(".*-",""),10) or 1
	local oscgrp	= mid:sub(1,7)
	local oscmid, oscmid2, oscmval

	-- set:
	if mid:match("ENVcopy") then
		for m,av in pairs(g_tssModMidi["tssOSC"]) do
			if m:match(oscgrp) then
				oscmid		= m..'-'..(osc-1)
				oscmid2		= m..'-'..osc
				oscmval		= getModNameVal(oscmid) or 0
				setModNameMVal(oscmid2, oscmval, true)
			end
		end
	end

	-- init:
	if mid:match("ENVinit") then
		local oscgrp	= mid:sub(1,7)
		local oscmid, oscmval

		-- init
		for m,av in pairs( tableConcat(g_tssModMidi["tssOSC"], g_tssModMidi["tssFLT"]) ) do
			if m:match(oscgrp) then
				oscmid		= m..'-'..osc
				oscmval 	= getModNameIntProperty(oscmid,"uiSliderDoubleClickValue") or 0
				setModNameMVal(oscmid, oscmval, true)
			end
		end
	end
end


--# NRPN ====================================================================================================
--
-- send Midi values, print graphs
--[[
local function sendTSSParam(mid, val, zch)
	local mtp	= mid:gsub("-.*","")						-- modul. type (root)
	local osc	= tonumber(mid:gsub(".*-",""),10) or 1		-- suffix (-1, -2...)

	local ENVCanvas
	local nmsb, nlsb, vmsb=0, vlsb
	local mdef		= {}
	local mcalc		= g_xwModCalc

	if 		mtp:match("tssOSC") then mdef	= g_tssModMidi["tssOSC"]		-- OSC
	elseif 	mtp:match("tssLFO") then mdef	= g_tssModMidi["tssLFO"]		-- lfo
	elseif 	mtp:match("tssFLT") then mdef	= g_tssModMidi["tssFLT"]		-- total filter
	elseif 	mtp:match("tssDSP") then mdef	= g_tssModMidi["tssDSP"]		-- DSP config
	elseif 	mtp:match("tssCOM") then mdef	= g_tssModMidi["tssCOM"]		-- Common
	end

	if not mdef or not mdef[mtp] then do return end ; end

	nmsb	= mdef["tssMSBid"][osc]
	nlsb	= mdef[mtp].id

	-- calculate casio midi values
	if 		mcalc[mdef[mtp].vt] 		then vmsb,vlsb = mcalc[mdef[mtp].vt](val)
	elseif 	mdef[mtp].vt:match("nf-")	then vmsb,vlsb = mcalc['nfx'](val, mdef[mtp].vt:sub(4))
	end

	-- send Midi:
	if mdef["tssMIDI"]	== 'cc' 	then sendCC(zch,nlsb,vmsb)					end
	if mdef["tssMIDI"]	== 'nrnp' 	then sendNRNP(zch,nmsb,nlsb,vmsb, vlsb)		end

	-- draw ADSRR graph:
	if mtp:match('ENV') then
		local gvar	= 	mtp:gsub('.*ENV','')					-- eg 'r1T'
		local gmod	=	mtp:gsub(gvar,'')..'canv-'..osc			-- eg tssOSCPENVcanv-1

		g_CANVdata[gmod][gvar] = vmsb							-- assign slider value to global data
		ENVCanvas = panel:getComponent(gmod) ; if ENVCanvas then ENVCanvas:repaint() end
	end
end
--]]


--# SYSEX ====================================================================================================
--
-- send Midi values, print graphs
--
local function sendTSSParamSX(mid, val)
	local v1,v2,v3,n
	local mdef		= {}
	local mcalc		= g_xwModCalc["V2SX"]
	local sxid

	-- init/reset buttons
	if mid:match("ENVcopy") or mid:match("ENVinit") then resetTSSParam(mid, osc) ; return ; end

	-- treat modulators
	mdef	= g_tssModSXtx

	if not mdef or not mdef[mid] then do return end ; end
	v1		= 0

	-- set sysex fragment:
	sxid	= mdef[mid].sx

	-- calculate casio midi values
	if 		mdef[mid].vt:match("wf")	then n	= tonumber(mid:sub(-1,-1),10) or 1	end 			-- special case osc waveforms, osc suffix (-1, -2...)
	if 		mcalc[mdef[mid].vt] 		then v1,v2,v3 = mcalc[mdef[mid].vt](val,n) end				-- mmsb,msb,lsb

	-- send Midi:
	sendXWSX(1, sxid, v1, v2, v3)

	-- draw ADSRR graph:
	if mid:match('ENV') then
		local ENVCanvas
		local gvar	= 	mid:gsub('.*ENV',''):gsub("-.*","")		-- eg 'r1T'
		local gmod	=	mid:gsub(gvar,'canv')					-- eg 'tssOSCPENVcanv-1'
		g_CANVdata[gmod][gvar] = val							-- assign slider value to global data
		ENVCanvas = panel:getComponent(gmod) ; if ENVCanvas then ENVCanvas:repaint() end
	end
end


--# SYSEX ====================================================================================================


--
-- MAIN method
--
ToneSoloSynth = function(mod, value)
	if not isPanelReady() 	then do return end ; end
	if not value 			then do return end ; end

	local thisMod	= L(mod:getName())

	if 		thisMod:match("^tss") then 			sendTSSParamSX(thisMod, value)				-- TSS is always zone 1
	end
end



-- ================================================================
-- METHOD 020: ToneHexLayer  (1163 bytes)
-- ================================================================

--
-- ToneHexLayer: tone controller / mixer for HexLayers
--

local sendCC 	= sendCC
local sendNRNP	= sendNRNP


local function sendHEXParam(mid, val, lay, zch)
	local nmsb, nlsb, vmsb=0, vlsb
	local mdef		= {}
	local mdefcalc	= g_xwModCalc["nrpn"]

	if 		mid:match("mixHEX") then mdef	= g_mixModMidi["mixHEX"]			-- hexlayer
	end

	if not mdef or not mdef[mid] then do return end ; end

	nmsb	= mdef["mixMSBid"][lay]
	nlsb	= mdef[mid].id

	-- calculate casio midi values
	if 		mdefcalc[mdef[mid].vt] 		then vmsb,vlsb = mdefcalc[mdef[mid].vt](val, 23)
	elseif 	mdef[mid].vt:match("nf-")	then vmsb,vlsb = mdefcalc['nfx'](val, mdef[mid].vt:sub(4))
	end

	-- send Midi:
	if mdef["mixMIDI"]	== 'nrnp' 	then sendNRNP(zch,nmsb,nlsb,vmsb, vlsb)	end
end


--
-- MAIN method
--
ToneHexLayer = function(mod, value)
	if not isPanelReady() 	then do return end ; end
	if not value 			then do return end ; end

	local thisMod	= L(mod:getName())

	local modid	= thisMod:gsub("-.*","")
	local layid	= tonumber(thisMod:gsub(".*-",""),10) or 1

	sendHEXParam(modid, value, layid, g_XWChannels.rx[1])				-- HEXLAYER is always zone 1
end



-- ================================================================
-- METHOD 021: XWMixer  (2600 bytes)
-- ================================================================

--
-- XWMixer: controller for MIXER layer
--

local sendCC 		= sendCC
local sendNRNP		= sendNRNP
local mixDrumKits	= g_mixDrumKits


--
-- // in development //
--

local function sendMIXParam(mid, val, prt, zch)
	local mdef

	if 		mid:match("mixPRTdsp") 		then mdef	= g_mixModMidi.DSP				-- DSP
	elseif 	mid:match("mixPRTtone")		then sendPC(8, mixDrumKits[val+1].msb, 0, mixDrumKits[val+1].prg)
	elseif 	mid:match("mixPRTtss")		then sendPC(1, 98, 0, 2)
	elseif 	mid:match("tssFactPreset")	then sendPC(g_XWChannels.rx[prt], 98, 0, val) ; setModNameMVal("MIDIsync", 0, true)
	elseif 	mid:match("tssUserPreset")	then sendPC(g_XWChannels.rx[prt], 98, 0, val) ; setModNameMVal("MIDIsync", 0, true)
	elseif 	mid:match("tssCOMvol")		then sendCC(g_XWChannels.rx[prt], 0x07, val)
	elseif 	mid:match("tssCOMrevb")		then sendCC(g_XWChannels.rx[prt], 0x5b, val)
	end

	if not mdef or not mdef[mid] then do return end ; end
end


--[[
	-- SOUND LOAD

	-- get all sound definitions from modulator content box:

 	snddefs		= mod:getComponent():getProperty("uiComboContent")
	if 	snddefs == nil then toConsole(thisfunc.." snddefs = nil"); do return ; end ; end

	-- transform the sound definitions list from string to array:

	loadstring( "snddefs_t = {"..snddefs:gsub("^","\""):gsub("$","\""):gsub("\n","\",\"").."}" )()

	-- for selected sound definition, split the definition into array and get name and prgchange

	loadstring ("snd_t = {"..snddefs_t[idx]:gsub("^","\""):gsub("$","\""):gsub("[ ]*=[ ]*","\",\""):gsub("[Â°â ]","").."}")()
	sndname  	= snd_t[1]
	sndprgch 	= snd_t[2]

	-- split sndprgch into pc/msb/lsb:

	_prg	= string.sub(sndprgch,1,2) ; 	s.prg		= tonumber(_prg, 16)
	_msb	= string.sub(sndprgch,3,4) ; 	s.msb		= tonumber(_msb, 16)
	_lsb	= string.sub(sndprgch,5,6) ; 	s.lsb		= tonumber(_lsb, 16)

	-- modulators:

	if thisMod == "VKbRyDrums" then			-- special treatment for 'GM2 rhythm drum set' 
		local ccmsg
		local midiChannel = 9

		ccmsg={ (0xb0+midiChannel),0x00, s.msb} ; 	sendMidiMsg(ccmsg)
		ccmsg={ (0xb0+midiChannel),0x20, s.lsb} ; 	sendMidiMsg(ccmsg)
		ccmsg={ (0xc0+midiChannel),      s.prg} ; 	sendMidiMsg(ccmsg)

		do return end
	end
--]]




--
-- MAIN method
--
XWMixer = function(mod, value)
	if not isPanelReady() 	then do return end ; end
	if not value 			then do return end ; end

	local thisMod	= L(mod:getName())

	local modid	= thisMod:gsub("-.*","")
	local prtid	= tonumber(thisMod:gsub(".*-",""),10) or 1					-- part 1-16

	sendMIXParam(modid, value, prtid, g_XWChannels.rx[prtid])
end


-- ================================================================
-- METHOD 022: XWOrgan  (4191 bytes)
-- ================================================================

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



-- ================================================================
-- METHOD 023: DSPHandler  (1002 bytes)
-- ================================================================

--
-- DSPHandler: handler for TSS and general DSPs
--


local sendXWSX		= sendXWSX



--
-- setDSPtssParam: set DSP for TSS
--
local function setDSPtssParam(mid, val)
	local v1,v2,v3
	local mdef		= {}
	local mcalc		= g_xwModCalc["V2SX"]
	local sxid

 	mdef	= g_dspModSXtx

	if not mdef or not mdef[mid] then do return end ; end

	-- set sysex fragment:
	sxid	= mdef[mid].sx
	-- calculate casio midi values
	if 		mcalc[mdef[mid].vt] 		then v1,v2 		= mcalc[mdef[mid].vt](val)
	elseif 	mdef[mid].vt:match("nf-")	then v1,v2		= mcalc['nfx'](val, mdef[mid].vt:sub(4))
	end

	-- send Midi:
	sendXWSX(1, sxid, v1, v2, v3)
end


--
-- MAIN method
--
DSPHandler = function(mod, value)
	if not isPanelReady() 	then do return end ; end
	if not value 			then do return end ; end

	local thisMod	= L(mod:getName())

	if 		thisMod:match("tssDSPTab") 	then setDSPtssParam(thisMod, value)
	elseif 	thisMod:match("tssDSP")		then setDSPtssParam(thisMod, value)
	end

end



-- ================================================================
-- METHOD 024: EXTPrograms  (6692 bytes)
-- ================================================================

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
