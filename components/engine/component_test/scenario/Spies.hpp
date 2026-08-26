#pragma once
#include <any>
#include "patterns/historian/IHistorian.hpp"
#include "patterns/strategy/ISortStrategyFactory.hpp"
#include "patterns/strategy/SortStrategyFactory.hpp"
#include "ScenarioVerifier.hpp"

// ─── HistorianSpy ─────────────────────────────────────────────────────────────
// Reports every IHistorian call synchronously to ScenarioVerifier.
// Calls arriving outside an active step (armed_=false) are silently ignored.
//
// Default-constructible for use as a base class via multiple inheritance.
// EngineTestBase::SetUp() calls attachVerifier(spyVerifier_) before any stimulus.

class HistorianSpy : public patterns::historian::IHistorian {
public:
    HistorianSpy() = default;

    void attachVerifier(ScenarioVerifier& v) { spyVerifier_ = &v; }

    void recordCommand(const patterns::historian::CommandHistory& cmd) override {
        if (!spyVerifier_) return;
        spyVerifier_->report({
            .from    = Endpoint::Engine,
            .to      = Endpoint::Historian,
            .name    = "recordCommand",
            .payload = std::any{cmd}
        });
    }

    void publishSnapshot(const patterns::historian::EngineSnapshot& snap) override {
        if (!spyVerifier_) return;
        spyVerifier_->report({
            .from    = Endpoint::Engine,
            .to      = Endpoint::Historian,
            .name    = "publishSnapshot",
            .payload = std::any{snap}
        });
    }

private:
    ScenarioVerifier* spyVerifier_ = nullptr;
};

// ─── FactorySpy ───────────────────────────────────────────────────────────────
// Reports every create() call to ScenarioVerifier.
// Delegates to real SortStrategyFactory so Engine gets a working strategy.
//
// Default-constructible for use as a base class via multiple inheritance.
// EngineTestBase::SetUp() calls attachVerifier(spyVerifier_) before any stimulus.
// The initial create(Ascending) from setFactory() is silently ignored
// because spyVerifier_ is not yet armed (armed_=false).

class FactorySpy : public patterns::strategy::ISortStrategyFactory {
public:
    FactorySpy() = default;

    void attachVerifier(ScenarioVerifier& v) { spyVerifier_ = &v; }

    [[nodiscard]] std::expected<std::unique_ptr<patterns::strategy::ISortStrategy>, std::string>
    create(patterns::strategy::SortStrategyId id) override {
        if (spyVerifier_) {
            spyVerifier_->report({
                .from    = Endpoint::Engine,
                .to      = Endpoint::Factory,
                .name    = "create",
                .payload = std::any{id}
            });
        }
        return real_.create(id);
    }

private:
    ScenarioVerifier*                       spyVerifier_ = nullptr;
    patterns::strategy::SortStrategyFactory real_;
};
