#pragma once
#include "IService.hpp"

namespace patterns::services {

class DoSomething : public IService {
public:
    void do_();
};

} // namespace patterns::services
