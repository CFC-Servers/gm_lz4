# gm_lz4 🚀

Introducing **gm_lz4**, a high-performance Garry's Mod server-side module that provides a lightning-fast alternative to Garry's Mod's built-in `util.Compress` and `util.Decompress` functions, leveraging the power of the [LZ4](https://lz4.github.io/lz4/) compression algorithm.

It offers basic synchronous functions as well as asynchronous functions that run in a separate thread, preventing server freezes even when handling extremely large datasets.

## 📚 Description

LZ4, a compression algorithm known for its "extremely fast decoding, with speed in multiple GB/s per core", outperforms Garry's Mod's default LZMA-powered util functions.
While LZMA offers an excellent compression ratio, its sluggish performance makes working with large data in Garry's Mod a challenge.

LZ4 prioritizes speed over compression ratio. It's roughly 4-6x faster than Garry's Mod's LZMA, but produces a result that is approximately 30% larger.

### ⚡ Benchmark

**Input**: 35.77 MB of JSON-ified table data
| Method | Avg. Time | Ratio |
|------|----------|------------|
| util.Compress | 6.912s | `2.1`:`1` |
| lz4.Compress | 1.503s | `1.4`:`1` |

_Note: The seven-second processing time for `util.Compress` would cause the game and all players to freeze for seven consecutive seconds if run server-side._

### 🎯 Use Cases
`gm_lz4` excels in situations where speed takes precedence over compression ratio.

It's an ideal choice for addons that need to handle large amounts of data. Even for non-time-sensitive data, `lz4.Compress` may still be preferable as it doesn't lock up your server for long periods.

###  🧩 Asynchronous Functions
The `gm_lz4` module, while already blazing fast, also provides asynchronous alternatives for both compressing and decompressing to ensure smooth server performance.

## 🛠️ Usage

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
