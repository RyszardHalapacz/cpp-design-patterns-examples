#pragma once
#include <vector>
#include <gtest/gtest.h>
#include "patterns/engine/Engine.hpp"
#include "scenario/Signal.hpp"
#include "scenario/SequenceLog.hpp"
#include "scenario/ScenarioVerifier.hpp"

// ─── EngineDriver ─────────────────────────────────────────────────────────────
// Executes a scenario against a live Engine using a stateful per-step parser.
//
// Multiple run() calls within one TEST form a single continuous signal stream.
// The boundary between run() calls has no semantic significance.
//
// A "step" is one Stimulus and all Expectations that follow it (in any run()).
// At most one step is open (pending) at any time.
//
// Parser rules:
//   Stimulus encountered:
//     → finalize previous step (if open): report unmatched actuals as errors
//     → open new step: beginStep(), execute stimulus, endStepCollection()
//   Expectation encountered:
//     → if no step open: Malformed scenario (ADD_FAILURE + return)
//     → if inactive channel: silently skip (not "Signal not received")
//     → otherwise: matchExpectation() against current step's actuals (ordered)
//   ~EngineDriver():
//     → finalize pending step (if open)
//
// Selective verification via ActiveChannels:
//   Expectations for inactive endpoints are silently skipped and do NOT produce
//   "Signal not received". The same pre-built Scenarios::* work across all
//   fixture topologies (EngineComponentTest / HistorianOnlyTest / FactoryOnlyTest).
//
// Diagram rows: flushDiagramRows() is called after finalizeStep() and in the
//   destructor, always outside any CaptureStdout scope.

class EngineDriver {
public:
    EngineDriver(patterns::engine::Engine& engine,
                 ScenarioVerifier&          verifier,
                 ActiveChannels             channels)
        : engine_(engine), verifier_(verifier), channels_(channels) {}

    ~EngineDriver() {
        if (stepOpen_) {
            verifier_.finalizeStep();
            verifier_.flushDiagramRows();
        }
    }

    void run(const std::vector<Signal>& scenario) {
        for (const auto& sig : scenario) {

            if (sig.role == SignalRole::Stimulus) {
                // Finalize previous step before opening the new one.
                // flushDiagramRows() here is outside any CaptureStdout scope.
                if (stepOpen_) {
                    verifier_.finalizeStep();
                    verifier_.flushDiagramRows();
                }

                verifier_.beginStep();
                stepOpen_ = true;

                testing::internal::CaptureStdout();
                sig.action(engine_);
                std::string captured = testing::internal::GetCapturedStdout();

                verifier_.endStepCollection();
                SequenceLog::logFlow(sig.from, sig.to, sig.name, captured);

            } else if (sig.role == SignalRole::Expectation) {
                if (!stepOpen_) {
                    ADD_FAILURE()
                        << "Malformed scenario:\n"
                        << "  Expectation \"" << sig.name
                        << "\" appears before any Stimulus";
                    return;
                }
                // Skip expectations for inactive channels.
                // They do not participate in the contract for this fixture topology.
                if (!channels_.isActive(sig.to)) continue;

                verifier_.matchExpectation(sig);
            }
        }
        // Step remains open across run() calls — finalized by next Stimulus or destructor.
    }

private:
    patterns::engine::Engine& engine_;
    ScenarioVerifier&          verifier_;
    ActiveChannels             channels_;
    bool                       stepOpen_ = false;
};
