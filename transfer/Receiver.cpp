#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#if defined(__linux__) || defined(__APPLE__)
#include <arpa/inet.h>
#include <unistd.h>
#define CLOSE_SOCKET close
using socket_t = int;
#else
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#define CLOSE_SOCKET closesocket
using socket_t = SOCKET;
#endif

// Helper: Receive a single file from socket
void receiveSingleFile(socket_t client, const std::string &destDir, size_t fileIndex, size_t totalFiles) {
    // Receive filename length + filename
    size_t name_len = 0;
    recv(client, reinterpret_cast<char*>(&name_len), sizeof(name_len), 0);

    std::string filename(name_len, '\0');
    recv(client, &filename[0], static_cast<int>(name_len), 0);

    std::cout << "[" << (fileIndex + 1) << "/" << totalFiles << "] Receiving: " << filename << "\n";

    // Receive file size
    size_t filesize;
    recv(client, reinterpret_cast<char*>(&filesize), sizeof(filesize), 0);

    // Save file to destination directory
    try {
        std::filesystem::path dir(destDir);
        if(!dir.empty()) std::filesystem::create_directories(dir);
        std::filesystem::path outPath = std::filesystem::path(destDir) / filename;
        std::ofstream file(outPath.string(), std::ios::binary);
        
        char buffer[4096];
        size_t received = 0;
        while(received < filesize) {
            int bytes = recv(client, buffer, sizeof(buffer), 0);
            file.write(buffer, bytes);
            received += bytes;
            std::cout << "\rProgress: " << (received * 100 / filesize) << "%";
            std::cout.flush();
        }

        std::cout << "\n";
        file.close();
    } catch(const std::exception &ex) {
        std::cerr << "Error saving file: " << ex.what() << "\n";
    }
}

void receiveFile(int port = 9999, const std::string &destDir = ".") {
    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) { perror("socket"); return; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return; }
    listen(sock, 1);

    std::cout << "Waiting for incoming file(s)...\n";
    socket_t client = accept(sock, nullptr, nullptr);
    if(client < 0) { perror("accept"); return; }

    // Receive number of files first
    size_t num_files = 0;
    recv(client, reinterpret_cast<char*>(&num_files), sizeof(num_files), 0);
    std::cout << "Receiving " << num_files << " file(s)...\n";

    // Receive each file from the queue
    for(size_t i = 0; i < num_files; ++i) {
        receiveSingleFile(client, destDir, i, num_files);
    }

    std::cout << "\nAll files received successfully!\n";
    CLOSE_SOCKET(client);
    CLOSE_SOCKET(sock);
}
