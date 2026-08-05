#pragma once
#include <string>
#include "IService.hpp"

namespace patterns::services {

class Logger : public IService {
public:
    void log(const std::string& message);
};

} // namespace patterns::services
