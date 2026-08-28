#pragma once
#include <any>
#include "Signal.hpp"
#include "ScenarioExecutor.hpp"
#include "patterns/strategy/SortStrategyId.hpp"

// ─── FactoryEndpoint ──────────────────────────────────────────────────────────
// Typed test handle dla kanału Factory.
// Publiczny member FactorySpy — dostępny w TEST_F przez dziedziczenie.
//
// receive(sig) deleguje do ScenarioExecutor::declareExpectation().

class FactoryEndpoint {
public:
    [[nodiscard]] Signal create(patterns::strategy::SortStrategyId id) const {
        return {
            .role   = SignalRole::Expectation,
            .name   = "create",
            .from   = Endpoint::Engine,
            .to     = Endpoint::Factory,
            .action = {},
            .payloadMatcher = [id](const std::any& payload) -> bool {
                const auto* rid =
                    std::any_cast<patterns::strategy::SortStrategyId>(&payload);
                return rid && *rid == id;
            }
        };
    }

    void receive(Signal sig) {
        executor_->declareExpectation(std::move(sig));
    }

    // Wywoływane przez EngineTestBase::SetUp().
    void attach(ScenarioExecutor& ex) { executor_ = &ex; }

private:
    ScenarioExecutor* executor_ = nullptr;
};
