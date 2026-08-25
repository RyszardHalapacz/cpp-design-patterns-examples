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
    : session_(session), engine_(std::move(engine)), factory_(std::move(factory)), historian_(std::move(historian)) {}

void EngineSessionCoordinator::disableHistorian() {
    historian_.reset();  // shared_ptr drops to 0 — Engine's weak_ptr expires
    logApp("[EngineSessionCoordinator] Historian disabled\n");
}

void EngineSessionCoordinator::enableHistorian() {
    historian_ = std::make_shared<patterns::historian::EngineHistorian>();
    engine_->setHistorian(historian_);  // Engine gets a new valid weak_ptr
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
    engine_->setHistorian(historian_);
    return {};
}

std::expected<void, std::string> EngineSessionCoordinator::finalizeSetup() {
    session_.openSession();
    return {};
}

} // namespace patterns::session
