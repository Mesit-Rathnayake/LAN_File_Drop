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

// Helper: Receive exactly N bytes (retry if needed)
bool recvAllBytes(socket_t sock, char *data, size_t size)
{
    size_t received = 0;
    while (received < size)
    {
        int n = recv(sock, data + received, static_cast<int>(size - received), 0);
        if (n <= 0)
            return false;
        received += n;
    }
    return true;
}

// Helper: Receive a single file from socket
void receiveSingleFile(socket_t client, const std::string &destDir, size_t fileIndex, size_t totalFiles)
{
    // Receive filename length + filename
    size_t name_len = 0;
    if (!recvAllBytes(client, reinterpret_cast<char *>(&name_len), sizeof(name_len)))
    {
        std::cerr << "Error receiving filename length\n";
        return;
    }

    std::string filename(name_len, '\0');
    if (!recvAllBytes(client, &filename[0], name_len))
    {
        std::cerr << "Error receiving filename\n";
        return;
    }

    std::cout << "[" << (fileIndex + 1) << "/" << totalFiles << "] Receiving: " << filename << "\n";

    // Receive file size
    size_t filesize;
    if (!recvAllBytes(client, reinterpret_cast<char *>(&filesize), sizeof(filesize)))
    {
        std::cerr << "Error receiving file size\n";
        return;
    }

    // Save file to destination directory
    try
    {
        std::filesystem::path dir(destDir);
        if (!dir.empty())
            std::filesystem::create_directories(dir);
        std::filesystem::path outPath = std::filesystem::path(destDir) / filename;
        std::ofstream file(outPath.string(), std::ios::binary);

        char buffer[4096];
        size_t received = 0;
        while (received < filesize)
        {
            int bytes_to_read = (filesize - received < sizeof(buffer)) ? (filesize - received) : sizeof(buffer);
            int recv_bytes = recv(client, buffer, bytes_to_read, 0);
            if (recv_bytes <= 0)
            {
                std::cerr << "\nError: Connection closed prematurely at " << received << "/" << filesize << " bytes\n";
                break;
            }
            file.write(buffer, recv_bytes);
            received += recv_bytes;
            std::cout << "\rProgress: " << (received * 100 / filesize) << "%";
            std::cout.flush();
        }

        std::cout << "\n";
        file.close();
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error saving file: " << ex.what() << "\n";
    }
}

void receiveFile(int port = 9999, const std::string &destDir = ".")
{
    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock <0)
    {
        perror("socket");
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return;
    }
    listen(sock, 1);

    std::cout << "Waiting for incoming file(s)...\n";
    socket_t client = accept(sock, nullptr, nullptr);
    if (client <0)
    {
        perror("accept");
        return;
    }

    // Receive number of files first
    size_t num_files = 0;
    recv(client, reinterpret_cast<char *>(&num_files), sizeof(num_files), 0);
    std::cout << "Receiving " << num_files << " file(s)...\n";

    // Receive each file from the queue
    for (size_t i = 0; i < num_files; ++i)
    {
        receiveSingleFile(client, destDir, i, num_files);
    }

    std::cout << "\nAll files received successfully!\n";
    CLOSE_SOCKET(client);
    CLOSE_SOCKET(sock);
}
