#pragma once
#include <memory>
#include <vector>
#include "patterns/observer/ISessionObserver.hpp"
#include "patterns/strategy/ISortStrategy.hpp"

namespace patterns::engine {

class Engine : public patterns::observer::ISessionObserver {
public:
    Engine();

    void start();
    void stop();
    void addVector(const std::vector<int>& vec);
    void setSortStrategy(std::unique_ptr<patterns::strategy::ISortStrategy> strategy);
    void sortVector(size_t index);
    void printData() const;

    void onSessionEvent(const patterns::observer::SessionEvent& event) override;

private:
    bool                                         running_ = false;
    std::vector<std::vector<int>>                data_;
    std::unique_ptr<patterns::strategy::ISortStrategy> sortStrategy_;
};

} // namespace patterns::engine
