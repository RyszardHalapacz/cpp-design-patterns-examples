#include "patterns/engine/Engine.hpp"
#include "patterns/strategy/SortStrategyFactory.hpp"
#include "patterns/services/ServiceLocator.hpp"
#include <sstream>

namespace patterns::engine {

using patterns::observer::SessionEvent;
using patterns::observer::SessionEventType;
using patterns::strategy::SortStrategyFactory;
using patterns::strategy::SortStrategyId;
using patterns::services::appLogger;

Engine::Engine()
    : sortStrategy_(SortStrategyFactory::create(SortStrategyId::Ascending)) {}

void Engine::start() {
    running_ = true;
    appLogger().log("[Engine] Start\n");
}

void Engine::stop() {
    running_ = false;
    appLogger().log("[Engine] Stop\n");
}

void Engine::addVector(const std::vector<int>& vec) {
    data_.push_back(vec);
}

void Engine::setSortStrategy(std::unique_ptr<patterns::strategy::ISortStrategy> strategy) {
    sortStrategy_ = std::move(strategy);
    std::ostringstream oss;
    oss << "[Engine] Sort strategy set: " << sortStrategy_->name() << "\n";
    appLogger().log(oss.str());
}

void Engine::sortVector(size_t index) {
    if (index >= data_.size()) {
        appLogger().log("[Engine] Invalid vector index\n");
        return;
    }
    std::ostringstream oss;
    oss << "[Engine] Sorting with strategy: " << sortStrategy_->name() << "\n";
    appLogger().log(oss.str());
    (*sortStrategy_)(data_[index]);
}

void Engine::printData() const {
    std::ostringstream oss;
    oss << "[Engine] Data:\n";
    for (size_t i = 0; i < data_.size(); ++i) {
        oss << "  [" << i << "]: ";
        for (int v : data_[i]) oss << v << " ";
        oss << "\n";
    }
    appLogger().log(oss.str());
}

void Engine::onSessionEvent(const SessionEvent& event) {
    appLogger().log("[Engine] Event received: session state changed\n");

    switch (event.type) {
        case SessionEventType::VectorAdded:
            appLogger().log("[Engine] -> recognized: vector added\n");
            addVector(event.vectorData);
            break;

        case SessionEventType::SortRequested:
            appLogger().log("[Engine] -> recognized: sort requested\n");
            sortVector(event.index);
            break;

        case SessionEventType::PrintRequested:
            appLogger().log("[Engine] -> recognized: print requested\n");
            printData();
            break;

        case SessionEventType::StrategyChangeRequested:
            appLogger().log("[Engine] -> recognized: strategy change requested\n");
            setSortStrategy(SortStrategyFactory::create(event.strategyId));
            break;

        case SessionEventType::SessionClosing:
            appLogger().log("[Engine] -> recognized: session closing, cleaning up and stopping\n");
            stop();
            break;
    }
}

} // namespace patterns::engine
