#include "patterns/session/SessionCoordinator.hpp"
#include "patterns/session/SessionManagement.hpp"
#include "patterns/engine/Engine.hpp"
#include "patterns/services/ServiceLocator.hpp"

namespace patterns::session {

using patterns::services::logApp;

std::expected<void, std::string> SessionCoordinator::establish() {
    logApp("[SessionCoordinator] Starting session establishment\n");
    return checkPreconditions()
        .and_then([this] { return connect(); })
        .and_then([this] { return configure(); })
        .and_then([this] { return finalizeSetup(); })
        .transform([](){ logApp("[SessionCoordinator] Session established\n"); })
        .or_else([](const std::string& e) -> std::expected<void, std::string> {
            logApp("[SessionCoordinator] " + e + " — aborting\n");
            return std::unexpected(e);
        });
}

EngineSessionCoordinator::EngineSessionCoordinator(SessionManagement& session,
                                                   std::shared_ptr<patterns::engine::Engine> engine,
                                                   std::shared_ptr<patterns::strategy::ISortStrategyFactory> factory,
                                                   std::shared_ptr<patterns::historian::EngineHistorian> historian)
    : session_(session), engine_(std::move(engine)), factory_(std::move(factory)), historian_(historian) {}

void EngineSessionCoordinator::disableHistorian() {
    engine_->clearHistorian();  // Engine drops its weak_ptr; historian still alive in Application
    logApp("[EngineSessionCoordinator] Historian disabled\n");
}

void EngineSessionCoordinator::enableHistorian() {
    if (auto historian = historian_.lock())
        engine_->setHistorian(historian);  // re-wire existing historian; no new allocation
    logApp("[EngineSessionCoordinator] Historian enabled\n");
}

std::expected<void, std::string> EngineSessionCoordinator::checkPreconditions() {
    logApp("[EngineSessionCoordinator] Checking engine availability\n");
    if (!engine_)
        return std::unexpected(std::string("Engine not available"));
    return {};
}

std::expected<void, std::string> EngineSessionCoordinator::connect() {
    session_.connectToEngine(engine_);
    return {};
}

std::expected<void, std::string> EngineSessionCoordinator::configure() {
    logApp("[EngineSessionCoordinator] Wiring factory and historian to engine\n");
    engine_->setFactory(factory_);
    if (auto historian = historian_.lock())
        engine_->setHistorian(historian);
    return {};
}

std::expected<void, std::string> EngineSessionCoordinator::finalizeSetup() {
    session_.openSession();
    return {};
}

} // namespace patterns::session
