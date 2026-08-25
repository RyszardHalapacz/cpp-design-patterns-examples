#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include "patterns/strategy/SortStrategyId.hpp"

namespace patterns::historian {

struct CommandHistory
{
    std::string      commandName;
    std::vector<int> data{};      // payload — empty when not applicable
};

struct EngineSnapshot
{
    bool                               running     = false;
    patterns::strategy::SortStrategyId strategy    = patterns::strategy::SortStrategyId::Ascending;
    std::size_t                        vectorCount = 0;
};

// ==================================
// IHISTORIAN — interface
// Receives commands and snapshots from Engine.
// Concrete implementations decide how to store or forward them.
// ==================================
class IHistorian {
public:
    virtual ~IHistorian() = default;

    virtual void recordCommand(const CommandHistory& cmd)      = 0;
    virtual void publishSnapshot(const EngineSnapshot& snap)   = 0;
};

} // namespace patterns::historian
