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
// Lifecycle per Step (one Stimulus + its Expectations):
//   setExpected(expectations)   — arms verifier for this step; resets all state
//   report(actual)              — called by spies synchronously during stimulus
//   flushDiagramRows()          — called by driver after GetCapturedStdout()
//   verifyComplete()            — called by driver at end of step; disarms verifier
//
// "Strict verification applies exclusively within an active step
//  (between setExpected and verifyComplete). Collaborator calls that happen
//  during SetUp or before driver.run() are silently ignored — the verifier
//  does not guard the component's entire lifetime."
//
// Failure semantics:
//   Unexpected signal   → ADD_FAILURE, confused_=true (prevents cascading)
//   Wrong payload       → ADD_FAILURE, nextExpected_++ (order still verified)
//   Signal not received → ADD_FAILURE in verifyComplete() (suppressed if confused)

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
    // Compares actual against next expected signal in this step's sequence.
    void report(const SignalDescriptor& actual) {
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

private:
    std::vector<Signal>                                      expectations_;
    size_t                                                   nextExpected_ = 0;
    bool                                                     armed_        = false;
    bool                                                     confused_     = false;
    std::vector<std::tuple<Endpoint, Endpoint, std::string>> pendingRows_;
};
