#pragma once
#include <string>
#include <tuple>
#include <vector>
#include <gtest/gtest.h>
#include "Signal.hpp"
#include "SequenceLog.hpp"
#include "SignalComparator.hpp"
#include "SignalMismatchFormatter.hpp"

// ─── ScenarioVerifier ─────────────────────────────────────────────────────────
// Central verification engine for strict, per-step scenario contracts.
//
// ── Per-step collection API (used by ScenarioExecutor) ───────────────────────
//   beginStep()             — opens a new step; spies collect into stepActuals_
//   report(actual)          — called by spies during stimulus; buffers actual
//   endStepCollection()     — stops collecting; subsequent report() calls ignored
//   matchExpectation(exp)   — ordered match of one expectation vs stepActuals_
//   finalizeStep()          — reports unmatched actuals; closes step
//   flushDiagramRows()      — called by ScenarioExecutor after finalizeStep()
//
// Assumes collaborator calls caused by a Stimulus are synchronous.
// A signal arriving after endStepCollection() is silently discarded (pre-test
// setup calls via engine_ are harmless — no step is open at that point).
//
// Failure semantics:
//   Unexpected signal   → ADD_FAILURE, stepConfused_ set (prevents cascading)
//   Wrong payload       → ADD_FAILURE, matching advances (order still verified)
//   Signal not received → ADD_FAILURE (suppressed if stepConfused_)

class ScenarioVerifier {
public:
    // Called by spies synchronously during stimulus execution.
    // If a step is open (collecting), buffers actual into stepActuals_.
    // If no step is open (pre-test setup calls), silently discards.
    void report(const SignalDescriptor& actual) {
        if (collectingActuals_)
            stepActuals_.push_back(actual);
    }

    // Flush queued diagram rows.
    // Called by ScenarioExecutor after finalizeStep() and outside CaptureStdout scope.
    void flushDiagramRows() {
        for (auto& [from, to, name] : pendingRows_)
            SequenceLog::logFlow(from, to, name);
        pendingRows_.clear();
    }

    // Opens a new step: clears per-step state and starts collecting actuals.
    // Called by ScenarioExecutor before executing a Stimulus.
    void beginStep() {
        stepActuals_.clear();
        nextActualInStep_ = 0;
        stepConfused_     = false;
        collectingActuals_ = true;
    }

    // Stops collecting actuals. Called by ScenarioExecutor after Stimulus execution.
    void endStepCollection() {
        collectingActuals_ = false;
    }

    // Matches one Expectation against the next actual in stepActuals_ (ordered).
    // Called by ScenarioExecutor for each Expectation signal in the stream.
    void matchExpectation(const Signal& exp) {
        if (stepConfused_) return;

        if (nextActualInStep_ >= stepActuals_.size()) {
            ADD_FAILURE()
                << "Signal not received:\n"
                << "  from:   " << endpointName(exp.from) << "\n"
                << "  to:     " << endpointName(exp.to)   << "\n"
                << "  signal: " << exp.name;
            return;
        }

        const SignalDescriptor& actual = stepActuals_[nextActualInStep_];
        auto result = compareSignals(exp, actual);

        if (!result) {
            ADD_FAILURE() << SignalMismatchFormatter::format(result.error());
            if (result.error().kind == SignalMismatchKind::MetadataMismatch) {
                stepConfused_ = true;
                ++nextActualInStep_;
                return;
            }
            // PayloadMismatch: advance
        }

        pendingRows_.emplace_back(exp.from, exp.to, exp.name);
        ++nextActualInStep_;
    }

    // Closes the current step.
    // Reports any unmatched actuals as "Unexpected signal".
    // Safe to call from a destructor — uses ADD_FAILURE (no throw).
    void finalizeStep() {
        if (!stepConfused_) {
            for (size_t i = nextActualInStep_; i < stepActuals_.size(); ++i)
                ADD_FAILURE() << SignalMismatchFormatter::format(
                                  makeUnexpectedExtra(stepActuals_[i]));
        }
        stepActuals_.clear();
        nextActualInStep_  = 0;
        stepConfused_      = false;
        collectingActuals_ = false;
    }

private:
    std::vector<SignalDescriptor>                            stepActuals_;
    size_t                                                   nextActualInStep_  = 0;
    bool                                                     collectingActuals_ = false;
    bool                                                     stepConfused_      = false;
    std::vector<std::tuple<Endpoint, Endpoint, std::string>> pendingRows_;
};
