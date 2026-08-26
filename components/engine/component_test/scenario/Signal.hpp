#pragma once
#include <any>
#include <functional>
#include <string>

// Engine.hpp included directly — forward declaration conflicts with
// 'using Engine = BasicEngine<>' alias defined in that header.
#include "patterns/engine/Engine.hpp"

// ─── Endpoint ─────────────────────────────────────────────────────────────────
enum class Endpoint { Driver, Engine, Historian, Factory };

inline const char* endpointName(Endpoint e) noexcept {
    switch (e) {
        case Endpoint::Driver:    return "Driver";
        case Endpoint::Engine:    return "Engine";
        case Endpoint::Historian: return "Historian";
        case Endpoint::Factory:   return "Factory";
    }
    return "?";
}

// ─── SignalRole ────────────────────────────────────────────────────────────────
enum class SignalRole { Stimulus, Expectation };

// ─── Signal ───────────────────────────────────────────────────────────────────
struct Signal {
    SignalRole  role;
    std::string name;
    Endpoint    from;
    Endpoint    to;

    // Stimulus only: action to perform against Engine.
    std::function<void(patterns::engine::Engine&)> action;

    // Expectation only: returns true if payload is acceptable.
    // Null → any payload accepted (only name + endpoints checked).
    std::function<bool(const std::any&)> payloadMatcher;
};

// ─── SignalDescriptor ─────────────────────────────────────────────────────────
// Describes an actual signal reported by a spy to ScenarioVerifier.
struct SignalDescriptor {
    Endpoint    from;
    Endpoint    to;
    std::string name;
    std::any    payload;
};

// ─── ActiveChannels ───────────────────────────────────────────────────────────
// Declares which collaborator endpoints are actively monitored in this fixture.
//
// Passed to EngineDriver; expectations for inactive endpoints are silently
// skipped when building a step — they do not participate in the contract and
// do NOT produce "Signal not received" failures.
//
// This lets the same pre-built scenario (e.g. Scenarios::SetStrategy) work
// across different fixture topologies without modification:
//
//   EngineComponentTest (both ON): expectFactoryCreate    → VERIFY
//                                  expectHistorianCommand → VERIFY
//
//   HistorianOnlyTest (factory OFF): expectFactoryCreate    → SKIP
//                                    expectHistorianCommand → VERIFY
//
//   FactoryOnlyTest (historian OFF): expectFactoryCreate    → VERIFY
//                                    expectHistorianCommand → SKIP

struct ActiveChannels {
    bool historian = true;
    bool factory   = true;

    bool isActive(Endpoint to) const noexcept {
        switch (to) {
            case Endpoint::Historian: return historian;
            case Endpoint::Factory:   return factory;
            default:                  return true;  // Driver, Engine always active
        }
    }
};
