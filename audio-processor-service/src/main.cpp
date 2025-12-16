#include <iostream>
#include <memory>
#include <string>
#include <cstdlib>
#include <thread>
#include <algorithm>
#include <grpcpp/grpcpp.h>
#include "audio_processor_service_async.h"

static int parseConcurrencyFromEnv() {
    const char* env = std::getenv("AUDIO_PROC_MAX_CONCURRENCY");
    if (env && *env) {
        try {
            int v = std::stoi(env);
            if (v > 0) return v;
        } catch (...) {}
    }
    unsigned int hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 2;
    int def = std::max(1u, hw / 2);
    return std::clamp(def, 1, 1024);
}

int main(int argc, char** argv) {
    try {
        std::string server_address("0.0.0.0:50051");
        int maxConcurrency = parseConcurrencyFromEnv();
        int numCQThreads = std::max(1, maxConcurrency / 2);  // 1 CQ thread per 2 RPC permits
        
        AudioProcessorAsync server(maxConcurrency);
        server.Run(server_address, numCQThreads);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}