#include "patterns/services/FileLogger.hpp"
#include <iostream>

namespace patterns::services {

FileLogger::FileLogger(const std::string& filename) : filename_(filename) {
    std::cout << "[FileLogger] Command: create file \""
              << filename_ << "\" (simulation — no actual disk write)\n";
}

void FileLogger::log(const std::string& message) {
    std::cout << "[FileLogger] Command: append to file \""
              << filename_ << "\": " << message;
}

} // namespace patterns::services
