#include <iostream>
#include <fstream>
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
// Sender.cpp
void sendFile(const std::string& filename, const std::string& ip, int port = 9999) {
    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) { perror("socket"); return; }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);

    if(connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("connect"); return;
    }

    std::ifstream file(filename, std::ios::binary);
    if(!file.is_open()) { std::cerr << "Cannot open file\n"; return; }

    // Send filename length + filename first
    std::string send_name = std::filesystem::path(filename).filename().string();
    size_t name_len = send_name.size();
    send(sock, reinterpret_cast<const char*>(&name_len), sizeof(name_len), 0);
    send(sock, send_name.c_str(), static_cast<int>(name_len), 0);

    // Send file size next
    file.seekg(0, std::ios::end);
    size_t filesize = file.tellg();
    file.seekg(0, std::ios::beg);
    send(sock, reinterpret_cast<const char*>(&filesize), sizeof(filesize), 0);

    char buffer[4096];
    size_t sent = 0;
    while(file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        send(sock, buffer, file.gcount(), 0);
        sent += file.gcount();
        std::cout << "\rProgress: " << (sent * 100 / filesize) << "%";
        std::cout.flush();
    }
    std::cout << "\nFile sent successfully!\n";
    file.close();
    CLOSE_SOCKET(sock);
}
