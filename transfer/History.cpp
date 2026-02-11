#ifndef HISTORY_CPP
#define HISTORY_CPP

#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <ctime>
#include <filesystem>
#include <sstream>

class HistoryManager {
public:
    // Log a transfer: Filename, Action (Sent/Recv), Extension, Date, Time
    static void logTransfer(const std::string& filePath, const std::string& action) {
        std::ofstream file("transfer_history.csv", std::ios::app);
        if (!file.is_open()) return;

        std::filesystem::path p(filePath);
        std::string filename = p.filename().string();
        std::string extension = p.extension().string();
        if (extension.empty()) extension = "N/A";

        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm ltm;
#ifdef _WIN32
        localtime_s(&ltm, &now);
#else
        localtime_r(&now, &ltm);
#endif

        file << filename << "," 
             << action << "," 
             << extension << "," 
             << std::put_time(&ltm, "%Y-%m-%d") << "," 
             << std::put_time(&ltm, "%H:%M:%S") << "\n";
        
        file.close();
    }

    static void showHistory() {
        std::ifstream file("transfer_history.csv");
        if (!file.is_open()) {
            std::cout << "\n[!] No history records found.\n";
            return;
        }

        std::cout << "\n" << std::string(75, '=') << "\n";
        std::cout << std::left << std::setw(25) << "FILENAME" 
                  << std::setw(12) << "ACTION" 
                  << std::setw(10) << "TYPE" 
                  << std::setw(15) << "DATE" 
                  << "TIME" << "\n";
        std::cout << std::string(75, '-') << "\n";

        std::string line;
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string name, action, ext, date, time;
            
            std::getline(ss, name, ',');
            std::getline(ss, action, ',');
            std::getline(ss, ext, ',');
            std::getline(ss, date, ',');
            std::getline(ss, time, ',');

            std::cout << std::left << std::setw(25) << name
                      << std::setw(12) << action
                      << std::setw(10) << ext
                      << std::setw(15) << date
                      << time << "\n";
        }
        std::cout << std::string(75, '=') << "\n\n";
        file.close();
    }
};

#endif