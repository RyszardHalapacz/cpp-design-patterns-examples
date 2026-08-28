#pragma once
#include <any>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Signal.hpp"
#include "ScenarioExecutor.hpp"
#include "patterns/historian/IHistorian.hpp"

// ─── HistorianEndpoint ────────────────────────────────────────────────────────
// Typed test handle dla kanału Historian.
// Publiczny member HistorianSpy — dostępny w TEST_F przez dziedziczenie.
//
// Metody builderów (addVector, sortVector, setSortStrategy, publishSnapshot)
// zwracają Signal z payloadMatcher. Są typowanym odpowiednikiem dotychczasowych
// expectHistorianCommand() / expectHistorianSnapshot() z Scenarios.hpp.
//
// Wewnętrzne stringi ("recordCommand", "addVector" itp.) są szczegółem
// implementacyjnym tego adaptera — scentralizowane tutaj, nie powielane w testach.
// Zmiana nazwy komendy w Engine wymaga edycji dokładnie jednej metody.
//
// receive(sig) deleguje do ScenarioExecutor::declareExpectation().

class HistorianEndpoint {
public:
    // ── Deskryptory expectations — domain-level ───────────────────────────────

    [[nodiscard]] Signal addVector(std::vector<int> data) const {
        return makeRecordCommand(
            [data](const patterns::historian::CommandHistory& cmd) {
                return cmd.commandName == "addVector" && cmd.data == data;
            });
    }

    [[nodiscard]] Signal sortVector() const {
        return makeRecordCommand(
            [](const patterns::historian::CommandHistory& cmd) {
                return cmd.commandName == "sortVector";
            });
    }

    [[nodiscard]] Signal setSortStrategy() const {
        return makeRecordCommand(
            [](const patterns::historian::CommandHistory& cmd) {
                return cmd.commandName == "setSortStrategy";
            });
    }

    [[nodiscard]] Signal publishSnapshot(std::optional<size_t> vectorCount = {}) const {
        return {
            .role   = SignalRole::Expectation,
            .name   = "publishSnapshot",
            .from   = Endpoint::Engine,
            .to     = Endpoint::Historian,
            .action = {},
            .payloadMatcher = [vectorCount](const std::any& payload) -> bool {
                const auto* snap =
                    std::any_cast<patterns::historian::EngineSnapshot>(&payload);
                if (!snap)                                            return false;
                if (vectorCount && snap->vectorCount != *vectorCount) return false;
                return true;
            }
        };
    }

    // ── Deklaracja expectation ────────────────────────────────────────────────

    void receive(Signal sig) {
        executor_->declareExpectation(std::move(sig));
    }

    // Wywoływane przez EngineTestBase::SetUp().
    void attach(ScenarioExecutor& ex) { executor_ = &ex; }

private:
    Signal makeRecordCommand(
            std::function<bool(const patterns::historian::CommandHistory&)> matcher) const
    {
        return {
            .role   = SignalRole::Expectation,
            .name   = "recordCommand",
            .from   = Endpoint::Engine,
            .to     = Endpoint::Historian,
            .action = {},
            .payloadMatcher = [m = std::move(matcher)](const std::any& payload) -> bool {
                const auto* cmd =
                    std::any_cast<patterns::historian::CommandHistory>(&payload);
                return cmd && m(*cmd);
            }
        };
    }

    ScenarioExecutor* executor_ = nullptr;
};
