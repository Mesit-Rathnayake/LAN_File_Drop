#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include "discovery/UdpBroadcast.cpp"
#include "discovery/UdpListner.cpp"
#include "transfer/Sender.cpp"
#include "transfer/Receiver.cpp"

int main() {
#ifdef _WIN32
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif
    std::thread listener(listenBroadcast, 8888);
    std::thread broadcaster(sendBroadcast, std::string("Device_1"), 8888);

    listener.detach();
    broadcaster.detach();

    std::cout << "LAN File Drop running...\n";
    std::cout << "1. Send File\n2. Receive File\nChoice: ";
    int choice; std::cin >> choice;

    if(choice == 1) {
        std::string ip, file;
        std::cout << "Enter receiver IP: "; std::cin >> ip;
        std::cout << "Enter file path: "; std::cin >> file;
        sendFile(file, ip);
    } else if(choice == 2) {
        receiveFile();
    }

    while(true) { std::this_thread::sleep_for(std::chrono::seconds(1)); }
}
