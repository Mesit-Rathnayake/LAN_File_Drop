#include <iostream>
#include <fstream>
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

void receiveFile(int port = 9999) {
    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) { perror("socket"); return; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return; }
    listen(sock, 1);

    std::cout << "Waiting for incoming file...\n";
    socket_t client = accept(sock, nullptr, nullptr);
    if(client < 0) { perror("accept"); return; }

    // Receive filename length + filename first
    size_t name_len = 0;
    recv(client, reinterpret_cast<char*>(&name_len), sizeof(name_len), 0);

    std::string filename(name_len, '\0');
    recv(client, &filename[0], static_cast<int>(name_len), 0);

    // Receive file size next
    size_t filesize;
    recv(client, reinterpret_cast<char*>(&filesize), sizeof(filesize), 0);

    std::ofstream file(filename, std::ios::binary);
    char buffer[4096];
    size_t received = 0;
    while(received < filesize) {
        int bytes = recv(client, buffer, sizeof(buffer), 0);
        file.write(buffer, bytes);
        received += bytes;
        std::cout << "\rProgress: " << (received * 100 / filesize) << "%";
        std::cout.flush();
    }

    std::cout << "\nFile received successfully!\n";
    file.close();
    CLOSE_SOCKET(client);
    CLOSE_SOCKET(sock);
}
