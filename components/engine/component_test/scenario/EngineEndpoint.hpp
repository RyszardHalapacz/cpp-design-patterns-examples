#pragma once
#include <functional>
#include <string>
#include <vector>
#include "Signal.hpp"
#include "ScenarioExecutor.hpp"
#include "patterns/engine/Engine.hpp"
#include "patterns/observer/SessionEvent.hpp"
#include "patterns/strategy/SortStrategyId.hpp"

// ─── EngineEndpoint ───────────────────────────────────────────────────────────
// Typed test handle dla kanału Engine.
// Publiczny member EngineTestBase — dostępny we wszystkich TEST_F.
//
// Metody builderów (addVector, sortVector, strategyChange, publishSnapshot)
// zwracają Descriptor z lambdą akcji. receive(d) przekazuje akcję do
// ScenarioExecutor::executeStimulus(), który zarządza cyklem życia kroku.
//
// Logicznym nadawcą jest Endpoint::Driver — test inicjuje wywołanie.

class EngineEndpoint {
public:
    // ── Descriptor — akcja do wykonania przeciwko Engine ─────────────────────

    struct Descriptor {
        std::string                                    name;
        std::function<void(patterns::engine::Engine&)> action;
    };

    // ── Deskryptory stimulus — domain-level ───────────────────────────────────

    [[nodiscard]] Descriptor addVector(std::vector<int> data) const {
        return {"onSessionEvent(addVector)",
            [data](patterns::engine::Engine& e) {
                patterns::observer::SessionEvent ev;
                ev.type       = patterns::observer::SessionEventType::VectorAdded;
                ev.vectorData = data;
                e.onSessionEvent(ev);
            }};
    }

    [[nodiscard]] Descriptor sortVector(size_t index = 0) const {
        return {"onSessionEvent(sortVector)",
            [index](patterns::engine::Engine& e) {
                patterns::observer::SessionEvent ev;
                ev.type  = patterns::observer::SessionEventType::SortRequested;
                ev.index = index;
                e.onSessionEvent(ev);
            }};
    }

    [[nodiscard]] Descriptor strategyChange(patterns::strategy::SortStrategyId id) const {
        return {"onSessionEvent(strategyChange)",
            [id](patterns::engine::Engine& e) {
                patterns::observer::SessionEvent ev;
                ev.type       = patterns::observer::SessionEventType::StrategyChangeRequested;
                ev.strategyId = id;
                e.onSessionEvent(ev);
            }};
    }

    [[nodiscard]] Descriptor publishSnapshot() const {
        return {"publishSnapshot",
            [](patterns::engine::Engine& e) { e.publishSnapshot(); }};
    }

    // ── Wykonanie stimulus ────────────────────────────────────────────────────

    void receive(Descriptor d) {
        executor_->executeStimulus(
            Endpoint::Driver, Endpoint::Engine, d.name, d.action, *engine_);
    }

    // Wywoływane przez EngineTestBase::SetUp().
    void attach(ScenarioExecutor& ex, patterns::engine::Engine& eng) {
        executor_ = &ex;
        engine_   = &eng;
    }

private:
    ScenarioExecutor*         executor_ = nullptr;
    patterns::engine::Engine* engine_   = nullptr;
};
