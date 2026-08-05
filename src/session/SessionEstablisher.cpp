#include "patterns/session/SessionEstablisher.hpp"
#include "patterns/session/SessionManagement.hpp"
#include "patterns/engine/Engine.hpp"
#include "patterns/services/ServiceLocator.hpp"

namespace patterns::session {

using patterns::services::appLogger;

void SessionEstablisher::establish() {
    appLogger().log("[SessionEstablisher] Starting session establishment\n");
    if (!checkPreconditions()) {
        appLogger().log("[SessionEstablisher] Preconditions not met — aborting\n");
        return;
    }
    connect();
    configure();
    finalizeSetup();
    appLogger().log("[SessionEstablisher] Session established\n");
}

EngineSessionEstablisher::EngineSessionEstablisher(SessionManagement& session,
                                                   patterns::engine::Engine& engine)
    : session_(session), engine_(engine) {}

bool EngineSessionEstablisher::checkPreconditions() {
    appLogger().log("[EngineSessionEstablisher] Checking engine availability\n");
    return true;
}

void EngineSessionEstablisher::connect() {
    session_.connectToEngine(engine_);
}

void EngineSessionEstablisher::configure() {
    appLogger().log("[EngineSessionEstablisher] Default configuration\n");
}

void EngineSessionEstablisher::finalizeSetup() {
    session_.openSession();
}

} // namespace patterns::session
