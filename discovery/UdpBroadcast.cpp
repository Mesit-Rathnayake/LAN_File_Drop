#include <iostream>
#include <cstring>
#if defined(__linux__) || defined(__APPLE__)
#include <arpa/inet.h>
#include <unistd.h>
#define CLOSE_SOCKET close
#define SLEEP_MS(ms) usleep((ms) * 1000)
using socket_t = int;
#else
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#define CLOSE_SOCKET closesocket
#define SLEEP_MS(ms) Sleep(ms)
using socket_t = SOCKET;
#endif

void sendBroadcast(const std::string& deviceName, int port = 8888) {
    socket_t sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0) { perror("socket"); return; }

    int broadcastEnable = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

    sockaddr_in broadcastAddr{};
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(port);
    broadcastAddr.sin_addr.s_addr = inet_addr("255.255.255.255");

    std::string message = "HELLO FROM: " + deviceName;

    while(true) {
         sendto(sock, message.c_str(), message.size(), 0,
             (struct sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
        SLEEP_MS(5000); // broadcast every 5 seconds
    }

    CLOSE_SOCKET(sock);
}
