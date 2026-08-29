#pragma once
#include <any>
#include <functional>
#include <string>

#include "PayloadMismatch.hpp"

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

// ─── Signal ───────────────────────────────────────────────────────────────────
// Expectation descriptor: describes a signal Engine must emit on a collaborator boundary.
// Built by HistorianEndpoint / FactoryEndpoint builder methods and passed to
// ScenarioExecutor::declareExpectation() via endpoint::receive().
struct Signal {
    std::string name;
    Endpoint    from;
    Endpoint    to;

    // Returns PayloadMatchResult (success or structured mismatch).
    // Null → any payload accepted (only name + endpoints checked).
    std::function<PayloadMatchResult(const std::any&)> payloadMatcher;
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
// Passed to ScenarioExecutor; expectations for inactive endpoints are silently
// skipped — they do not participate in the contract and do NOT produce
// "Signal not received" failures.
//
// Topology examples:
//   EngineComponentTest (both ON):   factory.receive(...)   → VERIFY
//                                    historian.receive(...) → VERIFY
//
//   HistorianOnlyTest (factory OFF): factory.receive(...)   → silently skipped
//                                    historian.receive(...) → VERIFY
//
//   FactoryOnlyTest (historian OFF): factory.receive(...)   → VERIFY
//                                    historian.receive(...) → silently skipped

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
