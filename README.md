# gm_lz4

A high-performance Garry's Mod server-side module that provides an ultra-fast alternative to Garry's Mod's built-in util.Compress and util.Decompress functions using the LZ4 compression algorithm. In addition to synchronous compression and decompression, gm_lz4 offers asynchronous functions that run in a separate thread, ensuring smooth gameplay even when working with large data.

## Description

LZ4 is a compression algorithm that boasts "extremely fast decoding, with speed in multiple GB/s per core". In contrast, Garry's Mod's default LZMA-powered util functions can be painfully slow. Although they offer an excellent compression ratio, they make working with large data in Garry's Mod challenging.

### Benchmark

**Input**: 35.77 MB of JSON-ified table data
| Method | Avg. Time | Ratio |
|------|----------|------------|
| util.Compress | 6.912s | `2.1`:`1` |
| lz4.Compress | 1.503s | `1.4`:`1` |

_Note: The seven-second processing time for `util.Compress` would cause the game and all players to freeze for seven consecutive seconds if run server-side._

In general, `lz4.Compress` is 4-6 times faster than util.Compress but produces a result that is approximately 30% larger.
Use cases

`gm_lz4` is ideal for situations where speed is more crucial than compression ratio. It is best suited for add-ons that require handling large amounts of data at once. Even when dealing with non-time-sensitive data, lz4.Compress is preferable, as it doesn't lock up your server for nearly as long.
Asynchronous Functions

While the `gm_lz4` module is already incredibly fast, it also offers asynchronous alternatives for both compressing and decompressing, ensuring smooth server performance.

## Usage

```lua
require( "lz4" )

local data = generateData()
local serialized = util.TableToJSON( data )

do
    local compressed = lz4.Compress( serialized )
    local decompressed = lz4.Decompress( compressed )
end

-- Async
lz4.CompressAsync( serialized, function( compressed )
    -- Now we have the compressed data, we can also decompress it asynchronously

    lz4.DecompressAsync( compressed, function( decompressed )
        assert( decompressed == serialized )
    end )
end )

## Building
TODO: Write better instructions than danielga :)
