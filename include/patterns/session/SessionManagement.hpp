#pragma once
#include <memory>
#include <vector>
#include "patterns/observer/ISessionObserver.hpp"
#include "patterns/strategy/SortStrategyId.hpp"
#include "patterns/gui/Command.hpp"

#include "patterns/engine/Engine.hpp"

namespace patterns::session {

// ==================================
// SESSION MANAGEMENT — Facade + Subject (Observer)
// Hides the complexity of Engine connection, broadcasts events to observers.
// ==================================
class SessionManagement {
public:
    void connectToEngine(std::shared_ptr<patterns::engine::Engine> engine);
    void openSession();
    void closeSession();

    void addVectorFromGui(const std::vector<int>& vec);
    void sortVectorFromGui(size_t index);
    void printDataFromGui();
    void executeBatch(const patterns::gui::CommandBatch& batch);
    void setSortStrategyFromGui(patterns::strategy::SortStrategyId id);
    void setAllowedStrategies(std::vector<patterns::strategy::SortStrategyId> allowed);

    void attach(std::shared_ptr<patterns::observer::ISessionObserver> observer);
    void detach(std::shared_ptr<patterns::observer::ISessionObserver> observer);

private:
    void notify(const patterns::observer::SessionEvent& event);
    bool isStrategyAllowed(patterns::strategy::SortStrategyId id) const;
    bool checkSession() const;

    std::weak_ptr<patterns::engine::Engine>                          engine_;
    bool                                                             sessionActive_ = false;
    std::vector<std::weak_ptr<patterns::observer::ISessionObserver>> observers_;
    std::vector<patterns::strategy::SortStrategyId>    allowedStrategies_;
};

} // namespace patterns::session
