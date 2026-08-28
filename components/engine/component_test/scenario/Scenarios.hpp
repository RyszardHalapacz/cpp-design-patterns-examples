#pragma once
#include <optional>
#include <utility>      // std::move
#include <vector>

#include "patterns/historian/IHistorian.hpp"
#include "patterns/strategy/SortStrategyId.hpp"
#include "Signal.hpp"
#include "MatcherHelpers.hpp"    // detail::match*
#include "ExpectationSpecs.hpp"  // ExpectedEngineSnapshot

// ═══════════════════════════════════════════════════════════════════════════════
// Internal expectation builders — framework test infrastructure only.
//
// Used exclusively by ScenarioFrameworkTest (and the new SignalComparatorTest /
// ScenarioVerifierTest) to test ScenarioVerifier in isolation.
// These functions do NOT appear in normal component test bodies (TEST_F).
// The public component test DSL is: engine.receive(...) / historian.receive(...)
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
        .payloadMatcher = [n = std::move(commandName), data]
                          (const std::any& payload) -> PayloadMatchResult {
            return detail::matchCommandHistory(n, data, payload);
        }
    };
}

// Expects historian.publishSnapshot().
// Primary overload: full partial expectation via ExpectedEngineSnapshot.
inline Signal expectHistorianSnapshot(ExpectedEngineSnapshot spec = {})
{
    return {
        .role   = SignalRole::Expectation,
        .name   = "publishSnapshot",
        .from   = Endpoint::Engine,
        .to     = Endpoint::Historian,
        .action         = {},
        .payloadMatcher = [spec](const std::any& payload) -> PayloadMatchResult {
            return detail::matchEngineSnapshot(spec, payload);
        }
    };
}

// Convenience overload: vectorCount-only check.
// Preserves existing call sites: expectHistorianSnapshot(std::nullopt)
// and expectHistorianSnapshot(3).
inline Signal expectHistorianSnapshot(std::optional<std::size_t> vectorCount)
{
    ExpectedEngineSnapshot spec;
    spec.vectorCount = vectorCount;
    return expectHistorianSnapshot(spec);
}

// Expects factory.create() called with the given SortStrategyId.
inline Signal expectFactoryCreate(patterns::strategy::SortStrategyId id) {
    return {
        .role   = SignalRole::Expectation,
        .name   = "create",
        .from   = Endpoint::Engine,
        .to     = Endpoint::Factory,
        .action         = {},
        .payloadMatcher = [id](const std::any& payload) -> PayloadMatchResult {
            return detail::matchSortStrategyId(id, payload);
        }
    };
}
