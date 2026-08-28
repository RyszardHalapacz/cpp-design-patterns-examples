#pragma once
#include <expected>
#include <utility>            // std::move
#include <vector>             // std::vector<SignalFieldMismatch>
#include "Signal.hpp"         // Signal, SignalDescriptor, Endpoint, endpointName
#include "SignalMismatch.hpp" // SignalMismatch, SignalMeta, SignalMismatchKind

using SignalMatchResult = std::expected<void, SignalMismatch>;

// ─── compareSignals() ─────────────────────────────────────────────────────────
// Compares expected Signal with actual SignalDescriptor.
// Returns success if all metadata and payload match.
//
// Comparison order:
//   1. from, to, name — if any differ → MetadataMismatch (all diffs reported)
//   2. payloadMatcher (if non-null)  — if fails → PayloadMismatch
//
// Step 2 is only attempted if step 1 succeeds.
// Within each step, all differences are collected before returning.
inline SignalMatchResult compareSignals(const Signal&           expected,
                                        const SignalDescriptor&  actual)
{
    SignalMeta expMeta{
        endpointName(expected.from), endpointName(expected.to), expected.name
    };
    SignalMeta actMeta{
        endpointName(actual.from), endpointName(actual.to), actual.name
    };

    // Step 1 — metadata
    std::vector<SignalFieldMismatch> metaDiffs;
    if (actual.from != expected.from)
        metaDiffs.push_back({"from", expMeta.from, actMeta.from});
    if (actual.to != expected.to)
        metaDiffs.push_back({"to", expMeta.to, actMeta.to});
    if (actual.name != expected.name)
        metaDiffs.push_back({"name", expMeta.name, actMeta.name});

    if (!metaDiffs.empty())
        return std::unexpected(SignalMismatch{
            .kind     = SignalMismatchKind::MetadataMismatch,
            .expected = expMeta,
            .actual   = actMeta,
            .metadata = std::move(metaDiffs),
            .payload  = std::nullopt
        });

    // Step 2 — payload
    if (expected.payloadMatcher) {
        auto result = expected.payloadMatcher(actual.payload);
        if (!result)
            return std::unexpected(SignalMismatch{
                .kind     = SignalMismatchKind::PayloadMismatch,
                .expected = expMeta,
                .actual   = actMeta,
                .metadata = {},
                .payload  = result.error()
            });
    }

    return {};  // success
}

// ─── makeUnexpectedExtra() ────────────────────────────────────────────────────
// Builds SignalMismatch for an actual signal that has no expected counterpart.
// Used by ScenarioVerifier when nextExpected_ >= expectations_.size() (Mode 1)
// and in finalizeStep() for unmatched actuals (Mode 2).
inline SignalMismatch makeUnexpectedExtra(const SignalDescriptor& actual)
{
    return {
        .kind     = SignalMismatchKind::UnexpectedExtra,
        .expected = std::nullopt,
        .actual   = {endpointName(actual.from), endpointName(actual.to), actual.name},
        .metadata = {},
        .payload  = std::nullopt
    };
}
