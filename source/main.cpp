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
#include "dbg.h"

namespace global {

// Thread-safe task queue
std::mutex taskQueueMutex;
std::queue<std::function<void()>> taskQueue;

void ScheduleOnMainThread(std::function<void()> task) {
    std::unique_lock<std::mutex> lock(taskQueueMutex);
    taskQueue.push(task);
}


std::vector<char> DecompressStringInternal(const char *input, size_t input_size) {
    LZ4F_dctx *dctx;
    LZ4F_errorCode_t err_create = LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
    if (LZ4F_isError(err_create)) {
        throw std::runtime_error("LZ4F_createDecompressionContext error");
    }

    // Get the decompressed data size from the frame header
    LZ4F_frameInfo_t frame_info;
    memset(&frame_info, 0, sizeof(frame_info));
    size_t input_consumed = input_size;
    LZ4F_errorCode_t err_frameinfo = LZ4F_getFrameInfo(dctx, &frame_info, input, &input_consumed);
    if (LZ4F_isError(err_frameinfo)) {
        LZ4F_freeDecompressionContext(dctx);
        throw std::runtime_error("LZ4F_getFrameInfo error");
    }

    size_t decompressed_data_size = frame_info.contentSize;
    std::vector<char> decompressed_data(decompressed_data_size);

    size_t src_pos = input_consumed, dst_pos = 0;
    LZ4F_decompressOptions_t options = {0};

    DevMsg("Input size (compressed): %zu\n", input_size);
    DevMsg("Input data (compressed): ");
    for (size_t i = 0; i < input_size; ++i) {
        DevMsg("%02X", static_cast<unsigned char>(input[i]));
    }
    DevMsg("\n");
    DevMsg("Decompressed data size: %zu\n", decompressed_data_size);

    while (src_pos < input_size) {
        size_t src_size = input_size - src_pos;
        size_t dst_size = decompressed_data_size - dst_pos;

        LZ4F_errorCode_t err_decompress = LZ4F_decompress(dctx, decompressed_data.data() + dst_pos, &dst_size, input + src_pos, &src_size, &options);
        if (LZ4F_isError(err_decompress)) {
            std::string error_msg = "LZ4F_decompress error: ";
            error_msg += LZ4F_getErrorName(err_decompress);
            LZ4F_freeDecompressionContext(dctx);
            throw std::runtime_error(error_msg);
        }


        src_pos += err_decompress;
        dst_pos += dst_size;
        DevMsg("Decompression progress: src_pos = %zu, dst_pos = %zu, src_size = %zu, dst_size = %zu\n", src_pos, dst_pos, src_size, dst_size);

        // Check if the decompression process is completed
        if (err_decompress == 0) {
            break;
        }
    }

    DevMsg("Decompression finished: src_pos = %zu, dst_pos = %zu\n", src_pos, dst_pos);



    LZ4F_freeDecompressionContext(dctx);
    decompressed_data.resize(dst_pos);
    return decompressed_data;
}




void DecompressStringAsyncFunction(const std::string &compressed_data, std::function<void(const std::vector<char> &)> callback) {
    try {
        std::vector<char> decompressed_data = DecompressStringInternal(compressed_data.data(), compressed_data.size());
        callback(decompressed_data);
    } catch (const std::runtime_error &e) {
        std::cerr << "DecompressStringAsyncFunction error: " << e.what() << std::endl;
    }
}


LUA_FUNCTION_STATIC(DecompressString) {
    size_t input_size = 0;
    const char *input = LUA->GetString(1, &input_size);

    try {
        std::vector<char> decompressed_data = DecompressStringInternal(input, input_size);
        LUA->PushString(decompressed_data.data(), decompressed_data.size());
    } catch (const std::runtime_error &e) {
        LUA->ThrowError(e.what());
    }

    return 1;
}

LUA_FUNCTION_STATIC(DecompressStringAsync) {
    size_t input_size = 0;
    const char *input = LUA->GetString(1, &input_size);
    std::string input_str(input, input_size);

    LUA->CheckType(2, GarrysMod::Lua::Type::FUNCTION);

    // Store the callback function reference in the registry
    LUA->Push(2);
    int callbackRef = LUA->ReferenceCreate();

    // Create a new thread to run the decompression asynchronously
    std::thread([=]() {
        // Call the async function with the captured input and callback
        DecompressStringAsyncFunction(input_str, [LUA, callbackRef](const std::vector<char> &decompressed_data) {
            ScheduleOnMainThread([=]() {
                // Get the callback function from the registry
                LUA->ReferencePush(callbackRef);
                LUA->PushString(decompressed_data.data(), decompressed_data.size());

                LUA->Call(1, 0);

                // Free the callback function reference
                LUA->ReferenceFree(callbackRef);
            });
        });
    }).detach();

    return 0;
}


std::vector<char> CompressStringInternal(const char *input, size_t input_size) {
    DevMsg("Input size: %zu\n", input_size);
    DevMsg("Input data: ");
    for (size_t i = 0; i < input_size; ++i) {
        DevMsg("%02X", static_cast<unsigned char>(input[i]));
    }
    DevMsg("\n");

    // Set up preferences and compressor
    LZ4F_preferences_t prefs;
    memset(&prefs, 0, sizeof(prefs));
    prefs.frameInfo.blockSizeID = LZ4F_max64KB;
    prefs.frameInfo.blockMode = LZ4F_blockLinked;
    prefs.frameInfo.contentChecksumFlag = LZ4F_contentChecksumEnabled;
    prefs.frameInfo.contentSize = input_size;

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

    DevMsg("Compressed size: %zu\n", compressed_data.size());
    DevMsg("Compressed data: ");
    for (size_t i = 0; i < compressed_data.size(); ++i) {
        DevMsg("%02X", static_cast<unsigned char>(compressed_data[i]));
    }
    DevMsg("\n");

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


void TestCompressionDecompression(GarrysMod::Lua::ILuaBase *LUA) {
    std::string input_str = "This is a test string to check compression and decompression.";
    std::vector<char> compressed_data = CompressStringInternal(input_str.data(), input_str.size());
    DevMsg("Compressed size: %d\n", compressed_data.size());

    try {
        std::vector<char> decompressed_data = DecompressStringInternal(compressed_data.data(), compressed_data.size());
        std::string decompressed_str(decompressed_data.begin(), decompressed_data.end());
        DevMsg("Decompressed size: %d\n", decompressed_str.size());
        DevMsg("Decompressed data: ");
        for (size_t i = 0; i < decompressed_str.size(); ++i) {
            DevMsg("%02X", static_cast<unsigned char>(decompressed_str[i]));
        }
        DevMsg("\n");
    } catch (const std::runtime_error &e) {
        LUA->ThrowError(e.what());
    }
}


static void Initialize(GarrysMod::Lua::ILuaBase *LUA) {
    TestCompressionDecompression(LUA);
    LUA->CreateTable();

    LUA->PushCFunction(CompressString);
    LUA->SetField(-2, "CompressString");

    LUA->PushCFunction(CompressStringAsync);
    LUA->SetField(-2, "CompressStringAsync");

    LUA->PushCFunction(DecompressString);
    LUA->SetField(-2, "DecompressString");

    LUA->PushCFunction(DecompressStringAsync);
    LUA->SetField(-2, "DecompressStringAsync");

    LUA->SetField(GarrysMod::Lua::INDEX_GLOBAL, "lz4");

    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->GetField(-1, "timer");
    LUA->GetField(-1, "Create");
    LUA->PushString("MyModule_Think");
    LUA->PushNumber(0);
    LUA->PushNumber(0);
    LUA->PushCFunction(Think);
    LUA->Call(4, 0);
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

