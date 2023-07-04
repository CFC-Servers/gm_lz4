if not CLIENT then return end

include( "lz4/lz4_html.lua" )

if lz4 then
    for _, panel in ipairs( lz4._panels ) do
        panel:Remove()
    end
end

local string_gsub = string.gsub
local function sanitize( str )
    local newstr, replacements = string_gsub( str, "\\", "\\\\" )
    print( "Replaced", replacements, "backslashes" )

    newstr, replacements = string_gsub( newstr, "\"", "\\\"" )
    print( "Replaced", replacements, "quotes" )

    return newstr
end

--- @class lz4
lz4 = lz4 or {}
lz4._panels = {}

--- Makes a panel to handle th enew lz4 function
--- @return DHTML
function lz4:_makePanel()
    local callback = function( _ ) end

    local startTime
    local panel = vgui.Create( "DHTML" ) --[[@as DHTML]]
    table.insert( self._panels, panel )

    panel.Paint = function() end
    panel:SetSize( 1, 1 )
    panel:SetAllowLua( true )
    panel:AddFunction( "gmod", "Report", function( data )
        local b64start = SysTime()
        local unb64 = util.Base64Decode( data )
        print( "B64 Took", SysTime() - b64start )
        print( "Final size:", string.len( unb64 ) )

        print( "Full process Took", SysTime() - startTime )

        callback( unb64 )

        panel:Remove()
    end )

    panel:SetHTML( lz4_html )

    --- @param data string
    --- @param cb function
    function panel:Compress( data, cb )
        callback = cb

        print( "Compressing", #data, "bytes" )
        startTime = SysTime()
        self:QueueJavascript( [[Compress( "]] .. data .. [[" )]] )
        self:Think()
    end

    --- @param compressed string
    --- @param cb function
    function panel:Decompress( compressed, cb )
        callback = cb
        compressed = string.JavascriptSafe( compressed )
        startTime = SysTime()
        self:QueueJavascript( [[Decompress( "]] .. compressed .. [[" )]] )
        self:Think()
    end

    return panel
end

function lz4:_getPanel()
    local panels = self._panels
    local panel = panels[#panels]

    -- Make a new one for the next user
    self:_makePanel()

    -- return the one that was ready
    return panel
end

-- Blocking
do
    --- Compresses the given string synchronously
    --- @param data string
    --- @return string compressed The compressed data
    function lz4.Compress( data )
        local panel = lz4:_getPanel()

        local compressedData = nil
        panel:Compress( data, function( compressed )
            compressedData = compressed
        end )

        while not compressedData do
            panel:Think()
            SysTime()
        end

        return compressedData
    end

    --- Decompresses the given string synchronously
    --- @param compressed string
    --- @return string decompressed The decompressed data
    function lz4.Decompress( compressed )
        local panel = lz4:_getPanel()

        local decompressedData = nil
        panel:Compress( compressed, function( data )
            decompressedData = data
        end )

        while not decompressedData do
            panel:Think()
            SysTime()
        end
    end
end

-- Async
do
    --- Compresses the given string asynchronously
    --- @param data string
    --- @param cb function
    function lz4.CompressAsync( data, cb )
        local panel = lz4:_getPanel()
        panel:Compress( data, cb )
    end

    --- Decompresses the given string asynchronously
    --- @param compressed string
    --- @param cb function
    function lz4.DecompressAsync( compressed, cb )
        local panel = lz4:_getPanel()
        panel:Compress( compressed, cb )
    end
end

lz4:_makePanel()
