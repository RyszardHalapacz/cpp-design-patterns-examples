#pragma once
#include <string>
#include "IService.hpp"

namespace patterns::services {

class FileLogger : public IService {
public:
    explicit FileLogger(const std::string& filename);
    void log(const std::string& message);

private:
    std::string filename_;
};

} // namespace patterns::services
