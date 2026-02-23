#ifndef FILEUTILS_CPP
#define FILEUTILS_CPP

#include <filesystem>
#include <vector>
#include <string>
#include <iostream>

namespace FileUtils {
    // Recursively scan a directory and collect all file paths
    std::vector<std::string> scanDirectory(const std::string& dirPath) {
        std::vector<std::string> files;
        
        try {
            if (!std::filesystem::exists(dirPath)) {
                std::cerr << "Path does not exist: " << dirPath << "\n";
                return files;
            }

            if (!std::filesystem::is_directory(dirPath)) {
                std::cerr << "Path is not a directory: " << dirPath << "\n";
                return files;
            }

            // Recursively iterate through directory
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath)) {
                if (entry.is_regular_file()) {
                    files.push_back(entry.path().string());
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Error scanning directory: " << e.what() << "\n";
        }

        return files;
    }

    // Get human-readable file size
    std::string formatFileSize(uint64_t bytes) {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int unitIndex = 0;
        double size = static_cast<double>(bytes);

        while (size >= 1024.0 && unitIndex < 4) {
            size /= 1024.0;
            unitIndex++;
        }

        char buffer[32];
        if (unitIndex == 0) {
            snprintf(buffer, sizeof(buffer), "%llu %s", static_cast<unsigned long long>(bytes), units[unitIndex]);
        } else {
            snprintf(buffer, sizeof(buffer), "%.1f %s", size, units[unitIndex]);
        }
        return std::string(buffer);
    }

    // Calculate total size of multiple files
    uint64_t getTotalSize(const std::vector<std::string>& filePaths) {
        uint64_t total = 0;
        for (const auto& path : filePaths) {
            try {
                if (std::filesystem::exists(path)) {
                    total += std::filesystem::file_size(path);
                }
            } catch (...) {
                // Skip files that can't be accessed
            }
        }
        return total;
    }

    // Get relative path from base folder
    std::string getRelativePath(const std::string& fullPath, const std::string& basePath) {
        try {
            std::filesystem::path full = std::filesystem::absolute(fullPath);
            std::filesystem::path base = std::filesystem::absolute(basePath);
            return std::filesystem::relative(full, base).string();
        } catch (...) {
            // Fallback to filename if relative path calculation fails
            return std::filesystem::path(fullPath).filename().string();
        }
    }
}

#endif
