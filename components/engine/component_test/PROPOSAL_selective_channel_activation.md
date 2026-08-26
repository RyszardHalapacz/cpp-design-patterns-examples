# PROPOSAL: Selective Channel Activation via Multiple Inheritance + dynamic_cast

## 1. Problem

Poprzedni proposal przywrócił `dynamic_cast` do wykrywania topologii fixture, ale
nie przywrócił drugiej połowy mechanizmu: **filtrowania expectations wg aktywnych kanałów**.

Efektem było to, że ten sam gotowy scenariusz:

```cpp
Scenarios::SetStrategy(SortStrategyId::Descending)
// zawiera: receiveStrategyChange + expectFactoryCreate + expectHistorianCommand
```

NIE DZIAŁAŁ w `HistorianOnlyTest` (Factory OFF) — `expectFactoryCreate` produkowało
"Signal not received" zamiast zostać pominięte.

Autor testu był zmuszony pisać osobny, okrojony scenariusz:

```cpp
driver.run({
    receiveStrategyChange(...),
    // ręcznie usunięte expectFactoryCreate
    expectHistorianCommand("setSortStrategy"),
});
```

To psuje główną wartość mechanizmu: **te same gotowe kolekcje scenariuszy,
niezależnie od aktywnej topologii fixture**.

Docelowa semantyka:

```
Scenarios::SetStrategy(Descending)
    receiveStrategyChange(...)    → Stimulus     → zawsze wykonywany
    expectFactoryCreate(...)      → Factory OFF  → SKIP (nie "Signal not received")
    expectHistorianCommand(...)   → Historian ON → VERIFY

Scenarios::SetStrategy(Descending)
    receiveStrategyChange(...)    → Stimulus     → zawsze wykonywany
    expectFactoryCreate(...)      → Factory ON   → VERIFY
    expectHistorianCommand(...)   → Historian ON → VERIFY

Scenarios::SetStrategy(Descending)
    receiveStrategyChange(...)    → Stimulus     → zawsze wykonywany
    expectFactoryCreate(...)      → Factory ON   → VERIFY
    expectHistorianCommand(...)   → Historian OFF → SKIP
```

Jeden scenariusz. Różna topologia fixture. Zero modyfikacji scenariusza.

Dodatkowo poprzedni proposal zawierał:
- demonstracja pustego kontraktu w `LocalScenario` — brak expectations przy aktywnym kanale → "Unexpected signal"
- brak realnego `FactoryOnlyTest` (był oznaczony jako "hypothetical")

---

## 2. Architektura rozwiązania

```
SetUp() w EngineTestBase:

  h = dynamic_cast<HistorianSpy*>(this)
  f = dynamic_cast<FactorySpy*>(this)

  channels_ = ActiveChannels{ historian: h != nullptr,
                               factory:   f != nullptr }

  h != null → wire historian spy    |  null → wire NullHistorian
  f != null → wire factory spy      |  null → wire SortStrategyFactory

EngineDriver::run(scenario, channels_):

  for signal in scenario:
    if Stimulus → add to step
    if Expectation:
      channels_.isActive(sig.to) == false → SKIP (not "Signal not received")
      channels_.isActive(sig.to) == true  → add to step.expectations

  for step in steps:
    verifier_.setExpected(step.expectations)  ← tylko aktywne expectations
    execute stimulus
    verifier_.verifyComplete()
```

Przepływ informacji:

```
dynamic_cast
     │
     ▼
ActiveChannels
     │
     ├──► wiring Engine (SetUp)
     │
     └──► EngineDriver filtering (run)
               │
               ▼
         ScenarioVerifier
         (widzi tylko aktywne expectations)
```

---

## 3. Zmieniane pliki (4)

| Plik | Zmiana |
|------|--------|
| `scenario/Signal.hpp` | Dodanie `ActiveChannels` struct |
| `scenario/Spies.hpp` | Default ctor + `attachVerifier()` |
| `EngineDriver.hpp` | Parametr `ActiveChannels`; filtrowanie expectations w `run()` |
| `EngineComponentTest.cpp` | `EngineTestBase` ze `channels_`; fixture hierarchy; `FactoryOnlyTest`; fix `LocalScenario` |

Niezmieniane: `ScenarioVerifier.hpp`, `Scenarios.hpp`, `SequenceLog.hpp`, `CMakeLists.txt`

---

## 4. Proponowany `scenario/Signal.hpp` (pełny)

### Kod aktualny

```cpp
#pragma once
#include <any>
#include <functional>
#include <string>
#include "patterns/engine/Engine.hpp"

enum class Endpoint { Driver, Engine, Historian, Factory };
inline const char* endpointName(Endpoint e) noexcept { ... }

enum class SignalRole { Stimulus, Expectation };

struct Signal {
    SignalRole  role;
    std::string name;
    Endpoint    from;
    Endpoint    to;
    std::function<void(patterns::engine::Engine&)> action;
    std::function<bool(const std::any&)> payloadMatcher;
};

struct SignalDescriptor {
    Endpoint    from;
    Endpoint    to;
    std::string name;
    std::any    payload;
};
// ← brak ActiveChannels
```

### Proponowany kod

```cpp
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
// This enables the same pre-built scenario collection (e.g. Scenarios::SetStrategy)
// to work correctly across all fixture topologies without modification:
//
//   EngineComponentTest (both ON):  expectFactoryCreate  → VERIFY
//                                   expectHistorianCommand → VERIFY
//
//   HistorianOnlyTest  (factory OFF): expectFactoryCreate  → SKIP
//                                     expectHistorianCommand → VERIFY
//
//   FactoryOnlyTest    (historian OFF): expectFactoryCreate  → VERIFY
//                                       expectHistorianCommand → SKIP

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
```

### Zmiana

Dodanie `ActiveChannels` — prosta struct z dwoma flagami i metodą `isActive(Endpoint)`.
Wartości domyślne pól (`true`/`true`) służą wyłącznie do budowania obiektu przez
`EngineTestBase::SetUp()` przez agregat: `channels_ = {h != nullptr, f != nullptr}`.
`EngineDriver` nie ma domyślnego argumentu — `ActiveChannels` musi być zawsze jawne.

---

## 5. Proponowany `scenario/Spies.hpp` (pełny)

### Kod aktualny

```cpp
class HistorianSpy : public patterns::historian::IHistorian {
public:
    explicit HistorianSpy(ScenarioVerifier& verifier) : verifier_(verifier) {}
    //                    ↑ referencja w konstruktorze — nie default-constructible
private:
    ScenarioVerifier& verifier_;
};

class FactorySpy : public patterns::strategy::ISortStrategyFactory {
public:
    explicit FactorySpy(ScenarioVerifier& verifier) : verifier_(verifier) {}
    //                  ↑ referencja w konstruktorze — nie default-constructible
private:
    ScenarioVerifier& verifier_;
};
```

**Problem**: GTest konstruuje fixture przez default ctor. Klasy bazowe muszą być
default-constructible. Referencja w konstruktorze to uniemożliwia.

### Proponowany kod

```cpp
#pragma once
#include <any>
#include "patterns/historian/IHistorian.hpp"
#include "patterns/strategy/ISortStrategyFactory.hpp"
#include "patterns/strategy/SortStrategyFactory.hpp"
#include "ScenarioVerifier.hpp"

// ─── HistorianSpy ─────────────────────────────────────────────────────────────
// Reports every IHistorian call synchronously to ScenarioVerifier.
// Calls arriving outside an active step (armed_=false) are silently ignored.
//
// Default-constructible for use as a base class via multiple inheritance.
// EngineTestBase::SetUp() calls attachVerifier(verifier_) before any stimulus.

class HistorianSpy : public patterns::historian::IHistorian {
public:
    HistorianSpy() = default;

    void attachVerifier(ScenarioVerifier& v) { verifier_ = &v; }

    void recordCommand(const patterns::historian::CommandHistory& cmd) override {
        if (!verifier_) return;
        verifier_->report({
            .from    = Endpoint::Engine,
            .to      = Endpoint::Historian,
            .name    = "recordCommand",
            .payload = std::any{cmd}
        });
    }

    void publishSnapshot(const patterns::historian::EngineSnapshot& snap) override {
        if (!verifier_) return;
        verifier_->report({
            .from    = Endpoint::Engine,
            .to      = Endpoint::Historian,
            .name    = "publishSnapshot",
            .payload = std::any{snap}
        });
    }

private:
    ScenarioVerifier* verifier_ = nullptr;
};

// ─── FactorySpy ───────────────────────────────────────────────────────────────
// Reports every create() call to ScenarioVerifier.
// Delegates to real SortStrategyFactory so Engine gets a working strategy.
//
// Default-constructible for use as a base class via multiple inheritance.
// EngineTestBase::SetUp() calls attachVerifier(verifier_) before any stimulus.

class FactorySpy : public patterns::strategy::ISortStrategyFactory {
public:
    FactorySpy() = default;

    void attachVerifier(ScenarioVerifier& v) { verifier_ = &v; }

    [[nodiscard]] std::expected<std::unique_ptr<patterns::strategy::ISortStrategy>, std::string>
    create(patterns::strategy::SortStrategyId id) override {
        if (verifier_) {
            verifier_->report({
                .from    = Endpoint::Engine,
                .to      = Endpoint::Factory,
                .name    = "create",
                .payload = std::any{id}
            });
        }
        return real_.create(id);
    }

private:
    ScenarioVerifier*                       verifier_ = nullptr;
    patterns::strategy::SortStrategyFactory real_;
};
```

### Zmiany

| Zmiana | Powód |
|--------|-------|
| `verifier_` typ: `ScenarioVerifier&` → `ScenarioVerifier*` | Wskaźnik (nullable) umożliwia default construction |
| `attachVerifier(ScenarioVerifier& v)` | Wywoływane przez `EngineTestBase::SetUp()` po inicjalizacji wszystkich członków |
| Guard `if (!verifier_) return;` | Spy skonstruowany bez verifier'a jest bezpieczny — nie woła nullptr |
| `HistorianSpy() = default` | GTest wymaga default ctor dla klas fixture |

---

## 6. Proponowany `EngineDriver.hpp` (pełny)

### Kod aktualny

```cpp
class EngineDriver {
public:
    EngineDriver(patterns::engine::Engine& engine, ScenarioVerifier& verifier)
        : engine_(engine), verifier_(verifier) {}

    void run(const std::vector<Signal>& scenario) {
        // ...
        for (const auto& sig : scenario) {
            if (sig.role == SignalRole::Stimulus) {
                steps.push_back({&sig, {}});
            } else if (sig.role == SignalRole::Expectation) {
                if (steps.empty()) { ADD_FAILURE() << "Malformed scenario..."; return; }
                steps.back().expectations.push_back(sig);  // ← bez filtrowania
            }
        }
        // ...
    }

private:
    patterns::engine::Engine& engine_;
    ScenarioVerifier&          verifier_;
    // ← brak channels_
};
```

**Problem**: `expectFactoryCreate` w scenariuszu trafia do `step.expectations` nawet gdy
Factory channel jest wyłączony → ScenarioVerifier czeka na sygnał który nigdy nie przyjdzie
→ "Signal not received".

### Proponowany kod

```cpp
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
//   across different fixture topologies without modification.
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
        // Expectations for inactive channels are silently skipped.
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
```

### Zmiany

| Zmiana | Powód |
|--------|-------|
| Parametr `ActiveChannels channels` (bez domyślnej) | Topologia fixture musi być jawna — pominięcie to błąd kompilacji |
| `if (!channels_.isActive(sig.to)) continue;` | Kluczowa linia: pomija expectation zamiast ją przekazywać do verifier'a |
| `channels_` member | Przechowuje topologię na czas wykonania `run()` |

`ScenarioVerifier` pozostaje niezmieniony — nie wie nic o kanałach. Filtrowanie
dzieje się wyżej, w EngineDriver, przed wywołaniem `setExpected()`.

---

## 7. Proponowany `EngineComponentTest.cpp` (pełny)

### Strukturalne zmiany względem kodu aktualnego

| Aktualnie | Proponowane |
|-----------|-------------|
| `EngineComponentTest : public ::testing::Test` | `EngineTestBase : public ::testing::Test` (nowa baza) |
| Fixture posiada `shared_ptr<HistorianSpy>`, `shared_ptr<FactorySpy>` | Fixture JEST spy'em via dziedziczenie; brak member'ów spy |
| Oba spy'e zawsze podłączone | `dynamic_cast` → `ActiveChannels` → warunkowe podłączenie |
| `EngineDriver driver(*engine_, verifier_)` | `EngineDriver driver(*engine_, verifier_, channels_)` |
| Brak `TearDown()` | `TearDown()` resetuje `engine_` przed zniszczeniem sub-obiektów spy |
| Jedna klasa fixture | Hierarchia: `EngineComponentTest` + `HistorianOnlyTest` + `FactoryOnlyTest` |
| `LocalScenario` z duplikatem expectation | `LocalScenario` — pusty kontrakt: brak expectations, "Unexpected signal" |

### Proponowany kod

```cpp
#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>
#include <any>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "patterns/engine/Engine.hpp"
#include "patterns/historian/IHistorian.hpp"
#include "patterns/services/ServiceLocator.hpp"
#include "patterns/services/Logger.hpp"
#include "patterns/strategy/SortStrategyId.hpp"
#include "patterns/strategy/SortStrategyFactory.hpp"

#include "scenario/Signal.hpp"
#include "scenario/ScenarioVerifier.hpp"
#include "scenario/Spies.hpp"
#include "scenario/Scenarios.hpp"
#include "EngineDriver.hpp"

using namespace patterns::services;
using namespace patterns::strategy;
using namespace patterns::historian;

// ─── NullHistorian ────────────────────────────────────────────────────────────
// No-op IHistorian dla fixture'ów bez HistorianSpy.
// Zapobiega null-dereference gdy Engine woła historian a spy nie jest podłączony.

struct NullHistorian : patterns::historian::IHistorian {
    void recordCommand(const patterns::historian::CommandHistory&) override {}
    void publishSnapshot(const patterns::historian::EngineSnapshot&) override {}
};

// ═══════════════════════════════════════════════════════════════════════════════
// ScenarioFrameworkTest  [niezmieniony]
// ═══════════════════════════════════════════════════════════════════════════════

class ScenarioFrameworkTest : public ::testing::Test {
protected:
    ScenarioVerifier verifier_;

    static SignalDescriptor historianCommand(std::string name,
                                             std::vector<int> data = {}) {
        return {Endpoint::Engine, Endpoint::Historian, "recordCommand",
                std::any{CommandHistory{std::move(name), std::move(data)}}};
    }

    static SignalDescriptor historianSnapshot(size_t vectorCount = 0) {
        EngineSnapshot snap;
        snap.vectorCount = vectorCount;
        return {Endpoint::Engine, Endpoint::Historian, "publishSnapshot",
                std::any{snap}};
    }

    static SignalDescriptor factoryCreate(SortStrategyId id) {
        return {Endpoint::Engine, Endpoint::Factory, "create", std::any{id}};
    }
};

TEST_F(ScenarioFrameworkTest, UnexpectedSignal_NoExpectation) {
    verifier_.setExpected({});
    EXPECT_NONFATAL_FAILURE(
        verifier_.report(historianCommand("addVector")),
        "Unexpected signal"
    );
}

TEST_F(ScenarioFrameworkTest, SignalNotReceived) {
    verifier_.setExpected({expectHistorianCommand("addVector")});
    EXPECT_NONFATAL_FAILURE(
        verifier_.verifyComplete(),
        "Signal not received"
    );
}

TEST_F(ScenarioFrameworkTest, WrongOrder) {
    verifier_.setExpected({
        expectHistorianCommand("addVector"),
        expectHistorianSnapshot(),
    });
    EXPECT_NONFATAL_FAILURE(
        verifier_.report(historianSnapshot(0)),
        "Unexpected signal"
    );
}

TEST_F(ScenarioFrameworkTest, PayloadMismatch) {
    verifier_.setExpected({expectHistorianCommand("addVector", {{1, 2, 3}})});
    EXPECT_NONFATAL_FAILURE(
        verifier_.report(historianCommand("addVector", {9, 9, 9})),
        "payload mismatch"
    );
}

TEST_F(ScenarioFrameworkTest, SignalFromWrongStep_RegressionRev1) {
    verifier_.setExpected({expectHistorianCommand("addVector")});
    verifier_.report(historianCommand("addVector"));
    EXPECT_NONFATAL_FAILURE(
        verifier_.report(historianSnapshot(0)),
        "Unexpected signal"
    );
}

// ═══════════════════════════════════════════════════════════════════════════════
// EngineTestBase
// ─────────────────────────────────────────────────────────────────────────────
// Wspólna baza dla wszystkich fixture'ów komponentowych Engine.
//
// SetUp():
//   Wykrywa aktywne kanały przez dynamic_cast na typie konkretnego fixture.
//   Buduje ActiveChannels i przekazuje go do EngineDriver (przez members channels_).
//   Podłącza spy (jeśli aktywny) lub real/null implementację.
//
// Własność spy'ów:
//   Spy'e są sub-obiektami klasy pochodnej (ta sama przestrzeń adresowa co fixture).
//   Engine dostaje non-owning shared_ptr (no-op deleter) — nie będzie próbował
//   usunąć spy'a.
//   TearDown() niszczy Engine (engine_.reset()) zanim sub-obiekty spy zostaną
//   zniszczone przez destruktory klas bazowych.
// ═══════════════════════════════════════════════════════════════════════════════

class EngineTestBase : public ::testing::Test {
protected:
    void SetUp() override {
        [[maybe_unused]] auto r = ServiceLocator::instance().provide<Logger>(
            std::make_shared<Logger>());

        engine_ = std::make_shared<patterns::engine::Engine>();

        auto* h = dynamic_cast<HistorianSpy*>(this);
        auto* f = dynamic_cast<FactorySpy*>(this);

        // Wynik dynamic_cast definiuje topologię — zapisany w channels_
        // i użyty zarówno do wiring Engine jak i do filtrowania expectations w EngineDriver.
        channels_ = {h != nullptr, f != nullptr};

        // ── Historian channel ─────────────────────────────────────────────────
        // Engine przechowuje historian przez weak_ptr — control block musi żyć
        // przez cały czas trwania fixture. historianKeeper_ przedłuża to życie.
        if (h) {
            h->attachVerifier(verifier_);
            historianKeeper_ =
                std::shared_ptr<patterns::historian::IHistorian>(h, [](auto*){});
            engine_->setHistorian(historianKeeper_);
        } else {
            historianKeeper_ = std::make_shared<NullHistorian>();
            engine_->setHistorian(historianKeeper_);
        }

        // ── Factory channel ───────────────────────────────────────────────────
        // Engine przechowuje factory przez weak_ptr — control block musi żyć
        // przez cały czas trwania fixture. factoryKeeper_ przedłuża to życie.
        // setFactory woła wewnętrznie factory->create(Ascending).
        // verifier_ nie jest jeszcze uzbrojony (armed_=false) → call silently ignored.
        if (f) {
            f->attachVerifier(verifier_);
            factoryKeeper_ =
                std::shared_ptr<patterns::strategy::ISortStrategyFactory>(f, [](auto*){});
            engine_->setFactory(factoryKeeper_);
        } else {
            factoryKeeper_ =
                std::make_shared<patterns::strategy::SortStrategyFactory>();
            engine_->setFactory(factoryKeeper_);
        }
    }

    void TearDown() override {
        // Zniszcz Engine zanim keepery i sub-obiekty spy zostaną zniszczone.
        // Engine trzyma weak_ptr do collaboratorów — musi być zniszczony,
        // gdy control blocki keeperów są jeszcze żywe.
        engine_.reset();
    }

    ScenarioVerifier                                          verifier_;
    std::shared_ptr<patterns::engine::Engine>                 engine_;
    std::shared_ptr<patterns::historian::IHistorian>          historianKeeper_;
    std::shared_ptr<patterns::strategy::ISortStrategyFactory> factoryKeeper_;
    ActiveChannels                                            channels_;
};

// ═══════════════════════════════════════════════════════════════════════════════
// EngineComponentTest — oba kanały aktywne
// ─────────────────────────────────────────────────────────────────────────────
// dynamic_cast<HistorianSpy*>(this) → non-null → HistorianSpy podłączony
// dynamic_cast<FactorySpy*>(this)   → non-null → FactorySpy   podłączony
// channels_ = { historian: true, factory: true }
//
// Każde Engine→Historian i Engine→Factory wołanie musi być zadeklarowane
// w scenariuszu. Żaden sygnał nie zostanie pominięty przez filtr kanałów.
// ═══════════════════════════════════════════════════════════════════════════════

class EngineComponentTest : public EngineTestBase,
                             public HistorianSpy,
                             public FactorySpy {};

// ─── Framework self-check ─────────────────────────────────────────────────────

TEST_F(EngineComponentTest, MalformedScenario_ExpectationBeforeStimulus) {
    EngineDriver driver(*engine_, verifier_, channels_);
    EXPECT_NONFATAL_FAILURE(
        driver.run({
            expectHistorianCommand("addVector"),  // ← przed Stimulus
            receiveVectorAdded({1, 2, 3}),
        }),
        "Malformed scenario"
    );
}

// ─── Engine component contracts (oba kanały) ──────────────────────────────────

TEST_F(EngineComponentTest, AddVector) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::AddVector({1, 2, 3}));
}

TEST_F(EngineComponentTest, SortVector) {
    engine_->addVector({5, 3, 1});
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::SortVector(0));
}

TEST_F(EngineComponentTest, SetStrategy) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::SetStrategy(SortStrategyId::Descending));
}

TEST_F(EngineComponentTest, PublishSnapshot) {
    engine_->addVector({1, 2, 3});
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::PublishSnapshot(1));
}

TEST_F(EngineComponentTest, FullEngineFlow) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::FullEngineFlow());
}

/// Pusty kontrakt — brak expectations przy aktywnych kanałach (historian:ON, factory:ON).
// Stimulus wykonuje się normalnie. Engine woła historian.recordCommand().
// HistorianSpy raportuje do verifier'a → nextExpected_ >= expectations_.size()
// → "Unexpected signal" (nie ma żadnej oczekiwanej expectations).
//
// Semantyka: empty expectations ≠ "nie weryfikuj".
//   aktywny kanał + zero expectations = zero-tolerance na jakikolwiek sygnał z tego kanału.
TEST_F(EngineComponentTest, LocalScenario) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run({
        receiveVectorAdded({10, 20, 30}),
      //  expectHistorianCommand("addVector", {{10, 20, 30}}),
      //   expectHistorianCommand("addVector", {{10, 20, 30}}),
    });
}

// ═══════════════════════════════════════════════════════════════════════════════
// HistorianOnlyTest — tylko kanał Historian
// ─────────────────────────────────────────────────────────────────────────────
// dynamic_cast<HistorianSpy*>(this) → non-null → HistorianSpy podłączony
// dynamic_cast<FactorySpy*>(this)   → null     → real SortStrategyFactory podłączony
// channels_ = { historian: true, factory: false }
//
// EngineDriver filtruje expectations wg channels_:
//   expectHistorianCommand → VERIFY
//   expectHistorianSnapshot → VERIFY
//   expectFactoryCreate → SKIP (nie "Signal not received")
//
// Te same gotowe kolekcje (Scenarios::*) działają bez modyfikacji.
// ═══════════════════════════════════════════════════════════════════════════════

class HistorianOnlyTest : public EngineTestBase, public HistorianSpy {};

// NEW 1
TEST_F(HistorianOnlyTest, AddVector) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::AddVector({1, 2, 3}));
}

// NEW 2
TEST_F(HistorianOnlyTest, SortVector) {
    engine_->addVector({5, 3, 1});
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::SortVector(0));
}

// NEW 3
// Scenarios::SetStrategy zawiera expectFactoryCreate + expectHistorianCommand.
// channels_.factory=false → expectFactoryCreate SKIP.
// Weryfikowany tylko: expectHistorianCommand("setSortStrategy").
// Ten sam Scenarios::SetStrategy co w EngineComponentTest — bez modyfikacji.
TEST_F(HistorianOnlyTest, SetStrategy) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::SetStrategy(SortStrategyId::Descending));
}

// NEW 4
TEST_F(HistorianOnlyTest, PublishSnapshot) {
    engine_->addVector({1, 2, 3});
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::PublishSnapshot(1));
}

// NEW 5
// Scenarios::FullEngineFlow zawiera wszystkie expectFactory* i expectHistorian*.
// channels_.factory=false → wszystkie expectFactoryCreate SKIP.
// Weryfikowane: expectHistorianCommand x3, expectHistorianSnapshot x1.
// Ten sam Scenarios::FullEngineFlow co w EngineComponentTest — bez modyfikacji.
TEST_F(HistorianOnlyTest, FullEngineFlow) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::FullEngineFlow());
}

// ═══════════════════════════════════════════════════════════════════════════════
// FactoryOnlyTest — tylko kanał Factory
// ─────────────────────────────────────────────────────────────────────────────
// dynamic_cast<HistorianSpy*>(this) → null     → NullHistorian podłączony
// dynamic_cast<FactorySpy*>(this)   → non-null → FactorySpy podłączony
// channels_ = { historian: false, factory: true }
//
// EngineDriver filtruje expectations wg channels_:
//   expectFactoryCreate → VERIFY
//   expectHistorianCommand → SKIP (nie "Signal not received")
//   expectHistorianSnapshot → SKIP
//
// Te same gotowe kolekcje (Scenarios::*) działają bez modyfikacji.
// ═══════════════════════════════════════════════════════════════════════════════

class FactoryOnlyTest : public EngineTestBase, public FactorySpy {};

// NEW 6
// Scenarios::SetStrategy zawiera expectFactoryCreate + expectHistorianCommand.
// channels_.historian=false → expectHistorianCommand SKIP.
// Weryfikowany tylko: expectFactoryCreate(Descending).
// Ten sam Scenarios::SetStrategy co w EngineComponentTest — bez modyfikacji.
TEST_F(FactoryOnlyTest, SetStrategy) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::SetStrategy(SortStrategyId::Descending));
}

// NEW 7
// Scenarios::FullEngineFlow zawiera wszystkie expectHistorian* i expectFactory*.
// channels_.historian=false → wszystkie expectHistorian* SKIP.
// Weryfikowane: expectFactoryCreate x1 (SetStrategy(Descending) wewnątrz FullEngineFlow).
// Początkowe create(Ascending) w SetUp() dzieje się przy nieuzbrojonym verifierze → ignorowane.
// AddVector, SortVector, PublishSnapshot steps mają puste expectations po filtrowaniu —
// verifier_.setExpected({}) + verifyComplete() → pass trivially.
TEST_F(FactoryOnlyTest, FullEngineFlow) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::FullEngineFlow());
}
```

---

## 8. Diagram przepływu — ten sam scenariusz, różne topologie

```
Scenarios::SetStrategy(Descending):
  [receiveStrategyChange, expectFactoryCreate, expectHistorianCommand]

                    EngineComponentTest         HistorianOnlyTest         FactoryOnlyTest
                    Hist:ON  Fact:ON             Hist:ON  Fact:OFF         Hist:OFF  Fact:ON
                    ─────────────────           ─────────────────         ─────────────────
receiveStrategyChange  Stimulus ✓              Stimulus ✓                Stimulus ✓
expectFactoryCreate    VERIFY   ✓              SKIP     –                VERIFY   ✓
expectHistorianCommand VERIFY   ✓              VERIFY   ✓                SKIP     –
```

---

## 9. Macierz testów

| Fixture | Test | Nowy? | Hist | Fact |
|---------|------|-------|------|------|
| ScenarioFrameworkTest | UnexpectedSignal_NoExpectation | — | direct | direct |
| ScenarioFrameworkTest | SignalNotReceived | — | direct | direct |
| ScenarioFrameworkTest | WrongOrder | — | direct | direct |
| ScenarioFrameworkTest | PayloadMismatch | — | direct | direct |
| ScenarioFrameworkTest | SignalFromWrongStep_RegressionRev1 | — | direct | direct |
| EngineComponentTest | MalformedScenario_ExpectationBeforeStimulus | — | ON | ON |
| EngineComponentTest | AddVector | — | ON | ON |
| EngineComponentTest | SortVector | — | ON | ON |
| EngineComponentTest | SetStrategy | — | ON | ON |
| EngineComponentTest | PublishSnapshot | — | ON | ON |
| EngineComponentTest | FullEngineFlow | — | ON | ON |
| EngineComponentTest | LocalScenario (empty contract → Unexpected signal) | demo | ON | ON |
| **HistorianOnlyTest** | **AddVector** | **NEW 1** | ON | OFF |
| **HistorianOnlyTest** | **SortVector** | **NEW 2** | ON | OFF |
| **HistorianOnlyTest** | **SetStrategy** | **NEW 3** | ON | OFF |
| **HistorianOnlyTest** | **PublishSnapshot** | **NEW 4** | ON | OFF |
| **HistorianOnlyTest** | **FullEngineFlow** | **NEW 5** | ON | OFF |
| **FactoryOnlyTest** | **SetStrategy** | **NEW 6** | OFF | ON |
| **FactoryOnlyTest** | **FullEngineFlow** | **NEW 7** | OFF | ON |

Łącznie: 19 testów (11 istniejących + 1 fix + 7 nowych).

---

## 10. Ryzyka i ograniczenia

### 10.1 Lifetime collaboratorów — weak_ptr i keepery

**Problem**: Engine przechowuje historian i factory przez `weak_ptr`. Jeżeli żaden
`shared_ptr` nie trzyma control blocka przy życiu, `weak_ptr` natychmiast staje się
expired, a Engine nie może już wołać collaboratora.

**Stare podejście (błąd)**:
```cpp
engine_->setHistorian(
    std::shared_ptr<IHistorian>(h, [](auto*){}));  // tymczasowy — control block znika ↑
// Po tej linii weak_ptr w Engine jest already expired!
```

**Poprawne podejście — keepery jako členy**:
```cpp
historianKeeper_ = std::shared_ptr<IHistorian>(h, [](auto*){});
engine_->setHistorian(historianKeeper_);  // weak_ptr żyje póki historianKeeper_ żyje ✓
```

`historianKeeper_` i `factoryKeeper_` są memberami `EngineTestBase` — żyją przez
cały czas trwania fixture (aż do destrukcji `EngineTestBase`).

**Porządek destrukcji** (dla `EngineComponentTest`):
```
1. TearDown()             → engine_.reset() → Engine zniszczony ← keepery i spye żyją ✓
2. ~EngineComponentTest() (trivial)
3. ~FactorySpy()
4. ~HistorianSpy()
5. ~EngineTestBase()      → historianKeeper_.reset(), factoryKeeper_.reset(),
                             engine_ już null → brak double-destroy ✓
```

`TearDown()` niszczy Engine (step 1) gdy keepery wciąż żyją (niszczone w step 5).
Gwarantuje to, że w żadnym momencie Engine nie trzyma expired weak_ptr do żywego
sub-obiektu spy — ani żaden destruktor nie woła spy'a po jego zniszczeniu.

### 10.2 Brak domyślnego argumentu `channels` — wymuszenie przez API

Konstruktor `EngineDriver` nie ma domyślnej wartości dla `channels`:

```cpp
EngineDriver(Engine& engine, ScenarioVerifier& verifier, ActiveChannels channels)
```

Pominięcie trzeciego argumentu to błąd kompilacji, nie cicha pomyłka runtime.
Gdyby `channels = {}` (domyślnie oba ON), w `HistorianOnlyTest` wystarczyłoby napisać:

```cpp
EngineDriver driver(*engine_, verifier_);  // zapomniany channels_
```

i `expectFactoryCreate` przestałoby być filtrowane → fałszywe "Signal not received".
Jawny `channels_` z `EngineTestBase` eliminuje tę klasę błędów.

### 10.3 `dynamic_cast` wymaga polymorphic base

`EngineTestBase : public ::testing::Test` → `::testing::Test` ma virtual destructor → typ
polimorficzny → `dynamic_cast` działa. ✓

### 10.4 Brak diamond inheritance

```
EngineComponentTest : EngineTestBase, HistorianSpy, FactorySpy
  EngineTestBase : ::testing::Test
  HistorianSpy   : IHistorian
  FactorySpy     : ISortStrategyFactory
```

`HistorianSpy` i `FactorySpy` nie dziedziczą po `EngineTestBase` ani po sobie. Brak diamentu. ✓

### 10.5 Wywołanie `create(Ascending)` w SetUp przed uzbrojeniem verifier'a

Gdy `FactorySpy` jest aktywny: `attachVerifier()` wywoływane PRZED `setFactory()`.
`setFactory()` woła `spy->create(Ascending)` → spy raportuje do verifier'a.
`armed_=false` → call silently ignored. ✓

### 10.6 Kroki z pustymi expectations (FactoryOnlyTest)

Gdy `channels_.historian=false`, kroki `AddVector`, `SortVector`, `PublishSnapshot`
mają puste expectations po filtrowaniu. `verifier_.setExpected({})` + `verifyComplete()`
bez żadnych przychodzących sygnałów → pass trivially. ✓
