#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
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

// Helper: Send a single file over an open socket
void sendSingleFile(socket_t sock, const std::string &filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Cannot open file: " << filename << "\n";
        return;
    }

    // Send filename length + filename
    std::string send_name = std::filesystem::path(filename).filename().string();
    size_t name_len = send_name.size();
    int sent_bytes = send(sock, reinterpret_cast<const char *>(&name_len), sizeof(name_len), 0);
    if (sent_bytes < 0)
    {
        std::cerr << "Error sending filename length\n";
        return;
    }

    sent_bytes = send(sock, send_name.c_str(), static_cast<int>(name_len), 0);
    if (sent_bytes < 0)
    {
        std::cerr << "Error sending filename\n";
        return;
    }

    // Send file size
    file.seekg(0, std::ios::end);
    size_t filesize = file.tellg();
    file.seekg(0, std::ios::beg);
    sent_bytes = send(sock, reinterpret_cast<const char *>(&filesize), sizeof(filesize), 0);
    if (sent_bytes < 0)
    {
        std::cerr << "Error sending file size\n";
        return;
    }

    // Send file data
    char buffer[4096];
    size_t sent = 0;
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0)
    {
        sent_bytes = send(sock, buffer, file.gcount(), 0);
        if (sent_bytes < 0)
        {
            std::cerr << "\nError sending file data (connection closed by receiver?)\n";
            return;
        }
        sent += sent_bytes;
        std::cout << "\rProgress: " << (sent * 100 / filesize) << "%";
        std::cout.flush();
    }
    std::cout << "\n";
    file.close();
}

// Single file send (backward compatible)
void sendFile(const std::string &filename, const std::string &ip, int port = 9999)
{
    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket");
        return;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("connect");
        return;
    }

    // Send number of files
    size_t num_files = 1;
    send(sock, reinterpret_cast<const char *>(&num_files), sizeof(num_files), 0);

    sendSingleFile(sock, filename);
    std::cout << "File sent successfully!\n";
    CLOSE_SOCKET(sock);
}

// Multiple files send (NEW - Queue support)
void sendMultipleFiles(const std::vector<std::string> &filePaths, const std::string &ip, int port = 9999)
{
    // Validate all files exist first
    for (const auto &filepath : filePaths)
    {
        if (!std::filesystem::exists(filepath))
        {
            std::cerr << "File not found: " << filepath << "\n";
            return;
        }
    }

    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket");
        return;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("connect");
        return;
    }

    // Send number of files first
    size_t num_files = filePaths.size();
    send(sock, reinterpret_cast<const char *>(&num_files), sizeof(num_files), 0);
    std::cout << "\nSending " << num_files << " file(s) sequentially...\n";

    // Send each file from the queue
    for (size_t i = 0; i < filePaths.size(); ++i)
    {
        std::cout << "\n[" << (i + 1) << "/" << num_files << "] ";
        std::cout << std::filesystem::path(filePaths[i]).filename().string() << "\n";
        sendSingleFile(sock, filePaths[i]);
    }

    std::cout << "\nAll files sent successfully!\n";
    CLOSE_SOCKET(sock);
}
