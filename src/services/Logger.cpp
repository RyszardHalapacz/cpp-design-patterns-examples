#include "patterns/services/Logger.hpp"
#include <iostream>

namespace patterns::services {

void Logger::log(const std::string& message) {
    std::cout << message;
}

} // namespace patterns::services
