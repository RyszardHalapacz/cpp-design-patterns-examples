#pragma once
#include <functional>
#include <string>
#include <gtest/gtest.h>
#include "patterns/engine/Engine.hpp"
#include "Signal.hpp"
#include "SequenceLog.hpp"
#include "ScenarioVerifier.hpp"

// ─── ScenarioExecutor ─────────────────────────────────────────────────────────
// Koordynuje kroki scenariusza; wywoływany przez endpointy.
//
// executeStimulus() — wywołany przez EngineEndpoint::receive()
//   Jeśli poprzedni krok jest otwarty: zamyka go (finalizeStep).
//   Otwiera nowy krok (beginStep), wykonuje akcję na Engine,
//   kończy zbieranie actuals (endStepCollection), loguje stimulus.
//
// declareExpectation() — wywołany przez HistorianEndpoint::receive() / FactoryEndpoint::receive()
//   Jeśli krok jest otwarty i kanał jest aktywny: matchExpectation().
//   Jeśli krok nie jest otwarty: ADD_FAILURE (expectation before stimulus).
//   Jeśli kanał nieaktywny: ciche pominięcie (nie "Signal not received").
//
// finalize() — wywołany jawnie przez TearDown() przed zniszczeniem spy sub-objectów.
//   Finalizuje oczekujący krok; failures zgłaszane tu, nie w destruktorze.
//   Idempotent — bezpieczna do wielokrotnego wywołania.
//
// ~ScenarioExecutor() — safety net: wywołuje finalize() jeśli TearDown() tego nie zrobił.
//
// Diagram rows (pendingRows_) są flushowane przez flushDiagramRows() wywoływane
// po finalizeStep(), zawsze poza zakresem CaptureStdout.

class ScenarioExecutor {
public:
    ScenarioExecutor(ScenarioVerifier& verifier, ActiveChannels channels)
        : verifier_(verifier), channels_(channels) {}

    // Nie copyable / nie moveable — trzyma referencje.
    ScenarioExecutor(const ScenarioExecutor&)            = delete;
    ScenarioExecutor& operator=(const ScenarioExecutor&) = delete;

    // Jawna finalizacja — wywoływana przez TearDown() przed zniszczeniem spy.
    // Po powrocie: stepOpen_ == false, wszystkie failures zgłoszone.
    // Idempotent przez finalized_ — bezpieczna do wielokrotnego wywołania.
    void finalize() {
        if (finalized_) return;
        finalized_ = true;
        if (stepOpen_) {
            verifier_.finalizeStep();
            verifier_.flushDiagramRows();
            stepOpen_ = false;
        }
    }

    // Safety net — no-op jeśli TearDown() już wywołał finalize().
    ~ScenarioExecutor() { finalize(); }

    // Wywołany przez EngineEndpoint::receive().
    // Parametry from/to/name służą wyłącznie do SequenceLog.
    void executeStimulus(
            Endpoint from, Endpoint to, const std::string& name,
            const std::function<void(patterns::engine::Engine&)>& action,
            patterns::engine::Engine& engine)
    {
        if (stepOpen_) {
            verifier_.finalizeStep();
            verifier_.flushDiagramRows();
        }

        verifier_.beginStep();
        stepOpen_ = true;

        testing::internal::CaptureStdout();
        action(engine);
        std::string captured = testing::internal::GetCapturedStdout();

        verifier_.endStepCollection();
        SequenceLog::logFlow(from, to, name, captured);
    }

    // Wywołany przez endpoint::receive() po stronie expectations.
    void declareExpectation(Signal sig) {
        if (!stepOpen_) {
            ADD_FAILURE()
                << "Expectation \"" << sig.name
                << "\" declared before any stimulus";
            return;
        }
        if (!channels_.isActive(sig.to)) return;

        verifier_.matchExpectation(sig);
        // pendingRows_ zostaną sflushowane przy kolejnym executeStimulus() lub w finalize().
    }

private:
    ScenarioVerifier& verifier_;
    ActiveChannels    channels_;
    bool              stepOpen_   = false;
    bool              finalized_  = false;
};
