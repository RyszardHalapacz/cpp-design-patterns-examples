#include "patterns/session/SessionAuditObserver.hpp"
#include "patterns/services/ServiceLocator.hpp"
#include "patterns/strategy/SortStrategyId.hpp"
#include <sstream>

namespace patterns::session {

using patterns::observer::SessionEvent;
using patterns::observer::SessionEventType;
using patterns::strategy::sortStrategyIdName;
using patterns::services::logApp;
using patterns::services::logFile;

SessionAuditObserver::SessionAuditObserver() {
    logApp("[Audit] Creating audit observer — starting to listen\n");
}

SessionAuditObserver::~SessionAuditObserver() {
    logApp("[Audit] Destructor — releasing audit observer resources\n");
}

void SessionAuditObserver::onSessionEvent(const SessionEvent& event) {
    switch (event.type) {
        case SessionEventType::VectorAdded:
            logApp("[Audit] Recorded: vector added\n");
            logFile("[Audit] Recorded: vector added\n");
            break;

        case SessionEventType::SortRequested:
            logApp("[Audit] Recorded: sort requested\n");
            logFile("[Audit] Recorded: sort requested\n");
            break;

        case SessionEventType::PrintRequested:
            logApp("[Audit] Recorded: print requested\n");
            logFile("[Audit] Recorded: print requested\n");
            break;

        case SessionEventType::StrategyChangeRequested: {
            std::ostringstream oss;
            oss << "[Audit] Recorded: strategy change to "
                << sortStrategyIdName(event.strategyId) << "\n";
            logApp(oss.str());
            logFile(oss.str());
            break;
        }

        case SessionEventType::SessionClosing:
            logApp("[Audit] Session closing — terminating\n");
            logFile("[Audit] Session closing — terminating\n");
            break;
    }
}

} // namespace patterns::session
