#pragma once
#include <any>
#include <optional>
#include <string>
#include <utility>      // std::move
#include <vector>
#include "PayloadMismatch.hpp"
#include "ExpectationSpecs.hpp"
#include "patterns/historian/IHistorian.hpp"
#include "patterns/strategy/SortStrategyId.hpp"

namespace detail {

// ── CommandHistory matcher ────────────────────────────────────────────────────
// Checks commandName. If expectedData is provided, also diffs data per element.
// All mismatching fields collected — no early exit.
inline PayloadMatchResult matchCommandHistory(
        const std::string&                     expectedName,
        const std::optional<std::vector<int>>& expectedData,
        const std::any&                        payload)
{
    const auto* cmd = std::any_cast<patterns::historian::CommandHistory>(&payload);
    if (!cmd) return payloadWrongType("CommandHistory", anyTypeName(payload));

    std::vector<FieldMismatch> fields;

    if (cmd->commandName != expectedName)
        fields.push_back({"commandName",
            "\"" + expectedName + "\"", "\"" + cmd->commandName + "\""});

    if (expectedData) {
        auto diffs = diffVector("data", *expectedData, cmd->data);
        fields.insert(fields.end(), diffs.begin(), diffs.end());
    }

    if (!fields.empty())
        return payloadFieldMismatch("CommandHistory", std::move(fields));
    return payloadOk();
}

// ── EngineSnapshot matcher ────────────────────────────────────────────────────
// Checks each field present in spec. Uses sortStrategyIdName() — existing project
// function in SortStrategyId.hpp. All mismatching fields collected.
inline PayloadMatchResult matchEngineSnapshot(
        const ExpectedEngineSnapshot& spec,
        const std::any&               payload)
{
    const auto* snap = std::any_cast<patterns::historian::EngineSnapshot>(&payload);
    if (!snap) return payloadWrongType("EngineSnapshot", anyTypeName(payload));

    std::vector<FieldMismatch> fields;

    if (spec.running && snap->running != *spec.running)
        fields.push_back({"running",
            *spec.running ? "true" : "false",
            snap->running  ? "true" : "false"});

    if (spec.strategy && snap->strategy != *spec.strategy)
        fields.push_back({"strategy",
            patterns::strategy::sortStrategyIdName(*spec.strategy),
            patterns::strategy::sortStrategyIdName(snap->strategy)});

    if (spec.vectorCount && snap->vectorCount != *spec.vectorCount)
        fields.push_back({"vectorCount",
            std::to_string(*spec.vectorCount),
            std::to_string(snap->vectorCount)});

    if (!fields.empty())
        return payloadFieldMismatch("EngineSnapshot", std::move(fields));
    return payloadOk();
}

// ── SortStrategyId matcher ────────────────────────────────────────────────────
// Uses sortStrategyIdName() — covers Ascending, Descending, Bubble.
inline PayloadMatchResult matchSortStrategyId(
        patterns::strategy::SortStrategyId expected,
        const std::any&                    payload)
{
    const auto* rid = std::any_cast<patterns::strategy::SortStrategyId>(&payload);
    if (!rid) return payloadWrongType("SortStrategyId", anyTypeName(payload));
    if (*rid != expected)
        return payloadFieldMismatch("SortStrategyId", {
            {"value",
             patterns::strategy::sortStrategyIdName(expected),
             patterns::strategy::sortStrategyIdName(*rid)}
        });
    return payloadOk();
}

} // namespace detail
