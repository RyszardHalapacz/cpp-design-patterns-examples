#pragma once
#include <vector>
#include <gtest/gtest.h>
#include "patterns/engine/Engine.hpp"
#include "scenario/Signal.hpp"
#include "scenario/SequenceLog.hpp"
#include "scenario/ScenarioVerifier.hpp"

// ─── EngineDriver ─────────────────────────────────────────────────────────────
// Executes a scenario against a live Engine using step-based verification.
//
// A "step" is one Stimulus followed by all immediately following Expectations.
// Verification is scoped per step:
//   1. verifier_.setExpected(step.expectations)
//   2. CaptureStdout → stimulus.action(engine_) → GetCapturedStdout
//   3. SequenceLog::logFlow(stimulus)
//   4. verifier_.flushDiagramRows()   ← prints Expectation rows outside capture
//   5. verifier_.verifyComplete()     ← checks for missing signals in this step
//
// Selective verification via ActiveChannels:
//   When building a step, expectations for inactive endpoints are silently skipped.
//   They do NOT appear in verifier_.setExpected() and do NOT produce
//   "Signal not received" failures. This lets the same pre-built scenario work
//   across different fixture topologies (EngineComponentTest / HistorianOnlyTest /
//   FactoryOnlyTest) without modification.
//
// Malformed scenario: an Expectation appearing before any Stimulus is an error —
// the framework does not silently ignore it (ADD_FAILURE + return).

class EngineDriver {
public:
    EngineDriver(patterns::engine::Engine& engine,
                 ScenarioVerifier&          verifier,
                 ActiveChannels             channels)
        : engine_(engine), verifier_(verifier), channels_(channels) {}

    void run(const std::vector<Signal>& scenario) {
        struct Step {
            const Signal*       stimulus;
            std::vector<Signal> expectations;
        };

        // Group scenario into steps.
        // Expectations for inactive channels are silently skipped (not "Signal not received").
        // An Expectation before the first Stimulus is a framework error.
        std::vector<Step> steps;
        for (const auto& sig : scenario) {
            if (sig.role == SignalRole::Stimulus) {
                steps.push_back({&sig, {}});
            } else if (sig.role == SignalRole::Expectation) {
                if (steps.empty()) {
                    ADD_FAILURE()
                        << "Malformed scenario:\n"
                        << "  Expectation \"" << sig.name
                        << "\" appears before any Stimulus";
                    return;
                }
                // Skip expectations for inactive channels.
                // They do not participate in the contract for this fixture topology.
                if (!channels_.isActive(sig.to)) {
                    continue;
                }
                steps.back().expectations.push_back(sig);
            }
        }

        // Execute each step with isolated verification scope.
        for (const auto& step : steps) {
            verifier_.setExpected(step.expectations);

            testing::internal::CaptureStdout();
            step.stimulus->action(engine_);
            std::string captured = testing::internal::GetCapturedStdout();

            SequenceLog::logFlow(step.stimulus->from, step.stimulus->to,
                                 step.stimulus->name, captured);

            // Flush Expectation rows after GetCapturedStdout so they are
            // not captured and appear at correct position in the diagram.
            verifier_.flushDiagramRows();

            // Per-step verification: missing signals → ADD_FAILURE here.
            verifier_.verifyComplete();
        }
    }

private:
    patterns::engine::Engine& engine_;
    ScenarioVerifier&          verifier_;
    ActiveChannels             channels_;
};
