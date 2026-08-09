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
using patterns::services::logApp;

void SessionManagement::connectToEngine(std::shared_ptr<patterns::engine::Engine> engine) {
    engine_ = engine;
    logApp("[Session] Connecting to Engine\n");
    logApp("[Session] Checking configuration\n");
    logApp("[Session] Engine connected\n");
    attach(engine);
}

void SessionManagement::openSession() {
    auto engine = engine_.lock();
    if (!engine) { logApp("[Session] No Engine\n"); return; }
    sessionActive_ = true;
    logApp("[Session] Opening session\n");
    engine->start();
}

void SessionManagement::closeSession() {
    if (engine_.expired()) { logApp("[Session] No Engine\n"); return; }
    logApp("[Session] Closing session — notifying all observers\n");
    sessionActive_ = false;
    notify(SessionEvent{SessionEventType::SessionClosing, {}, 0});
    observers_.clear();
}

void SessionManagement::addVectorFromGui(const std::vector<int>& vec) {
    if (!checkSession()) return;
    logApp("[Session] GUI wants to add vector\n");
    notify(SessionEvent{SessionEventType::VectorAdded, vec, 0});
}

void SessionManagement::sortVectorFromGui(size_t index) {
    if (!checkSession()) return;
    logApp("[Session] GUI wants to sort vector\n");
    notify(SessionEvent{SessionEventType::SortRequested, {}, index});
}

void SessionManagement::printDataFromGui() {
    if (!checkSession()) return;
    logApp("[Session] GUI wants to print data\n");
    notify(SessionEvent{SessionEventType::PrintRequested, {}, 0});
}

void SessionManagement::executeBatch(const CommandBatch& batch) {
    std::ostringstream header;
    header << "[Session] Received command batch (" << batch.size()
           << ") — executing in order\n";
    logApp(header.str());

    for (const auto& cmd : batch) {
        switch (cmd.type) {
            case CommandType::AddVector:  addVectorFromGui(cmd.vectorData); break;
            case CommandType::SortVector: sortVectorFromGui(cmd.index);      break;
            case CommandType::PrintData:  printDataFromGui();                break;
        }
    }
    logApp("[Session] Command batch executed\n");
}

void SessionManagement::setSortStrategyFromGui(SortStrategyId id) {
    if (engine_.expired()) { logApp("[Session] No Engine\n"); return; }
    if (!isStrategyAllowed(id)) {
        std::ostringstream oss;
        oss << "[Session] Strategy \"" << sortStrategyIdName(id)
            << "\" not allowed — Configurator did not authorize it\n";
        logApp(oss.str());
        return;
    }
    logApp("[Session] GUI requests sort strategy change\n");
    notify(SessionEvent{SessionEventType::StrategyChangeRequested, {}, 0, id});
}

void SessionManagement::setAllowedStrategies(std::vector<SortStrategyId> allowed) {
    allowedStrategies_ = std::move(allowed);
}

void SessionManagement::attach(std::shared_ptr<patterns::observer::ISessionObserver> observer) {
    std::erase_if(observers_, [](const auto& wp) { return wp.expired(); });
    for (const auto& wp : observers_) {
        if (wp.lock() == observer) {
            logApp("[Session] Observer already attached — skipping\n");
            return;
        }
    }
    observers_.push_back(observer);
}

void SessionManagement::detach(std::shared_ptr<patterns::observer::ISessionObserver> observer) {
    std::erase_if(observers_, [&observer](const auto& wp) {
        auto existing = wp.lock();
        return !existing || existing == observer;
    });
}

void SessionManagement::notify(const SessionEvent& event) {
    std::erase_if(observers_, [](const auto& wp) { return wp.expired(); });
    for (const auto& wp : observers_)
        if (auto obs = wp.lock()) obs->onSessionEvent(event);
}

bool SessionManagement::isStrategyAllowed(SortStrategyId id) const {
    return std::find(allowedStrategies_.begin(), allowedStrategies_.end(), id)
           != allowedStrategies_.end();
}

bool SessionManagement::checkSession() const {
    if (engine_.expired()) { logApp("[Session] No Engine\n");       return false; }
    if (!sessionActive_)   { logApp("[Session] Session inactive\n"); return false; }
    return true;
}

} // namespace patterns::session
