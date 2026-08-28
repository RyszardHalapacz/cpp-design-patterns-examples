#pragma once
#include <any>
#include "Signal.hpp"
#include "ScenarioExecutor.hpp"
#include "MatcherHelpers.hpp"
#include "patterns/strategy/SortStrategyId.hpp"

// ─── FactoryEndpoint ──────────────────────────────────────────────────────────
// Typed test handle for the Factory channel.
// Public member of FactorySpy — accessible in TEST_F via inheritance.
//
// create(id) delegates to detail::matchSortStrategyId in MatcherHelpers.hpp.
// payloadMatcher returns PayloadMatchResult (structured mismatch, not bool).
//
// receive(sig) delegates to ScenarioExecutor::declareExpectation().

class FactoryEndpoint {
public:
    [[nodiscard]] Signal create(patterns::strategy::SortStrategyId id) const {
        return {
            .role   = SignalRole::Expectation,
            .name   = "create",
            .from   = Endpoint::Engine,
            .to     = Endpoint::Factory,
            .action = {},
            .payloadMatcher = [id](const std::any& payload) -> PayloadMatchResult {
                return detail::matchSortStrategyId(id, payload);
            }
        };
    }

    void receive(Signal sig) {
        executor_->declareExpectation(std::move(sig));
    }

    // Called by EngineTestBase::SetUp().
    void attach(ScenarioExecutor& ex) { executor_ = &ex; }

private:
    ScenarioExecutor* executor_ = nullptr;
};
