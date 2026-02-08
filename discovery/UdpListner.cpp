#include <iostream>
#include <cstring>
#if defined(__linux__) || defined(__APPLE__)
#include <arpa/inet.h>
#include <unistd.h>
#define CLOSE_SOCKET close
using socket_t = int;
using socket_len_t = socklen_t;
#else
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#define CLOSE_SOCKET closesocket
using socket_t = SOCKET;
using socket_len_t = int;
#endif

void listenBroadcast(int port = 8888) {
    socket_t sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0) { perror("socket"); return; }

    sockaddr_in recvAddr{};
    recvAddr.sin_family = AF_INET;
    recvAddr.sin_port = htons(port);
    recvAddr.sin_addr.s_addr = INADDR_ANY;

    if(bind(sock, (struct sockaddr*)&recvAddr, sizeof(recvAddr)) < 0) {
        perror("bind"); return;
    }

    char buffer[1024];
    sockaddr_in senderAddr;
    socket_len_t senderLen = sizeof(senderAddr);

    while(true) {
        int bytes = recvfrom(sock, buffer, sizeof(buffer)-1, 0,
                             (struct sockaddr*)&senderAddr, &senderLen);
        if(bytes > 0) {
            buffer[bytes] = '\0';
        }
    }

    CLOSE_SOCKET(sock);
}
