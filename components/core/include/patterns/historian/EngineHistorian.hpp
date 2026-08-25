#pragma once
#include <cstddef>
#include "patterns/strategy/SortStrategyId.hpp"

namespace patterns::historian {

struct CommandHistory
{
};

struct EngineSnapshot
{
    bool                             running     = false;
    patterns::strategy::SortStrategyId strategy  = patterns::strategy::SortStrategyId::Ascending;
    std::size_t                      vectorCount = 0;
};

class EngineHistorian
{
public:
    void recordCommand(const CommandHistory&) {}
    void publishSnapshot(const EngineSnapshot&) {}
};

} // namespace patterns::historian
