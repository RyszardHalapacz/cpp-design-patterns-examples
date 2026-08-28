#pragma once
#include <string>
#include <tuple>
#include <vector>
#include <gtest/gtest.h>
#include "Signal.hpp"
#include "SequenceLog.hpp"

// ─── ScenarioVerifier ─────────────────────────────────────────────────────────
// Central verification engine for strict, per-step scenario contracts.
//
// ── Mode 1 — legacy direct API (used by ScenarioFrameworkTest) ───────────────
//   setExpected(expectations)   — arms verifier for this step; resets all state
//   report(actual)              — called by spies during stimulus (armed path)
//   flushDiagramRows()          — called by driver after GetCapturedStdout()
//   verifyComplete()            — called by driver at end of step; disarms verifier
//
// ── Mode 2 — stateful per-step collection (used by EngineDriver) ─────────────
//   beginStep()                 — opens a new step; spies collect into stepActuals_
//   endStepCollection()         — stops collecting; spies no longer buffer
//   matchExpectation(exp)       — ordered match of one expectation vs stepActuals_
//   finalizeStep()              — reports unmatched actuals; closes step
//
//   Mode 2 assumes collaborator calls caused by a Stimulus are synchronous.
//   A signal arriving after endStepCollection() falls through to Mode 1 logic
//   (silently ignored when armed_==false), which is safe for the current Engine.
//
// Failure semantics (both modes):
//   Unexpected signal   → ADD_FAILURE, confused flag set (prevents cascading)
//   Wrong payload       → ADD_FAILURE, matching advances (order still verified)
//   Signal not received → ADD_FAILURE (suppressed if confused)

class ScenarioVerifier {
public:
    // Arms the verifier with the ordered expectations for this step.
    // Resets all state including confused_ — each step starts clean.
    void setExpected(std::vector<Signal> expectations) {
        expectations_  = std::move(expectations);
        nextExpected_  = 0;
        armed_         = true;
        confused_      = false;
        pendingRows_.clear();
    }

    // Called by spies synchronously during stimulus execution.
    // Mode 2: if collecting, buffers actual into stepActuals_ for deferred matching.
    // Mode 1: if armed, matches actual against next expected signal immediately.
    void report(const SignalDescriptor& actual) {
        if (collectingActuals_) {
            stepActuals_.push_back(actual);
            return;
        }
        if (!armed_ || confused_) return;

        // No more expected in this step → unexpected call
        if (nextExpected_ >= expectations_.size()) {
            ADD_FAILURE()
                << "Unexpected signal:\n"
                << "  from:   " << endpointName(actual.from) << "\n"
                << "  to:     " << endpointName(actual.to)   << "\n"
                << "  signal: " << actual.name;
            confused_ = true;
            return;
        }

        const Signal& expected = expectations_[nextExpected_];

        // Wrong endpoint or name → order violation within this step
        if (actual.from != expected.from
                || actual.to   != expected.to
                || actual.name != expected.name)
        {
            ADD_FAILURE()
                << "Unexpected signal:\n"
                << "  received: " << endpointName(actual.from) << " -> "
                                  << endpointName(actual.to)   << " : " << actual.name << "\n"
                << "  expected: " << endpointName(expected.from) << " -> "
                                  << endpointName(expected.to)   << " : " << expected.name;
            confused_ = true;
            return;
        }

        // Correct signal, wrong payload — advance anyway (signal was received)
        if (expected.payloadMatcher && !expected.payloadMatcher(actual.payload)) {
            ADD_FAILURE()
                << "Signal payload mismatch:\n"
                << "  signal: " << endpointName(actual.from) << " -> "
                                << endpointName(actual.to)   << " : " << actual.name;
            pendingRows_.emplace_back(expected.from, expected.to, expected.name);
            ++nextExpected_;
            return;
        }

        // All checks passed
        pendingRows_.emplace_back(expected.from, expected.to, expected.name);
        ++nextExpected_;
    }

    // Flush queued Expectation diagram rows.
    // Must be called after GetCapturedStdout() to avoid rows being captured.
    void flushDiagramRows() {
        for (auto& [from, to, name] : pendingRows_)
            SequenceLog::logFlow(from, to, name);
        pendingRows_.clear();
    }

    // Called at end of each step.
    // Reports expected signals that never arrived.
    // Suppressed when confused_ to avoid misleading "not received" messages
    // after an ordering error already invalidated the step.
    void verifyComplete() {
        if (!confused_) {
            for (size_t i = nextExpected_; i < expectations_.size(); ++i) {
                const Signal& sig = expectations_[i];
                ADD_FAILURE()
                    << "Signal not received:\n"
                    << "  from:   " << endpointName(sig.from) << "\n"
                    << "  to:     " << endpointName(sig.to)   << "\n"
                    << "  signal: " << sig.name;
            }
        }
        armed_ = false;
    }

    // ── Mode 2 ───────────────────────────────────────────────────────────────

    // Opens a new step: clears per-step state and starts collecting actuals.
    // Called by EngineDriver before executing a Stimulus.
    //
    // TODO: beginStep() powinno zakładać, że verifier jest w czystym stanie.
    //   Jeśli collectingActuals_ == true lub stepActuals_ nie jest pusty,
    //   oznacza to że poprzedni krok nie został domknięty przez finalizeStep().
    //   Zamiast cicho kasować dane, powinno zgłosić ADD_FAILURE() z komunikatem
    //   "beginStep() called on an already-open step (missing finalizeStep())".
    //   Czyszczenie stanu to odpowiedzialność finalizeStep(), nie beginStep().
    void beginStep() {
        stepActuals_.clear();
        nextActualInStep_ = 0;
        stepConfused_     = false;
        collectingActuals_ = true;
    }

    // Stops collecting actuals. Called by EngineDriver after Stimulus execution.
    void endStepCollection() {
        collectingActuals_ = false;
    }

    // Matches one Expectation against the next actual in stepActuals_ (ordered).
    // Called by EngineDriver for each Expectation signal in the stream.
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

        if (actual.from != exp.from || actual.to != exp.to || actual.name != exp.name) {
            ADD_FAILURE()
                << "Unexpected signal:\n"
                << "  received: " << endpointName(actual.from) << " -> "
                                  << endpointName(actual.to)   << " : " << actual.name << "\n"
                << "  expected: " << endpointName(exp.from)    << " -> "
                                  << endpointName(exp.to)      << " : " << exp.name;
            stepConfused_ = true;
            ++nextActualInStep_;
            return;
        }

        if (exp.payloadMatcher && !exp.payloadMatcher(actual.payload)) {
            ADD_FAILURE()
                << "Signal payload mismatch:\n"
                << "  signal: " << endpointName(actual.from) << " -> "
                                << endpointName(actual.to)   << " : " << actual.name;
        }

        pendingRows_.emplace_back(exp.from, exp.to, exp.name);
        ++nextActualInStep_;
    }

    // Closes the current step.
    // Reports any unmatched actuals as "Unexpected signal".
    // Safe to call from a destructor — uses ADD_FAILURE (no throw).
    void finalizeStep() {
        if (!stepConfused_) {
            for (size_t i = nextActualInStep_; i < stepActuals_.size(); ++i) {
                const SignalDescriptor& actual = stepActuals_[i];
                ADD_FAILURE()
                    << "Unexpected signal:\n"
                    << "  from:   " << endpointName(actual.from) << "\n"
                    << "  to:     " << endpointName(actual.to)   << "\n"
                    << "  signal: " << actual.name;
            }
        }
        stepActuals_.clear();
        nextActualInStep_  = 0;
        stepConfused_      = false;
        collectingActuals_ = false;
    }

private:
    // Mode 1 fields
    std::vector<Signal>                                      expectations_;
    size_t                                                   nextExpected_ = 0;
    bool                                                     armed_        = false;
    bool                                                     confused_     = false;
    std::vector<std::tuple<Endpoint, Endpoint, std::string>> pendingRows_;

    // Mode 2 fields
    std::vector<SignalDescriptor> stepActuals_;
    size_t                        nextActualInStep_  = 0;
    bool                          collectingActuals_ = false;
    bool                          stepConfused_      = false;
};
