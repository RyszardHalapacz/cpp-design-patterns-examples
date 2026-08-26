#pragma once
#include <iterator>     // std::make_move_iterator
#include <optional>
#include <utility>      // std::move
#include <vector>

#include "patterns/engine/Engine.hpp"
#include "patterns/historian/IHistorian.hpp"
#include "patterns/observer/SessionEvent.hpp"
#include "patterns/strategy/SortStrategyId.hpp"
#include "Signal.hpp"

// ═══════════════════════════════════════════════════════════════════════════════
// Stimulus builders — Driver → Engine
// ═══════════════════════════════════════════════════════════════════════════════

inline Signal receiveVectorAdded(std::vector<int> data = {1, 2, 3}) {
    return {
        .role           = SignalRole::Stimulus,
        .name           = "receiveAddVector",
        .from           = Endpoint::Driver,
        .to             = Endpoint::Engine,
        .action         = [data](patterns::engine::Engine& e) {
            patterns::observer::SessionEvent ev;
            ev.type       = patterns::observer::SessionEventType::VectorAdded;
            ev.vectorData = data;
            e.onSessionEvent(ev);
        },
        .payloadMatcher = {}
    };
}

inline Signal receiveSortRequested(size_t index = 0) {
    return {
        .role           = SignalRole::Stimulus,
        .name           = "receiveSortRequested",
        .from           = Endpoint::Driver,
        .to             = Endpoint::Engine,
        .action         = [index](patterns::engine::Engine& e) {
            patterns::observer::SessionEvent ev;
            ev.type  = patterns::observer::SessionEventType::SortRequested;
            ev.index = index;
            e.onSessionEvent(ev);
        },
        .payloadMatcher = {}
    };
}

inline Signal receiveStrategyChange(patterns::strategy::SortStrategyId id) {
    return {
        .role           = SignalRole::Stimulus,
        .name           = "receiveStrategyChange",
        .from           = Endpoint::Driver,
        .to             = Endpoint::Engine,
        .action         = [id](patterns::engine::Engine& e) {
            patterns::observer::SessionEvent ev;
            ev.type       = patterns::observer::SessionEventType::StrategyChangeRequested;
            ev.strategyId = id;
            e.onSessionEvent(ev);
        },
        .payloadMatcher = {}
    };
}

inline Signal receivePublishSnapshot() {
    return {
        .role           = SignalRole::Stimulus,
        .name           = "receivePublishSnapshot",
        .from           = Endpoint::Driver,
        .to             = Endpoint::Engine,
        .action         = [](patterns::engine::Engine& e) { e.publishSnapshot(); },
        .payloadMatcher = {}
    };
}

// ═══════════════════════════════════════════════════════════════════════════════
// Expectation builders — Engine → collaborator
// ═══════════════════════════════════════════════════════════════════════════════

// Expects historian.recordCommand() with given commandName.
// data: if provided, also validates the payload vector.
inline Signal expectHistorianCommand(
        std::string                     commandName,
        std::optional<std::vector<int>> data = std::nullopt)
{
    return {
        .role   = SignalRole::Expectation,
        .name   = "recordCommand",
        .from   = Endpoint::Engine,
        .to     = Endpoint::Historian,
        .action         = {},
        .payloadMatcher = [commandName = std::move(commandName), data]
                          (const std::any& payload) -> bool {
            const auto* cmd =
                std::any_cast<patterns::historian::CommandHistory>(&payload);
            if (!cmd)                            return false;
            if (cmd->commandName != commandName) return false;
            if (data && cmd->data != *data)      return false;
            return true;
        }
    };
}

// Expects historian.publishSnapshot().
// vectorCount: if provided, validates snap.vectorCount.
inline Signal expectHistorianSnapshot(
        std::optional<size_t> vectorCount = std::nullopt)
{
    return {
        .role   = SignalRole::Expectation,
        .name   = "publishSnapshot",
        .from   = Endpoint::Engine,
        .to     = Endpoint::Historian,
        .action         = {},
        .payloadMatcher = [vectorCount](const std::any& payload) -> bool {
            const auto* snap =
                std::any_cast<patterns::historian::EngineSnapshot>(&payload);
            if (!snap)                                            return false;
            if (vectorCount && snap->vectorCount != *vectorCount) return false;
            return true;
        }
    };
}

// Expects factory.create() called with the given SortStrategyId.
inline Signal expectFactoryCreate(patterns::strategy::SortStrategyId id) {
    return {
        .role   = SignalRole::Expectation,
        .name   = "create",
        .from   = Endpoint::Engine,
        .to     = Endpoint::Factory,
        .action         = {},
        .payloadMatcher = [id](const std::any& payload) -> bool {
            const auto* rid =
                std::any_cast<patterns::strategy::SortStrategyId>(&payload);
            return rid && *rid == id;
        }
    };
}

// ═══════════════════════════════════════════════════════════════════════════════
// Pre-built scenario collections
// ═══════════════════════════════════════════════════════════════════════════════

namespace Scenarios {

// Engine receives VectorAdded → must record command with correct payload.
inline std::vector<Signal> AddVector(std::vector<int> data = {1, 2, 3}) {
    return {
        receiveVectorAdded(data),
        expectHistorianCommand("addVector", data),
    };
}

// Engine receives SortRequested → must record "sortVector" command.
// Precondition: engine already has a vector at `index`.
inline std::vector<Signal> SortVector(size_t index = 0) {
    return {
        receiveSortRequested(index),
        expectHistorianCommand("sortVector"),
    };
}

// Engine receives StrategyChangeRequested(id)
//   → must call factory.create(id)          } same step — both
//   → then record "setSortStrategy" command  } checked in order
inline std::vector<Signal> SetStrategy(patterns::strategy::SortStrategyId id) {
    return {
        receiveStrategyChange(id),
        expectFactoryCreate(id),
        expectHistorianCommand("setSortStrategy"),
    };
}

// Engine receives publishSnapshot()
//   → must call historian.publishSnapshot() with optional vectorCount check.
inline std::vector<Signal> PublishSnapshot(
        std::optional<size_t> vectorCount = std::nullopt)
{
    return {
        receivePublishSnapshot(),
        expectHistorianSnapshot(vectorCount),
    };
}

// Full flow: AddVector → SortVector → SetStrategy → PublishSnapshot.
// Each sub-scenario forms its own step; expectations cannot bleed across steps.
inline std::vector<Signal> FullEngineFlow(
        std::vector<int>                   data  = {1, 2, 3},
        patterns::strategy::SortStrategyId strat =
            patterns::strategy::SortStrategyId::Descending)
{
    std::vector<Signal> scenario;
    auto append = [&](std::vector<Signal> part) {
        scenario.insert(scenario.end(),
                        std::make_move_iterator(part.begin()),
                        std::make_move_iterator(part.end()));
    };
    append(AddVector(data));
    append(SortVector(0));
    append(SetStrategy(strat));
    append(PublishSnapshot(1));
    return scenario;
}

} // namespace Scenarios
