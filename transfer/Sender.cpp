#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint> // Added for fixed-width integers

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
// Updated to accept receiverIP for history logging and optional relativePath for folder structure
bool sendSingleFile(socket_t sock, const std::string &filename, const std::string& remoteIP, const std::string& relativePath = "")
{
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Cannot open file: " << filename << "\n";
        return false;
    }

    // Use relative path if provided (for folder transfers), otherwise just filename
    std::string send_name = relativePath.empty() 
        ? std::filesystem::path(filename).filename().string()
        : relativePath;
    
    // Use uint64_t to ensure cross-platform size consistency
    uint64_t name_len = static_cast<uint64_t>(send_name.size());

    // 1. Send filename length
    if (!sendAllBytes(sock, reinterpret_cast<const char *>(&name_len), sizeof(name_len)))
    {
        std::cerr << "Error sending filename length\n";
        return false;
    }

    // 2. Send filename string
    if (!sendAllBytes(sock, send_name.c_str(), static_cast<size_t>(name_len)))
    {
        std::cerr << "Error sending filename\n";
        return false;
    }

    // 3. Get and send file size
    file.seekg(0, std::ios::end);
    uint64_t filesize = static_cast<uint64_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    
    if (!sendAllBytes(sock, reinterpret_cast<const char *>(&filesize), sizeof(filesize)))
    {
        std::cerr << "Error sending file size\n";
        return false;
    }

    // 4. Send file content in chunks
    char buffer[4096];
    uint64_t sent_total = 0;
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0)
    {
        int bytes_read = static_cast<int>(file.gcount());
        if (!sendAllBytes(sock, buffer, bytes_read))
        {
            std::cerr << "\nError sending file data at " << sent_total << "/" << filesize << " bytes\n";
            return false;
        }
        sent_total += bytes_read;
        std::cout << "\rProgress: " << (sent_total * 100 / filesize) << "%";
        std::cout.flush();
    }
    std::cout << "\n";
    file.close();

    // --- HISTORY IMPLEMENTATION ---
    // Log the successful transfer with the Remote IP
    HistoryManager::logTransfer(filename, "Sent", remoteIP);
    // ------------------------------

    return true;
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
        CLOSE_SOCKET(sock);
        return;
    }

    // Send number of files using uint64_t
    uint64_t num_files = 1;
    if (!sendAllBytes(sock, reinterpret_cast<const char *>(&num_files), sizeof(num_files)))
    {
        std::cerr << "Error sending file count\n";
        CLOSE_SOCKET(sock);
        return;
    }

    if (sendSingleFile(sock, filename, ip))
    {
        std::cout << "File sent successfully!\n";
    }

    CLOSE_SOCKET(sock);
}

// Helper function to send multiple files with optional base path for folder structure preservation
void sendMultipleFilesWithBasePath(const std::vector<std::string> &filePaths, const std::string &ip, const std::string &basePath = "", int port = 9999)
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
        CLOSE_SOCKET(sock);
        return;
    }

    // Send total number of files in the queue as uint64_t
    uint64_t num_files = static_cast<uint64_t>(filePaths.size());
    if (!sendAllBytes(sock, reinterpret_cast<const char *>(&num_files), sizeof(num_files)))
    {
        std::cerr << "Error sending file count\n";
        CLOSE_SOCKET(sock);
        return;
    }
    std::cout << "\nSending " << num_files << " file(s) sequentially...\n";

    // Loop through and send each file
    for (size_t i = 0; i < filePaths.size(); ++i)
    {
        std::string relativePath = basePath.empty() 
            ? "" 
            : FileUtils::getRelativePath(filePaths[i], basePath);
        
        std::cout << "\n[" << (i + 1) << "/" << num_files << "] ";
        std::cout << (relativePath.empty() ? std::filesystem::path(filePaths[i]).filename().string() : relativePath) << "\n";
        
        if (!sendSingleFile(sock, filePaths[i], ip, relativePath))
        {
            std::cerr << "Transfer interrupted.\n";
            break;
        }
    }

    std::cout << "\nAll files processed.\n";
    CLOSE_SOCKET(sock);
}

// Multiple files send (Queue support) - wrapper for backward compatibility
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
        CLOSE_SOCKET(sock);
        return;
    }

    // Send total number of files in the queue as uint64_t
    uint64_t num_files = static_cast<uint64_t>(filePaths.size());
    if (!sendAllBytes(sock, reinterpret_cast<const char *>(&num_files), sizeof(num_files)))
    {
        std::cerr << "Error sending file count\n";
        CLOSE_SOCKET(sock);
        return;
    }
    std::cout << "\nSending " << num_files << " file(s) sequentially...\n";

    // Loop through and send each file
    for (size_t i = 0; i < filePaths.size(); ++i)
    {
        std::cout << "\n[" << (i + 1) << "/" << num_files << "] ";
        std::cout << std::filesystem::path(filePaths[i]).filename().string() << "\n";
        if (!sendSingleFile(sock, filePaths[i], ip))
        {
            std::cerr << "Transfer interrupted.\n";
            break;
        }
    }

    std::cout << "\nAll files processed.\n";
    CLOSE_SOCKET(sock);
}

// Send entire folder (recursively scan and send all files)
void sendFolder(const std::string &folderPath, const std::string &ip, int port = 9999)
{
    // Scan the folder recursively
    std::cout << "Scanning folder: " << folderPath << "\n";
    std::vector<std::string> files = FileUtils::scanDirectory(folderPath);
    
    if (files.empty()) {
        std::cerr << "No files found in folder or folder is inaccessible.\n";
        return;
    }

    // Calculate and display total size
    uint64_t totalSize = FileUtils::getTotalSize(files);
    std::cout << "Found " << files.size() << " file(s) | Total size: " 
              << FileUtils::formatFileSize(totalSize) << "\n";
    
    // Confirm before sending
    std::cout << "Proceed with transfer? (y/n): ";
    std::string confirm;
    std::getline(std::cin, confirm);
    
    if (confirm != "y" && confirm != "Y") {
        std::cout << "Transfer cancelled.\n";
        return;
    }

    // Use sendMultipleFiles to send all files in the folder
    // Pass the parent directory as base to include folder name in relative path
    std::filesystem::path folderPathObj(folderPath);
    std::string basePath = folderPathObj.parent_path().string();
    sendMultipleFilesWithBasePath(files, ip, basePath, port);
}