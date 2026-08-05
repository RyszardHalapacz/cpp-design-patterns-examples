#include "patterns/session/SessionManagement.hpp"
#include "patterns/engine/Engine.hpp"
#include "patterns/services/ServiceLocator.hpp"
#include "patterns/strategy/SortStrategyId.hpp"
#include <algorithm>
#include <sstream>

namespace patterns::session {

using patterns::observer::SessionEvent;
using patterns::observer::SessionEventType;
using patterns::strategy::SortStrategyId;
using patterns::strategy::sortStrategyIdName;
using patterns::gui::CommandBatch;
using patterns::gui::CommandType;
using patterns::services::appLogger;

void SessionManagement::connectToEngine(patterns::engine::Engine& engine) {
    engine_ = &engine;
    appLogger().log("[Session] Connecting to Engine\n");
    appLogger().log("[Session] Checking configuration\n");
    appLogger().log("[Session] Engine connected\n");
    attach(&engine);
}

void SessionManagement::openSession() {
    if (!engine_) { appLogger().log("[Session] No Engine\n"); return; }
    sessionActive_ = true;
    appLogger().log("[Session] Opening session\n");
    engine_->start();
}

void SessionManagement::closeSession() {
    if (!engine_) { appLogger().log("[Session] No Engine\n"); return; }
    appLogger().log("[Session] Closing session — notifying all observers\n");
    sessionActive_ = false;
    notify(SessionEvent{SessionEventType::SessionClosing, {}, 0});
    observers_.clear();
}

void SessionManagement::addVectorFromGui(const std::vector<int>& vec) {
    if (!checkSession()) return;
    appLogger().log("[Session] GUI wants to add vector\n");
    notify(SessionEvent{SessionEventType::VectorAdded, vec, 0});
}

void SessionManagement::sortVectorFromGui(size_t index) {
    if (!checkSession()) return;
    appLogger().log("[Session] GUI wants to sort vector\n");
    notify(SessionEvent{SessionEventType::SortRequested, {}, index});
}

void SessionManagement::printDataFromGui() {
    if (!checkSession()) return;
    appLogger().log("[Session] GUI wants to print data\n");
    notify(SessionEvent{SessionEventType::PrintRequested, {}, 0});
}

void SessionManagement::executeBatch(const CommandBatch& batch) {
    std::ostringstream header;
    header << "[Session] Received command batch (" << batch.size()
           << ") — executing in order\n";
    appLogger().log(header.str());

    for (const auto& cmd : batch) {
        switch (cmd.type) {
            case CommandType::AddVector:  addVectorFromGui(cmd.vectorData); break;
            case CommandType::SortVector: sortVectorFromGui(cmd.index);      break;
            case CommandType::PrintData:  printDataFromGui();                break;
        }
    }
    appLogger().log("[Session] Command batch executed\n");
}

void SessionManagement::setSortStrategyFromGui(SortStrategyId id) {
    if (!engine_) { appLogger().log("[Session] No Engine\n"); return; }
    if (!isStrategyAllowed(id)) {
        std::ostringstream oss;
        oss << "[Session] Strategy \"" << sortStrategyIdName(id)
            << "\" not allowed — Configurator did not authorize it\n";
        appLogger().log(oss.str());
        return;
    }
    appLogger().log("[Session] GUI requests sort strategy change\n");
    notify(SessionEvent{SessionEventType::StrategyChangeRequested, {}, 0, id});
}

void SessionManagement::setAllowedStrategies(std::vector<SortStrategyId> allowed) {
    allowedStrategies_ = std::move(allowed);
}

void SessionManagement::attach(patterns::observer::ISessionObserver* observer) {
    observers_.push_back(observer);
}

void SessionManagement::detach(patterns::observer::ISessionObserver* observer) {
    observers_.erase(
        std::remove(observers_.begin(), observers_.end(), observer),
        observers_.end());
}

void SessionManagement::notify(const SessionEvent& event) {
    for (auto* obs : observers_) obs->onSessionEvent(event);
}

bool SessionManagement::isStrategyAllowed(SortStrategyId id) const {
    return std::find(allowedStrategies_.begin(), allowedStrategies_.end(), id)
           != allowedStrategies_.end();
}

bool SessionManagement::checkSession() const {
    if (!engine_)        { appLogger().log("[Session] No Engine\n");       return false; }
    if (!sessionActive_) { appLogger().log("[Session] Session inactive\n"); return false; }
    return true;
}

} // namespace patterns::session
