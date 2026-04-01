#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <thread>
#include <climits>

#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

// Including component files
// Note: Ensure these paths match your project structure
#include "discovery/UdpBroadcast.cpp"
#include "discovery/UdpListner.cpp"
#include "transfer/History.cpp"
#include "transfer/FileUtils.cpp"
#include "transfer/Sender.cpp"
#include "transfer/Receiver.cpp" 

int main()
{
#ifdef _WIN32
    // Initialize Winsock for Windows
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    // Start background discovery threads (UDP Broadcast/Listen)
    // Detaching allows them to run in the background while the user interacts with the menu
    std::thread listener(listenBroadcast, 8888);
    std::thread broadcaster(sendBroadcast, std::string("Device_1"), 8888);

    listener.detach();
    broadcaster.detach();

    while (true)
    {
        std::cout << "\n========== LAN File Drop ==========\n";
        std::cout << "1. Send File(s)\n";
        std::cout << "2. Send Folder\n";
        std::cout << "3. Receive File(s)\n";
        std::cout << "4. View Transfer History\n";
        std::cout << "5. Exit\n";
        std::cout << "Choice: ";
        
        int choice;
        if (!(std::cin >> choice)) {
            // Handle non-integer input to prevent infinite loops
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Invalid input. Please enter a number.\n";
            continue;
        }

        if (choice == 1)
{
    // Ask number of devices
    std::cout << "How many devices to send the file(s) to? ";
    int deviceCount = 0;

    if (!(std::cin >> deviceCount) || deviceCount <= 0)
    {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "Invalid device count.\n";
        continue;
    }

    std::vector<std::string> deviceIPs;
    deviceIPs.reserve(deviceCount);

    for (int d = 0; d < deviceCount; ++d)
    {
        std::string ip;
        std::cout << "Enter receiver IP for device " << (d + 1) << ": ";
        std::cin >> ip;
        deviceIPs.push_back(ip);
    }

    std::cout << "How many files to send? ";
    int fileCount;

    if (!(std::cin >> fileCount) || fileCount <= 0)
    {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "Invalid file count.\n";
        continue;
    }

    std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');

    std::vector<std::string> fileQueue;

    if (fileCount == 1)
    {
        std::string file;
        std::cout << "Enter file path: ";
        std::getline(std::cin, file);
        fileQueue.push_back(file);
    }
    else
    {
        std::cout << "Enter file paths (one per line):\n";
        for (int i = 0; i < fileCount; ++i)
        {
            std::string file;
            std::cout << "File " << (i + 1) << ": ";
            std::getline(std::cin, file);
            fileQueue.push_back(file);
        }
    }

    // Send to each device
    for (const auto& ip : deviceIPs)
    {
        std::cout << "Sending file(s) to " << ip << "...\n";

        if (fileQueue.size() == 1)
            sendFile(fileQueue[0], ip);
        else
            sendMultipleFiles(fileQueue, ip);
    }
}

        // if (choice == 1)
        // {
        //     std::string ip;
        //     std::cout << "Enter receiver IP: ";
        //     std::cin >> ip;

        //     std::cout << "How many files to send? ";
        //     int fileCount;
        //     if (!(std::cin >> fileCount)) {
        //         std::cin.clear();
        //         std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        //         std::cout << "Invalid count.\n";
        //         continue;
        //     }
        //     // Clear buffer before getline
        //     std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');

        //     if (fileCount == 1)
        //     {
        //         // Single file mode
        //         std::string file;
        //         std::cout << "Enter file path: ";
        //         std::getline(std::cin, file);
        //         sendFile(file, ip);
        //     }
        //     else if (fileCount > 1)
        //     {
        //         // Multiple files mode
        //         std::vector<std::string> fileQueue;
        //         std::cout << "Enter file paths (one per line):\n";
        //         for (int i = 0; i < fileCount; ++i)
        //         {
        //             std::string file;
        //             std::cout << "File " << (i + 1) << ": ";
        //             std::getline(std::cin, file);
        //             fileQueue.push_back(file);
        //         }
        //         sendMultipleFiles(fileQueue, ip);
        //     }
        //     else
        //     {
        //         std::cout << "Invalid file count!\n";
        //     }
        // }
        // else if (choice == 2)
        // {
        //     // Send Folder option
        //     std::string ip;
        //     std::cout << "Enter receiver IP: ";
        //     std::cin >> ip;
            
        //     // Clear buffer before getline
        //     std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
            
        //     std::string folderPath;
        //     std::cout << "Enter folder path: ";
        //     std::getline(std::cin, folderPath);
            
        //     sendFolder(folderPath, ip);
        // }

        else if (choice == 2)
{
    std::cout << "How many devices to send the folder to? ";
    int deviceCount = 0;

    if (!(std::cin >> deviceCount) || deviceCount <= 0)
    {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "Invalid device count.\n";
        continue;
    }

    std::vector<std::string> deviceIPs;
    deviceIPs.reserve(deviceCount);

    for (int d = 0; d < deviceCount; ++d)
    {
        std::string ip;
        std::cout << "Enter receiver IP for device " << (d + 1) << ": ";
        std::cin >> ip;
        deviceIPs.push_back(ip);
    }

    // Clear buffer before getline
    std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');

    std::string folderPath;
    std::cout << "Enter folder path: ";
    std::getline(std::cin, folderPath);

    // Send folder to each device
    for (const auto& ip : deviceIPs)
    {
        std::cout << "Sending folder to " << ip << "...\n";
        sendFolder(folderPath, ip);
    }
}
        else if (choice == 3)
        {
            // Clear buffer before getline
            std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
            std::cout << "Enter destination folder (leave empty for current folder): ";
            std::string dest;
            std::getline(std::cin, dest);
            
            if (dest.empty())
                dest = ".";
            
            // Starts TCP listening for incoming files
            receiveFile(9999, dest);
        }
        else if (choice == 4)
        {
            // Displays the transfer_history.csv in a formatted table
            HistoryManager::showHistory();
        }
        else if (choice == 5)
        {
            std::cout << "Exiting...\n";
            break;
        }
        else
        {
            std::cout << "Invalid selection. Please try again.\n";
        }
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}