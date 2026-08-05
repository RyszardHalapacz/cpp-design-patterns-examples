#pragma once
#include <vector>
#include "patterns/strategy/SortStrategyId.hpp"

namespace patterns::observer {

enum class SessionEventType {
    VectorAdded,
    SortRequested,
    PrintRequested,
    SessionClosing,
    StrategyChangeRequested
};

struct SessionEvent {
    SessionEventType              type;
    std::vector<int>              vectorData;
    size_t                        index      = 0;
    patterns::strategy::SortStrategyId strategyId = patterns::strategy::SortStrategyId::Ascending;
};

} // namespace patterns::observer
