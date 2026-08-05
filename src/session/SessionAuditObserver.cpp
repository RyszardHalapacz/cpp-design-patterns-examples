#include "patterns/session/SessionAuditObserver.hpp"
#include "patterns/services/ServiceLocator.hpp"
#include "patterns/strategy/SortStrategyId.hpp"
#include <sstream>

namespace patterns::session {

using patterns::observer::SessionEvent;
using patterns::observer::SessionEventType;
using patterns::strategy::sortStrategyIdName;
using patterns::services::appLogger;

SessionAuditObserver::SessionAuditObserver() {
    appLogger().log("[Audit] Creating audit observer — starting to listen\n");
}

SessionAuditObserver::~SessionAuditObserver() {
    appLogger().log("[Audit] Destructor — releasing audit observer resources\n");
}

void SessionAuditObserver::onSessionEvent(const SessionEvent& event) {
    switch (event.type) {
        case SessionEventType::VectorAdded:
            appLogger().log("[Audit] Recorded: vector added\n");
            break;

        case SessionEventType::SortRequested:
            appLogger().log("[Audit] Recorded: sort requested\n");
            break;

        case SessionEventType::PrintRequested:
            appLogger().log("[Audit] Recorded: print requested\n");
            break;

        case SessionEventType::StrategyChangeRequested: {
            std::ostringstream oss;
            oss << "[Audit] Recorded: strategy change to "
                << sortStrategyIdName(event.strategyId) << "\n";
            appLogger().log(oss.str());
            break;
        }

        case SessionEventType::SessionClosing:
            appLogger().log("[Audit] Session closing — terminating\n");
            delete this;  // lifetime tied to the session
            break;
    }
}

} // namespace patterns::session
