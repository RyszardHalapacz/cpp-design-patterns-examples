#include "patterns/session/SessionEstablisher.hpp"
#include "patterns/session/SessionManagement.hpp"
#include "patterns/engine/Engine.hpp"
#include "patterns/services/ServiceLocator.hpp"

namespace patterns::session {

using patterns::services::logApp;

void SessionEstablisher::establish() {
    logApp("[SessionEstablisher] Starting session establishment\n");
    if (!checkPreconditions()) {
        logApp("[SessionEstablisher] Preconditions not met — aborting\n");
        return;
    }
    connect();
    configure();
    finalizeSetup();
    logApp("[SessionEstablisher] Session established\n");
}

EngineSessionEstablisher::EngineSessionEstablisher(SessionManagement& session,
                                                   patterns::engine::Engine& engine)
    : session_(session), engine_(engine) {}

bool EngineSessionEstablisher::checkPreconditions() {
    logApp("[EngineSessionEstablisher] Checking engine availability\n");
    return true;
}

void EngineSessionEstablisher::connect() {
    session_.connectToEngine(engine_);
}

void EngineSessionEstablisher::configure() {
    logApp("[EngineSessionEstablisher] Default configuration\n");
}

void EngineSessionEstablisher::finalizeSetup() {
    session_.openSession();
}

} // namespace patterns::session
