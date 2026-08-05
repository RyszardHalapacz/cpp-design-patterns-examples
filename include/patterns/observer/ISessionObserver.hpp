#pragma once
#include "SessionEvent.hpp"

namespace patterns::observer {

class ISessionObserver {
public:
    virtual ~ISessionObserver() = default;
    virtual void onSessionEvent(const SessionEvent& event) = 0;
};

} // namespace patterns::observer
