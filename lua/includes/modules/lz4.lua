if not CLIENT then return end

require( "html_loader" )

if lz4 then
    for _, panel in ipairs( lz4._panels ) do
        panel:Remove()
    end
end

--- @class lz4
lz4 = lz4 or {}
lz4._panels = {}

--- Makes a panel to handle th enew lz4 function
--- @return DHTML
function lz4:_makePanel()
    local callback = function( _ ) end

    local panel = vgui.Create( "DHTML" ) --[[@as DHTML]]
    table.insert( self._panels, panel )

    panel:SetSize( 0, 0 )
    panel:SetAllowLua( true )

    function panel:OnDocumentReady()
        panel:AddFunction( "gmod", "Report", function( data )
            self:Remove()
            callback( data )
        end )
    end

    --- @param data string
    --- @param cb function
    function panel:Compress( data, cb )
        callback = cb
        data = string.JavascriptSafe( data )
        self:QueueJavascript( [[Compress( "]] .. data .. [[" )]] )
    end

    --- @param compressed string
    --- @param cb function
    function panel:Decompress( compressed, cb )
        callback = cb
        compressed = string.JavascriptSafe( compressed )
        self:QueueJavascript( [[Decompress( "]] .. compressed .. [[" )]] )
    end

    return panel
end

-- Blocking
do
    --- Compresses the given string synchronously
    --- @param data string
    --- @return string compressed The compressed data
    function lz4.Compress( data )
        local panel = lz4:_makePanel()

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
        local panel = lz4:_makePanel()

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
        local panel = lz4:_makePanel()
        panel:Compress( data, cb )
    end

    --- Decompresses the given string asynchronously
    --- @param compressed string
    --- @param cb function
    function lz4.DecompressAsync( compressed, cb )
        local panel = lz4:_makePanel()
        panel:Compress( compressed, cb )
    end
end
