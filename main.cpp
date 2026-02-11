/*#include <chrono>
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
#include "discovery/UdpBroadcast.cpp"
#include "discovery/UdpListner.cpp"
#include "transfer/Sender.cpp"
#include "transfer/Receiver.cpp"

int main()
{
#ifdef _WIN32
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif
    std::thread listener(listenBroadcast, 8888);
    std::thread broadcaster(sendBroadcast, std::string("Device_1"), 8888);

    listener.detach();
    broadcaster.detach();

    std::cout << "========== LAN File Drop ==========\n";
    std::cout << "1. Send File(s)\n2. Receive File(s)\nChoice: ";
    int choice;
    std::cin >> choice;

    if (choice == 1)
    {
        std::string ip;
        std::cout << "Enter receiver IP: ";
        std::cin >> ip;

        std::cout << "How many files to send? ";
        int fileCount;
        std::cin >> fileCount;
        std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');

        if (fileCount == 1)
        {
            // Single file mode
            std::string file;
            std::cout << "Enter file path: ";
            std::getline(std::cin, file);
            sendFile(file, ip);
        }
        else if (fileCount > 1)
        {
            // Multiple files mode - queue them in a vector
            std::vector<std::string> fileQueue;
            std::cout << "Enter file paths (one per line):\n";
            for (int i = 0; i < fileCount; ++i)
            {
                std::string file;
                std::cout << "File " << (i + 1) << ": ";
                std::getline(std::cin, file);
                fileQueue.push_back(file);
            }
            // Send all files sequentially from the queue
            sendMultipleFiles(fileQueue, ip);
        }
        else
        {
            std::cout << "Invalid input!\n";
        }
    }
    else if (choice == 2)
    {
        std::cout << "Enter destination folder (leave empty for current folder): ";
        std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
        std::string dest;
        std::getline(std::cin, dest);
        if (dest.empty())
            dest = ".";
        receiveFile(9999, dest);
    }

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
*/
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
#include "transfer/Sender.cpp"
#include "transfer/Receiver.cpp"
#include "transfer/History.cpp" 

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
        std::cout << "2. Receive File(s)\n";
        std::cout << "3. View Transfer History\n";
        std::cout << "4. Exit\n";
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
            std::string ip;
            std::cout << "Enter receiver IP: ";
            std::cin >> ip;

            std::cout << "How many files to send? ";
            int fileCount;
            if (!(std::cin >> fileCount)) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Invalid count.\n";
                continue;
            }
            // Clear buffer before getline
            std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');

            if (fileCount == 1)
            {
                // Single file mode
                std::string file;
                std::cout << "Enter file path: ";
                std::getline(std::cin, file);
                sendFile(file, ip);
            }
            else if (fileCount > 1)
            {
                // Multiple files mode
                std::vector<std::string> fileQueue;
                std::cout << "Enter file paths (one per line):\n";
                for (int i = 0; i < fileCount; ++i)
                {
                    std::string file;
                    std::cout << "File " << (i + 1) << ": ";
                    std::getline(std::cin, file);
                    fileQueue.push_back(file);
                }
                sendMultipleFiles(fileQueue, ip);
            }
            else
            {
                std::cout << "Invalid file count!\n";
            }
        }
        else if (choice == 2)
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
        else if (choice == 3)
        {
            // Displays the transfer_history.csv in a formatted table
            HistoryManager::showHistory();
        }
        else if (choice == 4)
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