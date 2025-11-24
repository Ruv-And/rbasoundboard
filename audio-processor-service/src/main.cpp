#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "audio_processor_service.h"

void RunServer() {
    std::string server_address("0.0.0.0:50051");
    AudioProcessorServiceImpl service;

    grpc::ServerBuilder builder;
    
    // Listen on the given address without any authentication
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    
    // Register service
    builder.RegisterService(&service);
    
    // Build and start server
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "========================================" << std::endl;
    std::cout << "Audio Processor Server listening on " << server_address << std::endl;
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