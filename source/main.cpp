#include "GarrysMod/Lua/Interface.h"
#include <thread>
#include <iostream>
#include <lz4frame.h>
#include <cstring>

namespace global {

LUA_FUNCTION_STATIC(CompressString) {
    const char* input = LUA->GetString(1);
    size_t input_size = LUA->StringLength(1);

    // Set up preferences and compressor
    LZ4F_preferences_t prefs;
    memset(&prefs, 0, sizeof(prefs));
    prefs.frameInfo.blockSizeID = LZ4F_max64KB;
    prefs.frameInfo.blockMode = LZ4F_blockLinked;
    prefs.frameInfo.contentChecksumFlag = LZ4F_contentChecksumEnabled;
    prefs.compressionLevel = 0;

    LZ4F_compressionContext_t ctx;
    size_t ctxSize = 0;
    LZ4F_errorCode_t err = LZ4F_createCompressionContext(&ctx, LZ4F_VERSION);
    if (LZ4F_isError(err)) {
        // Handle error
        LUA->ThrowError("LZ4F_createCompressionContext error");
        return 1;
    }

    err = LZ4F_getBlockSize(ctx, &prefs.frameInfo, &prefs.compressionLevel);
    if (LZ4F_isError(err)) {
        // Handle error
        LZ4F_freeCompressionContext(ctx);
        LUA->ThrowError("LZ4F_getBlockSize error");
        return 1;
    }

    std::vector<char> compressed_data(LZ4F_compressBound(input_size, &prefs));

    size_t compressed_size = LZ4F_compressBegin(ctx, compressed_data.data(), compressed_data.size(), &prefs);
    if (LZ4F_isError(compressed_size)) {
        // Handle error
        LZ4F_freeCompressionContext(ctx);
        LUA->ThrowError("LZ4F_compressBegin error");
        return 1;
    }

    size_t input_pos = 0, output_pos = compressed_size;
    size_t block_size = prefs.frameInfo.blockSizeID * 1024;
    while (input_pos < input_size) {
        size_t input_remaining = input_size - input_pos;
        size_t block_remaining = block_size - (output_pos - compressed_size);
        size_t chunk_size = (input_remaining < block_remaining) ? input_remaining : block_remaining;

        err = LZ4F_compressUpdate(ctx, compressed_data.data() + output_pos, compressed_data.size() - output_pos, input + input_pos, chunk_size, NULL);
        if (LZ4F_isError(err)) {
            // Handle error
            LZ4F_freeCompressionContext(ctx);
            LUA->ThrowError("LZ4F_compressUpdate error");
            return 1;
        }

        input_pos += chunk_size;
        output_pos += err;
    }

    size_t flush_size = LZ4F_compressEnd(ctx, compressed_data.data() + output_pos, compressed_data.size() - output_pos, NULL);
    if (LZ4F_isError(flush_size)) {
        // Handle error
        LZ4F_freeCompressionContext(ctx);
        LUA->ThrowError("LZ4F_compressEnd error");
        return 1;
    }

    output_pos += flush_size;

    LZ4F_freeCompressionContext(ctx);

    LUA->PushLString(compressed_data.data(), output_pos);

    return 1;
}




static void Initialize(GarrysMod::Lua::ILuaBase *LUA) {
    LUA->CreateTable();

    LUA->PushCFunction(CompressString);
    LUA->SetField(-2, "CompressString");

    LUA->SetField(GarrysMod::Lua::INDEX_GLOBAL, "lz4");
}

static void Deinitialize(GarrysMod::Lua::ILuaBase *LUA) {
    LUA->PushNil();
    LUA->SetField(GarrysMod::Lua::INDEX_GLOBAL, "lz4");
}

}

GMOD_MODULE_OPEN() {
    global::Initialize(LUA);
    return 1;
}

GMOD_MODULE_CLOSE() {
    global::Deinitialize(LUA);
    return 0;
}

