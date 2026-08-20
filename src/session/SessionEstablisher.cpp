#include "patterns/session/SessionEstablisher.hpp"
#include "patterns/session/SessionManagement.hpp"
#include "patterns/engine/Engine.hpp"
#include "patterns/services/ServiceLocator.hpp"

namespace patterns::session {

using patterns::services::logApp;

std::expected<void, std::string> SessionEstablisher::establish() {
    logApp("[SessionEstablisher] Starting session establishment\n");
    return checkPreconditions()
        .and_then([this] { return connect(); })
        .and_then([this] { return configure(); })
        .and_then([this] { return finalizeSetup(); })
        .transform([](){ logApp("[SessionEstablisher] Session established\n"); })
        .or_else([](const std::string& e) -> std::expected<void, std::string> {
            logApp("[SessionEstablisher] " + e + " — aborting\n");
            return std::unexpected(e);
        });
}

EngineSessionEstablisher::EngineSessionEstablisher(SessionManagement& session,
                                                   std::shared_ptr<patterns::engine::Engine> engine)
    : session_(session), engine_(std::move(engine)) {}

std::expected<void, std::string> EngineSessionEstablisher::checkPreconditions() {
    logApp("[EngineSessionEstablisher] Checking engine availability\n");
    if (!engine_)
        return std::unexpected(std::string("Engine not available"));
    return {};
}

std::expected<void, std::string> EngineSessionEstablisher::connect() {
    session_.connectToEngine(engine_);
    return {};
}

std::expected<void, std::string> EngineSessionEstablisher::configure() {
    logApp("[EngineSessionEstablisher] Default configuration\n");
    return {};
}

std::expected<void, std::string> EngineSessionEstablisher::finalizeSetup() {
    session_.openSession();
    return {};
}

} // namespace patterns::session
