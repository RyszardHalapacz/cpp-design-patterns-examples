# PROPOSAL IMPLEMENTACYJNY: Typed Endpoint API

**Status:** draft
**Data:** 2026-08-27
**Baza:** PROPOSAL_typed_endpoint_api.md (zaakceptowany)

---

## 1. Zakres

**W zakresie:**
- `ScenarioExecutor` — nowy koordynator kroków (zastępuje `EngineDriver` jako silnik)
- `EngineEndpoint`, `HistorianEndpoint`, `FactoryEndpoint` — typed test handles
- Rozszerzenie `HistorianSpy` / `FactorySpy` o publiczne endpointy
- Rozszerzenie `EngineTestBase` o `engine` member i `executor_`
- Nowe regression testy dla nowego API
- Migracja istniejących testów `EngineComponentTest` / `HistorianOnlyTest` / `FactoryOnlyTest`

**Poza zakresem:**
- `engine.send()` — przyszłościowe rozszerzenie, nie implementowane teraz
- Nowe komponenty Engine (GuiSpy itp.)
- Zmiany w produkcyjnym kodzie Engine
- `ScenarioVerifier` — bez zmian
- `SequenceLog` — bez zmian
- `Signal` / `SignalDescriptor` / `ActiveChannels` — bez zmian

---

## 2. Plan plików

### Nowe pliki

| Plik | Zawartość |
|------|-----------|
| `scenario/ScenarioExecutor.hpp` | `class ScenarioExecutor` |
| `scenario/EngineEndpoint.hpp` | `class EngineEndpoint` |
| `scenario/HistorianEndpoint.hpp` | `class HistorianEndpoint` |
| `scenario/FactoryEndpoint.hpp` | `class FactoryEndpoint` |

### Modyfikowane pliki

| Plik | Co się zmienia |
|------|----------------|
| `scenario/Spies.hpp` | `HistorianSpy` += `historian`; `FactorySpy` += `factory` |
| `EngineComponentTest.cpp` | `EngineTestBase` += `engine`, `executor_`; SetUp/TearDown; nowe testy |

### Pliki niezmienione

`ScenarioVerifier.hpp`, `SequenceLog.hpp`, `Signal.hpp`, `Scenarios.hpp`, `EngineDriver.hpp`
(backward compat — stare testy nadal kompilują się i przechodzą przez całą migrację)

### CMakeLists.txt

Brak zmian — nowe `.hpp` są header-only, nie wymagają rejestracji.

---

## 3. Kolejność implementacji

```
Faza 1  ScenarioExecutor.hpp          — niezależna od endpointów
Faza 2  HistorianEndpoint.hpp         — zależy od ScenarioExecutor, Signal, IHistorian
        FactoryEndpoint.hpp           — zależy od ScenarioExecutor, Signal, ISortStrategyFactory
        EngineEndpoint.hpp            — zależy od ScenarioExecutor, Signal, Engine
Faza 3  Spies.hpp                     — dodanie memberów historian / factory
Faza 4  EngineComponentTest.cpp       — EngineTestBase: engine + executor_; SetUp/TearDown
Faza 5  EngineComponentTest.cpp       — nowe regression testy (EndpointApiTest)
Faza 6  EngineComponentTest.cpp       — migracja istniejących testów
```

Każda faza kończy się kompilującym się kodem. `ScenarioFrameworkTest` przechodzi przez
wszystkie fazy bez modyfikacji (ScenarioVerifier niezmieniony).

---

## 4. Faza 1: `ScenarioExecutor`

### Plik: `scenario/ScenarioExecutor.hpp`

`ScenarioExecutor` to `EngineDriver` z nowym publicznym API.
Wewnętrznie identyczna logika: `stepOpen_`, `beginStep()`, `finalizeStep()`.

```cpp
#pragma once
#include <functional>
#include <string>
#include <gtest/gtest.h>
#include "patterns/engine/Engine.hpp"
#include "Signal.hpp"
#include "SequenceLog.hpp"
#include "ScenarioVerifier.hpp"

// ─── ScenarioExecutor ─────────────────────────────────────────────────────────
// Koordynuje kroki scenariusza; obsługiwany przez endpointy.
//
// executeStimulus() — wywołany przez EngineEndpoint::receive()
//   Zamyka poprzedni krok (jeśli otwarty), otwiera nowy, wykonuje akcję,
//   kończy zbieranie actuals, loguje stimulus do SequenceLog.
//
// declareExpectation() — wywołany przez HistorianEndpoint::receive() / FactoryEndpoint::receive()
//   Jeśli krok jest otwarty i kanał jest aktywny: matchExpectation().
//   Jeśli krok nie jest otwarty: ADD_FAILURE (Expectation before stimulus).
//   Jeśli kanał nieaktywny: cicho pomija (identycznie jak EngineDriver).
//
// ~ScenarioExecutor() — finalizuje oczekujący krok.
//   Musi być wywołany przez TearDown() PRZED zniszczeniem spy sub-objektów.

class ScenarioExecutor {
public:
    ScenarioExecutor(ScenarioVerifier& verifier, ActiveChannels channels)
        : verifier_(verifier), channels_(channels) {}

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

    // Nie copyable / nie moveable — trzyma referencje.
    ScenarioExecutor(const ScenarioExecutor&)            = delete;
    ScenarioExecutor& operator=(const ScenarioExecutor&) = delete;

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
        // pendingRows_ zostaną sflushowane przy kolejnym executeStimulus() lub w destruktorze.
    }

private:
    ScenarioVerifier& verifier_;
    ActiveChannels    channels_;
    bool              stepOpen_ = false;
};
```

**Niezmienność:** `ScenarioVerifier::beginStep()` ma TODO guard (patrz istniejący komentarz).
`ScenarioExecutor` nigdy nie wywołuje `beginStep()` dwukrotnie bez `finalizeStep()` pomiędzy
— wynika to z `stepOpen_` flagi.

---

## 5. Faza 2: Endpointy

### Plik: `scenario/HistorianEndpoint.hpp`

```cpp
#pragma once
#include <any>
#include <optional>
#include <vector>
#include "Signal.hpp"
#include "ScenarioExecutor.hpp"
#include "patterns/historian/IHistorian.hpp"

// ─── HistorianEndpoint ────────────────────────────────────────────────────────
// Typed test handle dla kanału Historian.
// Publiczny member HistorianSpy — dostępny w TEST_F przez dziedziczenie.
//
// Metody builderów (addVector, sortVector, setSortStrategy, publishSnapshot)
// produkują Signal z payloadMatcher — są odpowiednikiem dotychczasowych
// expectHistorianCommand() / expectHistorianSnapshot() z Scenarios.hpp,
// ale bez stringów w publicznym API.
//
// Wewnętrzne stringi ("recordCommand", "addVector" itp.) są szczegółem
// implementacyjnym tego adaptera — scentralizowane, nie powielane w testach.
//
// receive(sig) deleguje do ScenarioExecutor::declareExpectation().

class HistorianEndpoint {
public:
    // ── Deskryptory expectations — domain-level ───────────────────────────────

    [[nodiscard]] Signal addVector(std::vector<int> data) const {
        return makeRecordCommand("addVector",
            [data](const patterns::historian::CommandHistory& cmd) {
                return cmd.commandName == "addVector" && cmd.data == data;
            });
    }

    [[nodiscard]] Signal sortVector() const {
        return makeRecordCommand("sortVector",
            [](const patterns::historian::CommandHistory& cmd) {
                return cmd.commandName == "sortVector";
            });
    }

    [[nodiscard]] Signal setSortStrategy() const {
        return makeRecordCommand("setSortStrategy",
            [](const patterns::historian::CommandHistory& cmd) {
                return cmd.commandName == "setSortStrategy";
            });
    }

    [[nodiscard]] Signal publishSnapshot(std::optional<size_t> vectorCount = {}) const {
        return {
            .role   = SignalRole::Expectation,
            .name   = "publishSnapshot",
            .from   = Endpoint::Engine,
            .to     = Endpoint::Historian,
            .payloadMatcher = [vectorCount](const std::any& payload) -> bool {
                const auto* snap =
                    std::any_cast<patterns::historian::EngineSnapshot>(&payload);
                if (!snap) return false;
                if (vectorCount && snap->vectorCount != *vectorCount) return false;
                return true;
            }
        };
    }

    // ── Deklaracja expectation ────────────────────────────────────────────────

    void receive(Signal sig) {
        executor_->declareExpectation(std::move(sig));
    }

    // Wywoływane przez EngineTestBase::SetUp().
    void attach(ScenarioExecutor& ex) { executor_ = &ex; }

private:
    Signal makeRecordCommand(
            const std::string& commandName,
            std::function<bool(const patterns::historian::CommandHistory&)> matcher) const
    {
        return {
            .role   = SignalRole::Expectation,
            .name   = "recordCommand",
            .from   = Endpoint::Engine,
            .to     = Endpoint::Historian,
            .payloadMatcher = [m = std::move(matcher)](const std::any& payload) -> bool {
                const auto* cmd =
                    std::any_cast<patterns::historian::CommandHistory>(&payload);
                return cmd && m(*cmd);
            }
        };
    }

    ScenarioExecutor* executor_ = nullptr;
};
```

### Plik: `scenario/FactoryEndpoint.hpp`

```cpp
#pragma once
#include <any>
#include "Signal.hpp"
#include "ScenarioExecutor.hpp"
#include "patterns/strategy/ISortStrategyFactory.hpp"

// ─── FactoryEndpoint ──────────────────────────────────────────────────────────
// Typed test handle dla kanału Factory.
// Publiczny member FactorySpy — dostępny w TEST_F przez dziedziczenie.

class FactoryEndpoint {
public:
    [[nodiscard]] Signal create(patterns::strategy::SortStrategyId id) const {
        return {
            .role   = SignalRole::Expectation,
            .name   = "create",
            .from   = Endpoint::Engine,
            .to     = Endpoint::Factory,
            .payloadMatcher = [id](const std::any& payload) -> bool {
                const auto* rid =
                    std::any_cast<patterns::strategy::SortStrategyId>(&payload);
                return rid && *rid == id;
            }
        };
    }

    void receive(Signal sig) {
        executor_->declareExpectation(std::move(sig));
    }

    void attach(ScenarioExecutor& ex) { executor_ = &ex; }

private:
    ScenarioExecutor* executor_ = nullptr;
};
```

### Plik: `scenario/EngineEndpoint.hpp`

```cpp
#pragma once
#include <functional>
#include <string>
#include <vector>
#include "Signal.hpp"
#include "ScenarioExecutor.hpp"
#include "patterns/engine/Engine.hpp"
#include "patterns/observer/SessionEvent.hpp"
#include "patterns/strategy/SortStrategyId.hpp"

// ─── EngineEndpoint ───────────────────────────────────────────────────────────
// Typed test handle dla kanału Engine.
// Publiczny member EngineTestBase — dostępny we wszystkich TEST_F.
//
// Metody builderów (addVector, sortVector, strategyChange, publishSnapshot)
// produkują Descriptor z lambdą akcji. receive(d) przekazuje akcję do
// ScenarioExecutor::executeStimulus(), który zarządza cyklem życia kroku.
//
// Nadawcą logicznym jest zawsze Endpoint::Driver — Test inicjuje wywołanie.

class EngineEndpoint {
public:
    // ── Descriptor — akcja do wykonania przeciwko Engine ─────────────────────

    struct Descriptor {
        std::string                                      name;
        std::function<void(patterns::engine::Engine&)>   action;
    };

    // ── Deskryptory stimulus — domain-level ───────────────────────────────────

    [[nodiscard]] Descriptor addVector(std::vector<int> data) const {
        return {"onSessionEvent(addVector)",
            [data](patterns::engine::Engine& e) {
                patterns::observer::SessionEvent ev;
                ev.type       = patterns::observer::SessionEventType::VectorAdded;
                ev.vectorData = data;
                e.onSessionEvent(ev);
            }};
    }

    [[nodiscard]] Descriptor sortVector(size_t index = 0) const {
        return {"onSessionEvent(sortVector)",
            [index](patterns::engine::Engine& e) {
                patterns::observer::SessionEvent ev;
                ev.type  = patterns::observer::SessionEventType::SortRequested;
                ev.index = index;
                e.onSessionEvent(ev);
            }};
    }

    [[nodiscard]] Descriptor strategyChange(patterns::strategy::SortStrategyId id) const {
        return {"onSessionEvent(strategyChange)",
            [id](patterns::engine::Engine& e) {
                patterns::observer::SessionEvent ev;
                ev.type       = patterns::observer::SessionEventType::StrategyChangeRequested;
                ev.strategyId = id;
                e.onSessionEvent(ev);
            }};
    }

    [[nodiscard]] Descriptor publishSnapshot() const {
        return {"publishSnapshot",
            [](patterns::engine::Engine& e) { e.publishSnapshot(); }};
    }

    // ── Wykonanie stimulus ────────────────────────────────────────────────────

    void receive(Descriptor d) {
        executor_->executeStimulus(
            Endpoint::Driver, Endpoint::Engine, d.name, d.action, *engine_);
    }

    // Wywoływane przez EngineTestBase::SetUp().
    void attach(ScenarioExecutor& ex, patterns::engine::Engine& eng) {
        executor_ = &ex;
        engine_   = &eng;
    }

private:
    ScenarioExecutor*         executor_ = nullptr;
    patterns::engine::Engine* engine_   = nullptr;
};
```

---

## 6. Faza 3: Rozszerzenie Spies

### Plik: `scenario/Spies.hpp` — diff semantyczny

**`HistorianSpy`** — dodać:
1. `#include "HistorianEndpoint.hpp"`
2. Publiczny member `HistorianEndpoint historian;`
3. W metodzie `attachVerifier()` — dodać parametr `ScenarioExecutor&` **lub** dodać
   osobną metodę `attachEndpoint(ScenarioExecutor& ex)`.

Wybór: **osobna metoda** — nie miesza dwóch odpowiedzialności.

```cpp
// Spies.hpp — sekcja HistorianSpy (pełna po zmianach)
#pragma once
#include <any>
#include "patterns/historian/IHistorian.hpp"
#include "patterns/strategy/ISortStrategyFactory.hpp"
#include "patterns/strategy/SortStrategyFactory.hpp"
#include "HistorianEndpoint.hpp"
#include "FactoryEndpoint.hpp"
#include "ScenarioVerifier.hpp"

class HistorianSpy : public patterns::historian::IHistorian {
public:
    HistorianEndpoint historian;   // ← NOWE: typed test handle

    void attachVerifier(ScenarioVerifier& v) { spyVerifier_ = &v; }
    void attachEndpoint(ScenarioExecutor& ex) { historian.attach(ex); }  // ← NOWE

    void recordCommand(const patterns::historian::CommandHistory& cmd) override {
        if (!spyVerifier_) return;
        spyVerifier_->report({
            .from    = Endpoint::Engine,
            .to      = Endpoint::Historian,
            .name    = "recordCommand",
            .payload = std::any{cmd}
        });
    }

    void publishSnapshot(const patterns::historian::EngineSnapshot& snap) override {
        if (!spyVerifier_) return;
        spyVerifier_->report({
            .from    = Endpoint::Engine,
            .to      = Endpoint::Historian,
            .name    = "publishSnapshot",
            .payload = std::any{snap}
        });
    }

private:
    ScenarioVerifier* spyVerifier_ = nullptr;
};

class FactorySpy : public patterns::strategy::ISortStrategyFactory {
public:
    FactoryEndpoint factory;   // ← NOWE: typed test handle

    void attachVerifier(ScenarioVerifier& v) { spyVerifier_ = &v; }
    void attachEndpoint(ScenarioExecutor& ex) { factory.attach(ex); }  // ← NOWE

    [[nodiscard]] std::expected<std::unique_ptr<patterns::strategy::ISortStrategy>, std::string>
    create(patterns::strategy::SortStrategyId id) override {
        if (spyVerifier_) {
            spyVerifier_->report({
                .from    = Endpoint::Engine,
                .to      = Endpoint::Factory,
                .name    = "create",
                .payload = std::any{id}
            });
        }
        return real_.create(id);
    }

private:
    ScenarioVerifier*                       spyVerifier_ = nullptr;
    patterns::strategy::SortStrategyFactory real_;
};
```

---

## 7. Faza 4: Rozszerzenie `EngineTestBase`

### Sekcja w `EngineComponentTest.cpp`

Zmiany względem obecnego kodu:
1. `#include` nowych plików nagłówkowych
2. Publiczny member `EngineEndpoint engine`
3. Prywatny member `std::unique_ptr<ScenarioExecutor> executor_`
4. `SetUp()` — tworzenie `executor_`, wiring endpointów
5. `TearDown()` — reset w kolejności: engine_, executor_

```cpp
// ─── EngineTestBase ───────────────────────────────────────────────────────────
// [nowe includy]
#include "scenario/ScenarioExecutor.hpp"
#include "scenario/EngineEndpoint.hpp"

class EngineTestBase : public ::testing::Test {
protected:
    // ── Publiczne test API — dostępne bezpośrednio w TEST_F ───────────────────
    // historian i factory są dostępne gdy fixture dziedziczy po HistorianSpy / FactorySpy.
    EngineEndpoint engine;

    void SetUp() override {
        [[maybe_unused]] auto r = ServiceLocator::instance().provide<Logger>(
            std::make_shared<Logger>());

        engine_ = std::make_shared<patterns::engine::Engine>();

        auto* h = dynamic_cast<HistorianSpy*>(this);
        auto* f = dynamic_cast<FactorySpy*>(this);
        channels_ = {h != nullptr, f != nullptr};

        // Executor musi powstać po wykryciu kanałów.
        executor_ = std::make_unique<ScenarioExecutor>(verifier_, channels_);

        // Engine endpoint — wiring.
        engine.attach(*executor_, *engine_);

        // ── Historian channel ─────────────────────────────────────────────────
        if (h) {
            h->attachVerifier(verifier_);
            h->attachEndpoint(*executor_);   // ← NOWE: podłącz historian endpoint
            historianKeeper_ =
                std::shared_ptr<patterns::historian::IHistorian>(h, [](auto*){});
            engine_->setHistorian(historianKeeper_);
        } else {
            historianKeeper_ = std::make_shared<NullHistorian>();
            engine_->setHistorian(historianKeeper_);
        }

        // ── Factory channel ───────────────────────────────────────────────────
        if (f) {
            f->attachVerifier(verifier_);
            f->attachEndpoint(*executor_);   // ← NOWE: podłącz factory endpoint
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
        // Kolejność jest krytyczna dla poprawności lifetime'ów:
        //
        // 1. engine_.reset() — niszczy Engine (nie może już wołać spy przez weak_ptr).
        // 2. executor_->finalize() — jawna finalizacja kroku; failures zgłaszane tu,
        //    nie w destruktorze. Bezpieczne: accesses only verifier_, nie spy.
        //    Musi być przed zniszczeniem spy sub-objectów.
        // 3. executor_.reset() — niszczy executor (destruktor jest no-op: finalized_=true).
        // 4. spy sub-obiekty (HistorianSpy, FactorySpy) — niszczone przez C++ destruktory
        //    baz po TearDown().
        // 5. verifier_, engine_, executor_ — niszczone przez destruktor EngineTestBase.
        engine_.reset();
        executor_->finalize();
        executor_.reset();
    }

    ScenarioVerifier                                          verifier_;
    std::shared_ptr<patterns::engine::Engine>                 engine_;
    std::unique_ptr<ScenarioExecutor>                         executor_;
    std::shared_ptr<patterns::historian::IHistorian>          historianKeeper_;
    std::shared_ptr<patterns::strategy::ISortStrategyFactory> factoryKeeper_;
    ActiveChannels                                            channels_;
};
```

### Dlaczego `executor_.reset()` w TearDown, nie w destruktorze

Destruktor `EngineTestBase` wywołany jest PO destruktorach baz pochodnych (`HistorianSpy`,
`FactorySpy`). Gdyby `executor_` żył do destruktora `EngineTestBase`, jego `~ScenarioExecutor()`
wywołałby `finalizeStep()` po zniszczeniu spy. Spy nie jest wywoływany (actuals już
zbuforowane), ale `verifier_.flushDiagramRows()` woła `SequenceLog::logFlow` — bezpieczne.
Niemniej `executor_.reset()` w `TearDown()` jest jawny i nie zależy od wiedzy o kolejności
destrukcji. Zasada: im mniej wiedzy o kolejności destrukcji, tym lepiej.

---

## 8. Faza 5: Nowe regression testy

Dodać do `EngineComponentTest.cpp` przed sekcją `EngineComponentTest`:

```cpp
// ═══════════════════════════════════════════════════════════════════════════════
// EndpointApiTest — regression testy nowego API endpointów
// ─────────────────────────────────────────────────────────────────────────────
// Weryfikują zachowanie ScenarioExecutor + endpointów bezpośrednio,
// używając żywego Engine (jak EngineComponentTest, ale skupione na semantyce API).
// ScenarioFrameworkTest pokrywa ScenarioVerifier — te testy pokrywają warstwę nad nim.
// ═══════════════════════════════════════════════════════════════════════════════

class EndpointApiTest : public EngineTestBase,
                        public HistorianSpy,
                        public FactorySpy {};

// Expectation przed pierwszym stimulus → "Expectation ... declared before any stimulus".
TEST_F(EndpointApiTest, ExpectationBeforeStimulus)
{
    EXPECT_NONFATAL_FAILURE(
        historian.receive(historian.addVector({1, 2, 3})),
        "Expectation"
    );
}

// Stimulus bez expectations → "Unexpected signal" przy kolejnym stimulus.
TEST_F(EndpointApiTest, UnexpectedSignal_NoExpectation)
{
    EXPECT_NONFATAL_FAILURE(
        [&]() {
            engine.receive(engine.addVector({1, 2, 3}));
            // brak historian.receive() → przy następnym receive/destruktorze: Unexpected signal
            engine.receive(engine.addVector({1, 2, 3}));
        }(),
        "Unexpected signal"
    );
}

// Expectation bez actuals → "Signal not received".
TEST_F(EndpointApiTest, SignalNotReceived)
{
    engine.receive(engine.addVector({1, 2, 3}));
    EXPECT_NONFATAL_FAILURE(
        historian.receive(historian.sortVector()),   // addVector ≠ sortVector
        "Unexpected signal"
    );
}

// Payload mismatch → "payload mismatch".
TEST_F(EndpointApiTest, PayloadMismatch)
{
    engine.receive(engine.addVector({1, 2, 3}));
    EXPECT_NONFATAL_FAILURE(
        historian.receive(historian.addVector({9, 9, 9})),
        "payload mismatch"
    );
}

// Expectations w złej kolejności → "Unexpected signal".
TEST_F(EndpointApiTest, WrongOrder)
{
    engine.receive(engine.strategyChange(SortStrategyId::Descending));
    EXPECT_NONFATAL_FAILURE(
        historian.receive(historian.setSortStrategy()),  // historian przed factory — wrong order
        "Unexpected signal"
    );
}

// Wiele kroków w jednym TEST_F — poprawna izolacja między krokami.
TEST_F(EndpointApiTest, MultiStep_StepsAreIsolated)
{
    engine.receive(engine.addVector({1, 2, 3}));
    historian.receive(historian.addVector({1, 2, 3}));

    engine.receive(engine.strategyChange(SortStrategyId::Descending));
    factory.receive(factory.create(SortStrategyId::Descending));
    historian.receive(historian.setSortStrategy());
}

// Drugi engine.receive() zamyka poprzedni krok i raportuje brakujące expectations.
TEST_F(EndpointApiTest, SecondStimulus_ClosesPreviousStep)
{
    engine.receive(engine.addVector({1, 2, 3}));
    // Brak historian.receive() — step 1 jest "open" z nieodebranymi actuals.
    EXPECT_NONFATAL_FAILURE(
        engine.receive(engine.addVector({1, 2, 3})),  // zamknięcie step 1
        "Unexpected signal"
    );
    historian.receive(historian.addVector({1, 2, 3}));
}
```

---

## 9. Faza 6: Migracja istniejących testów

Migracja odbywa się test po teście. Stary `EngineDriver` / `Scenarios::*` kompilują się
przez cały czas migracji (backward compat). Poniżej wzorzec migracji dla każdego testu.

### `EngineComponentTest::AddVector`

```cpp
// STARE:
TEST_F(EngineComponentTest, AddVector) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::AddVector({1, 2, 3}));
}

// NOWE:
TEST_F(EngineComponentTest, AddVector) {
    engine.receive(engine.addVector({1, 2, 3}));
    historian.receive(historian.addVector({1, 2, 3}));
}
```

### `EngineComponentTest::SortVector`

```cpp
// STARE:
TEST_F(EngineComponentTest, SortVector) {
    engine_->addVector({5, 3, 1});
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::SortVector(0));
}

// NOWE:
TEST_F(EngineComponentTest, SortVector) {
    engine_->addVector({5, 3, 1});       // precondition: raw access
    engine.receive(engine.sortVector(0));
    historian.receive(historian.sortVector());
}
```

### `EngineComponentTest::SetStrategy`

```cpp
// NOWE:
TEST_F(EngineComponentTest, SetStrategy) {
    engine.receive(engine.strategyChange(SortStrategyId::Descending));
    factory.receive(factory.create(SortStrategyId::Descending));
    historian.receive(historian.setSortStrategy());
}
```

### `EngineComponentTest::PublishSnapshot`

```cpp
// NOWE:
TEST_F(EngineComponentTest, PublishSnapshot) {
    engine_->addVector({1, 2, 3});
    engine.receive(engine.publishSnapshot());
    historian.receive(historian.publishSnapshot(1));
}
```

### `EngineComponentTest::FullEngineFlow`

```cpp
// NOWE:
TEST_F(EngineComponentTest, FullEngineFlow) {
    engine.receive(engine.addVector({1, 2, 3}));
    historian.receive(historian.addVector({1, 2, 3}));

    engine.receive(engine.sortVector(0));
    historian.receive(historian.sortVector());

    engine.receive(engine.strategyChange(SortStrategyId::Descending));
    factory.receive(factory.create(SortStrategyId::Descending));
    historian.receive(historian.setSortStrategy());

    engine.receive(engine.publishSnapshot());
    historian.receive(historian.publishSnapshot(1));
}
```

### `EngineComponentTest::LocalScenario`

```cpp
// STARE — demonstracja split run():
TEST_F(EngineComponentTest, LocalScenario) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run({ receiveVectorAdded({10, 20, 30}) });
    driver.run({ expectHistorianCommand("addVector", {{10, 20, 30}}) });
}

// NOWE — ta właściwość (brak granicy kroku) jest teraz naturalna:
TEST_F(EngineComponentTest, LocalScenario) {
    engine.receive(engine.addVector({10, 20, 30}));
    historian.receive(historian.addVector({10, 20, 30}));
}
```

### Testy `SplitRun_*` — czy migrować?

`SplitRun_*` testują właściwość "run() to fragment, nie granica kroku". W nowym API
ta właściwość jest nieodłączna — nie ma granicy run(). Testy `SplitRun_*` można zastąpić
testami `EndpointApiTest::MultiStep_*` i `SecondStimulus_ClosesPreviousStep`.

Rekomendacja: **zachować** `SplitRun_*` przez fazę 6, usunąć dopiero gdy
`EndpointApiTest` pokrywa te same klasy błędów (patrz sekcja 11).

### `HistorianOnlyTest` i `FactoryOnlyTest`

Migracja analogiczna. Kluczowa różnica:
- `historian` dostępny w `HistorianOnlyTest` (dziedziczy `HistorianSpy`)
- `factory` **niedostępne** w `HistorianOnlyTest` — kompilator blokuje

```cpp
// HistorianOnlyTest::SetStrategy — brak factory.receive():
TEST_F(HistorianOnlyTest, SetStrategy) {
    engine.receive(engine.strategyChange(SortStrategyId::Descending));
    // factory.receive(...) — nie kompiluje się, FactorySpy nie jest bazą
    historian.receive(historian.setSortStrategy());
    // Brak expectation factory.create() jest poprawny: kanał nieaktywny
}
```

Skutek: `HistorianOnlyTest::SetStrategy` i `EngineComponentTest::SetStrategy` nie
dzielą kodu — każdy test jest niezależny. Jest to celowy trade-off (czytelność > reużycie).

---

## 10. Niezmienniki podczas migracji

Przez całą migrację muszą być spełnione:

1. **Wszystkie 84 testy przechodzą** — nie wolno łamać istniejących testów
2. **`ScenarioFrameworkTest` bez zmian** — verifier niezmieniony
3. **`EngineDriver` kompiluje się** — backward compat przez fazę 1–6
4. **`Scenarios::*` kompilują się** — jw.
5. **`EndpointApiTest` przechodzi** — nowe regression testy przed migracją starych
6. **Każda faza kończy się `cmake --build` bez błędów** — inkrementalne

---

## 11. Kryteria akceptacji

### Funkcjonalne

- [ ] `engine.receive(engine.addVector({1,2,3}))` kompiluje się i wykonuje stimulus
- [ ] `historian.receive(historian.addVector({1,2,3}))` kompiluje się i weryfikuje expectation
- [ ] `factory.receive(factory.create(id))` j.w.
- [ ] `factory.receive(...)` nie kompiluje się w `HistorianOnlyTest`
- [ ] `historian.receive(...)` nie kompiluje się w `FactoryOnlyTest`
- [ ] Strict ordering zachowany — sygnał w złej kolejności → "Unexpected signal"
- [ ] Payload mismatch → "payload mismatch"
- [ ] Brak expectation → "Unexpected signal" w destruktorze executor_
- [ ] Brak actuals → "Signal not received"
- [ ] Expectation przed stimulus → "Expectation ... before any stimulus"
- [ ] Kanał nieaktywny → ciche pominięcie (nie "Signal not received")
- [ ] SequenceLog produkuje poprawny diagram dla nowych testów

### Strukturalne

- [ ] `ScenarioVerifier.hpp` — niezmieniony (diff: 0 linii)
- [ ] `SequenceLog.hpp` — niezmieniony
- [ ] `Signal.hpp` — niezmieniony
- [ ] `EngineDriver.hpp` — niezmieniony (kompiluje się przez całą migrację)
- [ ] Nowe pliki: `ScenarioExecutor.hpp`, `EngineEndpoint.hpp`,
      `HistorianEndpoint.hpp`, `FactoryEndpoint.hpp`
- [ ] `HistorianSpy` ma `public HistorianEndpoint historian`
- [ ] `FactorySpy` ma `public FactoryEndpoint factory`
- [ ] `EngineTestBase` ma `public EngineEndpoint engine`
- [ ] `TearDown()` woła `engine_.reset()` przed `executor_.reset()`
- [ ] Brak testów z `ADD_FAILURE` w destruktorze po poprawnym scenariuszu

### Liczba testów

Liczba testów po migracji powinna być >= 84. Nowe `EndpointApiTest` (7 testów)
zastępuje lub uzupełnia `SplitRun_*` (8 testów). Docelowo: ~83–92 testy.

---

## 12. Kwestie otwarte (do rozstrzygnięcia przed implementacją)

### 12.1 Nazwa deskryptora stimulus w SequenceLog

Obecne nazwy stimulusa w `Scenarios.hpp`: `"receiveAddVector"`, `"receiveSortRequested"`.
Proponowane nazwy deskryptorów: `"onSessionEvent(addVector)"`, `"onSessionEvent(sortVector)"`.

Czy ta zmiana nazw w logu sekwencji jest akceptowalna?
Alternatywa: zachować poprzednie nazwy w `Descriptor::name` bez zmiany.

### 12.2 `HistorianOnlyTest::SetStrategy` — brak factory expectation

Czy akceptować osobne testy dla każdej topologii?
Czy zamiast tego potrzebny jest mechanizm `optionally().receive(...)` (pomija jeśli kanał nieaktywny)?
Ten mechanizm nie jest potrzebny dla obecnych testów — decyzja może być podjęta później.

### 12.3 `engine_` jako `protected`

Aktualnie `engine_` jest `protected` w `EngineTestBase` — używane w testach do
preconditions (`engine_->addVector(...)`). Po migracji nadal potrzebny.
Alternatywa: dodać `EngineEndpoint::setup(lambda)` dla preconditions.
Rekomendacja: zachować `engine_` jako protected — prostsze, explicit.

### 12.4 Usunięcie `EngineDriver` i `Scenarios::*`

Możliwe dopiero po kompletnej migracji (faza 6 zakończona).
Nie jest częścią tego propozalu implementacyjnego.
