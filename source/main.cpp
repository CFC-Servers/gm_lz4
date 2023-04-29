#include "GarrysMod/Lua/Interface.h"
#include "GarrysMod/Lua/LuaGameCallback.h"
#include <thread>
#include <functional>
#include <iostream>
#include <lz4.h>
#include <lz4frame.h>
#include <lz4frame_static.h>
#include <lz4hc.h>
#include <cstring>
#include <vector>
#include <mutex>
#include <queue>

namespace global {

// Thread-safe task queue
std::mutex taskQueueMutex;
std::queue<std::function<void()>> taskQueue;

void ScheduleOnMainThread(std::function<void()> task) {
    std::unique_lock<std::mutex> lock(taskQueueMutex);
    taskQueue.push(task);
}


std::vector<char> CompressStringInternal(const char *input, size_t input_size) {
    // Set up preferences and compressor
    LZ4F_preferences_t prefs;
    memset(&prefs, 0, sizeof(prefs));
    prefs.frameInfo.blockSizeID = LZ4F_max64KB;
    prefs.frameInfo.blockMode = LZ4F_blockLinked;
    prefs.frameInfo.contentChecksumFlag = LZ4F_contentChecksumEnabled;
    prefs.compressionLevel = 9;

    LZ4F_cctx* ctx;
    LZ4F_errorCode_t err = LZ4F_createCompressionContext(&ctx, LZ4F_VERSION);
    if (LZ4F_isError(err)) {
        throw std::runtime_error("LZ4F_createCompressionContext error");
    }

    std::vector<char> compressed_data(LZ4F_compressBound(input_size, &prefs));

    size_t header_size = LZ4F_compressBegin(ctx, compressed_data.data(), compressed_data.size(), &prefs);
    if (LZ4F_isError(header_size)) {
        LZ4F_freeCompressionContext(ctx);
        throw std::runtime_error("LZ4F_compressBegin error");
    }

    size_t input_pos = 0, output_pos = header_size;
    while (input_pos < input_size) {
        size_t input_remaining = input_size - input_pos;
        size_t dst_capacity = LZ4F_compressBound(input_remaining, &prefs);

        if (compressed_data.size() - output_pos < dst_capacity) {
            compressed_data.resize(output_pos + dst_capacity);
        }

        err = LZ4F_compressUpdate(ctx, compressed_data.data() + output_pos, compressed_data.size() - output_pos, input + input_pos, input_remaining, NULL);
        if (LZ4F_isError(err)) {
            LZ4F_freeCompressionContext(ctx);
            throw std::runtime_error("LZ4F_compressUpdate error");
        }

        input_pos += input_remaining;
        output_pos += err;
    }

    size_t end_size = LZ4F_compressEnd(ctx, compressed_data.data() + output_pos, compressed_data.size() - output_pos, NULL);
    if (LZ4F_isError(end_size)) {
        LZ4F_freeCompressionContext(ctx);
        throw std::runtime_error("LZ4F_compressEnd error");
    }

    output_pos += end_size;

    LZ4F_freeCompressionContext(ctx);

    compressed_data.resize(output_pos);
    return compressed_data;
}


void CompressStringAsyncFunction(const std::string &input, std::function<void(const std::vector<char> &)> callback) {
    try {
        std::vector<char> compressed_data = CompressStringInternal(input.data(), input.size());
        callback(compressed_data);
    } catch (const std::runtime_error &e) {
        std::cerr << "CompressStringAsyncFunction error: " << e.what() << std::endl;
    }
}

LUA_FUNCTION_STATIC(CompressString) {
    size_t input_size = 0;
    const char *input = LUA->GetString(1, &input_size);

    try {
        std::vector<char> compressed_data = CompressStringInternal(input, input_size);
        LUA->PushString(compressed_data.data(), compressed_data.size());
    } catch (const std::runtime_error &e) {
        LUA->ThrowError(e.what());
    }

    return 1;
}


LUA_FUNCTION_STATIC(CompressStringAsync) {
    size_t input_size = 0;
    const char *input = LUA->GetString(1, &input_size);
    std::string input_str(input, input_size);

    LUA->CheckType(2, GarrysMod::Lua::Type::FUNCTION);

    // Store the callback function reference in the registry
    LUA->Push(2);
    int callbackRef = LUA->ReferenceCreate();

    // Create a new thread to run the compression asynchronously
    std::thread([=]() {
        // Call the async function with the captured input and callback
        CompressStringAsyncFunction(input_str, [LUA, callbackRef](const std::vector<char> &compressed_data) {
            ScheduleOnMainThread([=]() {
                // Get the callback function from the registry
                LUA->ReferencePush(callbackRef);
                LUA->PushString(compressed_data.data(), compressed_data.size());

                // Call the callback function
                LUA->Call(1, 0);

                // Free the callback function reference
                LUA->ReferenceFree(callbackRef);
            });
        });
    }).detach();

    return 0;
}

LUA_FUNCTION_STATIC(Think) {
    std::unique_lock<std::mutex> lock(taskQueueMutex);

    while (!taskQueue.empty()) {
        auto task = taskQueue.front();
        taskQueue.pop();
        lock.unlock();

        task();

        lock.lock();
    }

    return 0;
}


static void Initialize(GarrysMod::Lua::ILuaBase *LUA) {
    LUA->CreateTable();

    LUA->PushCFunction(CompressString);
    LUA->SetField(-2, "CompressString");

    LUA->PushCFunction(CompressStringAsync);
    LUA->SetField(-2, "CompressStringAsync");

    // LUA->PushCFunction(DecompressString);
    // LUA->SetField(-2, "DecompressString");

    LUA->SetField(GarrysMod::Lua::INDEX_GLOBAL, "lz4");

    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->GetField(-1, "hook");
    LUA->GetField(-1, "Add");
    LUA->PushString("Think");
    LUA->PushString("MyModule_Think");
    LUA->PushCFunction(Think);
    LUA->Call(3, 0);
    LUA->Pop(2);
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

