#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
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

// Helper: Send exactly N bytes (retry if needed)
bool sendAllBytes(socket_t sock, const char *data, size_t size)
{
    size_t sent = 0;
    while (sent < size)
    {
        int n = send(sock, data + sent, static_cast<int>(size - sent), 0);
        if (n <= 0)
            return false;
        sent += n;
    }
    return true;
}

// Helper: Send a single file to socket
bool sendSingleFile(socket_t sock, const std::string &filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Cannot open file: " << filename << "\n";
        return false;
    }

    std::string send_name = std::filesystem::path(filename).filename().string();
    size_t name_len = send_name.size();
    if (!sendAllBytes(sock, reinterpret_cast<const char *>(&name_len), sizeof(name_len)))
    {
        std::cerr << "Error sending filename length\n";
        return false;
    }

    if (!sendAllBytes(sock, send_name.c_str(), name_len))
    {
        std::cerr << "Error sending filename\n";
        return false;
    }

    file.seekg(0, std::ios::end);
    size_t filesize = file.tellg();
    file.seekg(0, std::ios::beg);
    if (!sendAllBytes(sock, reinterpret_cast<const char *>(&filesize), sizeof(filesize)))
    {
        std::cerr << "Error sending file size\n";
        return false;
    }

    char buffer[4096];
    size_t sent = 0;
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0)
    {
        int bytes_read = file.gcount();
        if (!sendAllBytes(sock, buffer, bytes_read))
        {
            std::cerr << "\nError sending file data at " << sent << "/" << filesize << " bytes\n";
            return false;
        }
        sent += bytes_read;
        std::cout << "\rProgress: " << (sent * 100 / filesize) << "%";
        std::cout.flush();
    }
    std::cout << "\n";
    file.close();
    return true;
}

// Single file send (backward compatible)
void sendFile(const std::string &filename, const std::string &ip, int port = 9999)
{
    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock <0)
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
    if (!sendAllBytes(sock, reinterpret_cast<const char *>(&num_files), sizeof(num_files)))
    {
        std::cerr << "Error sending file count\n";
        CLOSE_SOCKET(sock);
        return;
    }

    if (!sendSingleFile(sock, filename))
    {
        CLOSE_SOCKET(sock);
        return;
    }
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
    if (sock <0)
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
    if (!sendAllBytes(sock, reinterpret_cast<const char *>(&num_files), sizeof(num_files)))
    {
        std::cerr << "Error sending file count\n";
        CLOSE_SOCKET(sock);
        return;
    }
    std::cout << "\nSending " << num_files << " file(s) sequentially...\n";

    // Send each file from the queue
    for (size_t i = 0; i < filePaths.size(); ++i)
    {
        std::cout << "\n[" << (i + 1) << "/" << num_files << "] ";
        std::cout << std::filesystem::path(filePaths[i]).filename().string() << "\n";
        if (!sendSingleFile(sock, filePaths[i]))
        {
            CLOSE_SOCKET(sock);
            return;
        }
    }

    std::cout << "\nAll files sent successfully!\n";
    CLOSE_SOCKET(sock);
}
