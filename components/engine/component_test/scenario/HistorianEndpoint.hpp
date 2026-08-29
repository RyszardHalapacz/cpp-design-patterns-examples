#pragma once
#include <any>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>
#include "Signal.hpp"
#include "ScenarioExecutor.hpp"
#include "ExpectationSpecs.hpp"
#include "MatcherHelpers.hpp"
#include "patterns/historian/IHistorian.hpp"

// ─── HistorianEndpoint ────────────────────────────────────────────────────────
// Typed test handle for the Historian channel.
// Public member of EngineTestBase — accessible in all fixtures.
//
// Expectation builders delegate to detail::match* helpers in MatcherHelpers.hpp.
// payloadMatcher returns PayloadMatchResult (structured mismatch, not bool).
//
// receive(sig) delegates to ScenarioExecutor::declareExpectation().
// Inactive channel (channels_.historian=false): declareExpectation() silently skips.

class HistorianEndpoint {
public:
    // ── Expectation builders — domain-level ───────────────────────────────────

    [[nodiscard]] Signal addVector(std::vector<int> data) const {
        return makeRecordCommand("addVector", std::move(data));
    }

    [[nodiscard]] Signal sortVector()      const { return makeRecordCommand("sortVector");      }
    [[nodiscard]] Signal setSortStrategy() const { return makeRecordCommand("setSortStrategy"); }

    // Primary overload: full partial expectation via ExpectedEngineSnapshot.
    [[nodiscard]] Signal publishSnapshot(ExpectedEngineSnapshot spec = {}) const {
        return {
            .name   = "publishSnapshot",
            .from   = Endpoint::Engine,
            .to     = Endpoint::Historian,
            .payloadMatcher = [spec](const std::any& payload) -> PayloadMatchResult {
                return detail::matchEngineSnapshot(spec, payload);
            }
        };
    }

    // Convenience overload: vectorCount-only check.
    // Preserves existing call sites: historian.receive(historian.publishSnapshot(1))
    [[nodiscard]] Signal publishSnapshot(std::size_t vectorCount) const {
        ExpectedEngineSnapshot spec;
        spec.vectorCount = vectorCount;
        return publishSnapshot(spec);
    }

    // ── Expectation declaration ───────────────────────────────────────────────

    void receive(Signal sig) {
        executor_->declareExpectation(std::move(sig));
    }

    // Called by EngineTestBase::SetUp().
    void attach(ScenarioExecutor& ex) { executor_ = &ex; }

private:
    // Internal helper: builds a recordCommand expectation signal.
    Signal makeRecordCommand(const std::string&              commandName,
                             std::optional<std::vector<int>> data = {}) const
    {
        return {
            .name   = "recordCommand",
            .from   = Endpoint::Engine,
            .to     = Endpoint::Historian,
            .payloadMatcher = [commandName, data]
                              (const std::any& payload) -> PayloadMatchResult {
                return detail::matchCommandHistory(commandName, data, payload);
            }
        };
    }

    ScenarioExecutor* executor_ = nullptr;
};
