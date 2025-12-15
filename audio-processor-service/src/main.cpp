#include <iostream>
#include <memory>
#include <string>
#include <cstdlib>
#include <thread>
#include <sstream>
#include <algorithm>
#include <grpcpp/grpcpp.h>
#include "audio_processor_service.h"

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

void RunServer() {
    std::string server_address("0.0.0.0:50051");
    int maxConcurrency = parseConcurrencyFromEnv();
    AudioProcessorServiceImpl service(maxConcurrency);

    grpc::ServerBuilder builder;
    
    // Listen on the given address without any authentication
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    
    // Configure bounded sync server thread pool
    builder.SetSyncServerOption(grpc::ServerBuilder::SyncServerOption::NUM_CQS, 1);
    builder.SetSyncServerOption(grpc::ServerBuilder::SyncServerOption::MIN_POLLERS, maxConcurrency);
    builder.SetSyncServerOption(grpc::ServerBuilder::SyncServerOption::MAX_POLLERS, maxConcurrency);
    grpc::ResourceQuota rq;
    rq.SetMaxThreads(maxConcurrency + 2);
    builder.SetResourceQuota(rq);

    // Register service
    builder.RegisterService(&service);
    
    // Build and start server
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "========================================" << std::endl;
    std::cout << "Audio Processor Server listening on " << server_address << std::endl;
    std::cout << "Max concurrency: " << maxConcurrency << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Wait for server to shutdown
    server->Wait();
}

int main(int argc, char** argv) {
    try {
        RunServer();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}