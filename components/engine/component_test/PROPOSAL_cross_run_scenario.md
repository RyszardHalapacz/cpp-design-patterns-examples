# Proposal v3: Cross-run scenario lifetime — stateful per-step parser

## Zmiany względem v2 (recenzja)

Trzy błędy wskazane w recenzji:

1. `SplitRun_ExpectationWithoutActual` — test nie dostaje "Signal not received", dostaje
   "Malformed scenario" (racja parsera: bez żadnego stimulus expectation jest malformed).
   Poprawny scenariusz dla "Signal not received": otwarty krok, stimulus nie wygenerował actual.

2. `SplitRun_WrongOrder` — test nie sprawdza wrong order; drugi stimulus finalizuje krok A
   (A nieodebrane → "Unexpected signal"), zanim w ogóle zostaną zadeklarowane odwrócone
   expectations. Potrzeba dwóch actuali z jednego stimulusa, dopasowanych w złej kolejności.

3. Tabela "strict per-step" kłamała — `armed_=true` / Mode 1 to ścieżka `ScenarioFrameworkTest`,
   nie EngineDriver. Wszystkie scenariusze przez EngineDriver (w tym same-run) przechodzą
   przez Mode 2. Brakuje parity tests Mode 2.

---

## 1. Diagnoza (bez zmian — skrót)

```
run({stimulusA})  →  setExpected({}) → report() → armed+empty → "Unexpected signal" ✗
run({expectA})    →  steps.empty() przy Expectation → "Malformed scenario" ✗
```

---

## 2. Model docelowy (bez zmian)

`EngineDriver` traktuje wszystkie `run()` jako jeden ciągły strumień `Signal`.

```
napotkany Stimulus:
    jeśli pending step → finalizuj go
    otwórz nowy pending step
    wykonaj stimulus (collectingActuals_=true → actuals do stepActuals_)

napotkana Expectation:
    jeśli brak pending step → Malformed scenario
    dopasuj do actuali pending step (ordered, strict)

~EngineDriver():
    jeśli pending step → finalizuj go
```

**Invariant**: co najwyżej jeden otwarty krok w danej chwili.
Nowy Stimulus finalizuje poprzedni krok przed otwarciem nowego.

---

## 3. Proponowane zmiany (bez zmian względem v2)

### Zmiana A: `ScenarioVerifier` — Mode 1 i Mode 2

#### Dwa tryby — jasna separacja

**Mode 1** — istniejący, używany wyłącznie przez `ScenarioFrameworkTest` bezpośrednio:
```
setExpected()  →  report() z armed_=true  →  verifyComplete()
```
Nie dotknięty. `ScenarioFrameworkTest` używa go bez zmian.

**Mode 2** — nowy, używany wyłącznie przez `EngineDriver` (dla WSZYSTKICH scenariuszy,
w tym same-run):
```
beginStep()  →  report() z collectingActuals_=true  →  endStepCollection()
→  matchExpectation()  →  finalizeStep()
```

Jedyna zmiana w istniejącym kodzie `report()`: dwie nowe linie na początku.

#### Nowe pola prywatne

```cpp
std::vector<SignalDescriptor> stepActuals_;
size_t                        nextActualInStep_ = 0;
bool                          collectingActuals_ = false;
bool                          stepConfused_      = false;
```

#### Zmiana `report()` — nowe pierwsze dwie linie

```cpp
void report(const SignalDescriptor& actual) {
    if (collectingActuals_) {          // ← Mode 2: kolekcjonuj actual
        stepActuals_.push_back(actual);
        return;
    }
    if (!armed_ || confused_) return;  // ← Mode 1: bez zmian
    ...
```

#### Nowe metody Mode 2

```cpp
void beginStep() {
    stepActuals_.clear();
    nextActualInStep_ = 0;
    stepConfused_     = false;
    collectingActuals_ = true;
}

void endStepCollection() {
    collectingActuals_ = false;
}

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
```

---

### Zmiana B: `EngineDriver` — parser stanowy

```cpp
class EngineDriver {
public:
    EngineDriver(patterns::engine::Engine& engine,
                 ScenarioVerifier&          verifier,
                 ActiveChannels             channels)
        : engine_(engine), verifier_(verifier), channels_(channels) {}

    ~EngineDriver() {
        if (stepOpen_) {
            verifier_.finalizeStep();
            verifier_.flushDiagramRows();
        }
    }

    void run(const std::vector<Signal>& scenario) {
        for (const auto& sig : scenario) {

            if (sig.role == SignalRole::Stimulus) {
                if (stepOpen_) {
                    verifier_.finalizeStep();
                    verifier_.flushDiagramRows();
                }
                verifier_.beginStep();
                stepOpen_ = true;

                testing::internal::CaptureStdout();
                sig.action(engine_);
                std::string captured = testing::internal::GetCapturedStdout();

                verifier_.endStepCollection();
                SequenceLog::logFlow(sig.from, sig.to, sig.name, captured);

            } else if (sig.role == SignalRole::Expectation) {
                if (!stepOpen_) {
                    ADD_FAILURE()
                        << "Malformed scenario:\n"
                        << "  Expectation \"" << sig.name
                        << "\" appears before any Stimulus";
                    return;
                }
                if (!channels_.isActive(sig.to)) continue;
                verifier_.matchExpectation(sig);
            }
        }
    }

private:
    patterns::engine::Engine& engine_;
    ScenarioVerifier&          verifier_;
    ActiveChannels             channels_;
    bool                       stepOpen_ = false;
};
```

---

## 4. Weryfikacja przypadków

### A. `run({stimulusA}); run({expectA})` → PASS ✓

```
run({stimulusA}):  beginStep, execute A → stepActuals_=[A]. stepOpen_=true.
run({expectA}):    matchExpectation(expectA) → pasuje A. stepOpen_=true.
destruktor:        finalizeStep() → pusty → OK. flushDiagramRows().
```

### B. `run({stimulusA}); run({stimulusB})` → FAIL (poprawnie — A niematched) ✓

```
run({stimulusA}):  beginStep, stepActuals_=[A]. stepOpen_=true.
run({stimulusB}):
  stepOpen_=true → finalizeStep() → A niematched → "Unexpected signal: A"
  beginStep(), execute B → stepActuals_=[B]. stepOpen_=true.
destruktor:
  finalizeStep() → B niematched → "Unexpected signal: B"
```

### C. `run({stimulusA}); run({expectA, stimulusB, expectB})` → PASS ✓

```
run({stimulusA}):  beginStep, stepActuals_=[A]. stepOpen_=true.
run({expectA, stimulusB, expectB}):
  expectA:   matchExpectation(expectA) → pasuje A. pendingRows_=[A].
  stimulusB: finalizeStep() → pusty → OK. flushDiagramRows() → rysuje A.
             beginStep(), execute B → stepActuals_=[B].
  expectB:   matchExpectation(expectB) → pasuje B.
destruktor:  finalizeStep() → pusty. flushDiagramRows() → rysuje B.
```

### D. `run({expectation, stimulus})` bez poprzedniego stimulusa → Malformed ✓

```
sig=expectation: stepOpen_=false → "Malformed scenario"
```

### E. `run({sA}); run({sB}); run({expA}); run({expB})` → FAIL (poprawnie) ✓

```
run({sA}): stepActuals_=[A]. stepOpen_=true.
run({sB}): finalizeStep() → A niematched → "Unexpected signal: A".
           stepActuals_=[B]. stepOpen_=true.
run({expA}): matchExpectation(expA) → porównuje z stepActuals_[0]=B.
             B != A → "Unexpected signal: received B, expected A". stepConfused_=true.
run({expB}): matchExpectation(expB) → stepConfused_=true → return (no cascade).
destruktor: finalizeStep() → stepConfused_=true → bez dodatkowego reportowania.
```

---

## 5. Testy regresyjne — poprawione i uzupełnione

### 5a. Parity tests Mode 2 — `ScenarioFrameworkTest`

Dowodzą, że `matchExpectation()` + `finalizeStep()` dają ten sam strict contract
co `report()` + `verifyComplete()` z Mode 1.
Używają API `ScenarioVerifier` bezpośrednio (bez `EngineDriver`).

```cpp
// Mode 2: actual bez expectation → "Unexpected signal"
TEST_F(ScenarioFrameworkTest, Mode2_UnexpectedSignal) {
    verifier_.beginStep();
    verifier_.report(historianCommand("addVector"));
    verifier_.endStepCollection();
    EXPECT_NONFATAL_FAILURE(
        verifier_.finalizeStep(),
        "Unexpected signal"
    );
}

// Mode 2: expectation bez actual → "Signal not received"
TEST_F(ScenarioFrameworkTest, Mode2_SignalNotReceived) {
    verifier_.beginStep();
    // brak report() — stepActuals_ puste
    verifier_.endStepCollection();
    EXPECT_NONFATAL_FAILURE(
        verifier_.matchExpectation(expectHistorianCommand("addVector")),
        "Signal not received"
    );
}

// Mode 2: dwa actuale, expectation na drugi → wrong order → "Unexpected signal"
TEST_F(ScenarioFrameworkTest, Mode2_WrongOrder) {
    verifier_.beginStep();
    verifier_.report(historianCommand("addVector"));    // actual[0]
    verifier_.report(historianSnapshot(0));             // actual[1]
    verifier_.endStepCollection();
    // Żądamy drugiego zanim pierwszego:
    EXPECT_NONFATAL_FAILURE(
        verifier_.matchExpectation(expectHistorianSnapshot()),
        "Unexpected signal"
    );
}

// Mode 2: poprawny sygnał, zły payload → "payload mismatch"
TEST_F(ScenarioFrameworkTest, Mode2_PayloadMismatch) {
    verifier_.beginStep();
    verifier_.report(historianCommand("addVector", {9, 9, 9}));
    verifier_.endStepCollection();
    EXPECT_NONFATAL_FAILURE(
        verifier_.matchExpectation(expectHistorianCommand("addVector", {{1, 2, 3}})),
        "payload mismatch"
    );
}
```

### 5b. Cross-run tests — `EngineComponentTest` / `HistorianOnlyTest`

```cpp
// ── Split: stimulus w run #1, expectation w run #2 → PASS ──────────────────
TEST_F(EngineComponentTest, SplitRun_StimulusThenExpectation) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run({ receiveVectorAdded({10, 20, 30}) });
    driver.run({ expectHistorianCommand("addVector", {{10, 20, 30}}) });
}

// ── Actual bez expectation do końca scenariusza → "Unexpected signal" ───────
// (detekcja w destruktorze EngineDriver)
TEST_F(EngineComponentTest, SplitRun_ActualWithoutExpectation) {
    EXPECT_NONFATAL_FAILURE(
        [&]() {
            EngineDriver driver(*engine_, verifier_, channels_);
            driver.run({ receiveVectorAdded({10, 20, 30}) });
            // brak expectation → destruktor: finalizeStep() → "Unexpected signal"
        }(),
        "Unexpected signal"
    );
}

// ── Signal not received: stimulus nie generuje oczekiwanego sygnału ─────────
// receiveVectorAdded nie wywołuje FactorySpy → stepActuals_ puste dla Factory.
// W FactoryOnlyTest channels_.factory=true → expectFactoryCreate nie jest filtrowane.
TEST_F(FactoryOnlyTest, SplitRun_SignalNotReceived) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run({ receiveVectorAdded({1, 2, 3}) });
    // receiveVectorAdded nie triggeruje Factory → stepActuals_ puste
    EXPECT_NONFATAL_FAILURE(
        driver.run({ expectFactoryCreate(SortStrategyId::Ascending) }),
        "Signal not received"
    );
}

// ── Wrong order: dwa actuale z jednego stimulusa, expectations odwrócone ─────
// receiveStrategyChange generuje w jednym kroku:
//   actual[0]: factory.create(Descending)
//   actual[1]: historian.recordCommand("setSortStrategy")
// Deklarujemy w odwrotnej kolejności → mismatch → "Unexpected signal"
TEST_F(EngineComponentTest, SplitRun_WrongOrder) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run({ receiveStrategyChange(SortStrategyId::Descending) });
    EXPECT_NONFATAL_FAILURE(
        driver.run({
            expectHistorianCommand("setSortStrategy"),       // żądany pierwszy, ale przychodzi drugi
            expectFactoryCreate(SortStrategyId::Descending),
        }),
        "Unexpected signal"
    );
}

// ── Payload mismatch w trybie split ─────────────────────────────────────────
TEST_F(EngineComponentTest, SplitRun_PayloadMismatch) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run({ receiveVectorAdded({10, 20, 30}) });
    EXPECT_NONFATAL_FAILURE(
        driver.run({ expectHistorianCommand("addVector", {{99, 99, 99}}) }),
        "payload mismatch"
    );
}

// ── Multi-step split — cztery oddzielne run() ────────────────────────────────
TEST_F(EngineComponentTest, SplitRun_MultiStep) {
    engine_->addVector({3, 1, 2});
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run({ receiveVectorAdded({3, 1, 2}) });
    driver.run({ expectHistorianCommand("addVector", {{3, 1, 2}}) });
    driver.run({ receiveSortRequested(0) });
    driver.run({ expectHistorianCommand("sortVector") });
}

// ── Mixed fragment: expectation + stimulus + expectation w jednym run() ──────
// Przypadek niemożliwy w v1; dowodzi że granica run() znika z modelu semantycznego.
TEST_F(EngineComponentTest, SplitRun_MixedFragment) {
    engine_->addVector({5, 3, 1});
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run({ receiveVectorAdded({1, 2, 3}) });
    driver.run({
        expectHistorianCommand("addVector", {{1, 2, 3}}),  // zamyka krok A
        receiveSortRequested(0),                            // otwiera krok B
        expectHistorianCommand("sortVector"),               // zamyka krok B
    });
}

// ── Selective channels w trybie split ────────────────────────────────────────
TEST_F(HistorianOnlyTest, SplitRun_HistorianOnly) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run({ receiveVectorAdded({5, 6, 7}) });
    driver.run({ expectHistorianCommand("addVector", {{5, 6, 7}}) });
}

// ── LocalScenario — główny przypadek z wymagania ─────────────────────────────
TEST_F(EngineComponentTest, LocalScenario) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run({ receiveVectorAdded({10, 20, 30}) });
    driver.run({ expectHistorianCommand("addVector", {{10, 20, 30}}) });
}
```

---

## 6. Podsumowanie zmian

| Plik | Zmiana | Zakres |
|------|--------|--------|
| `scenario/ScenarioVerifier.hpp` | `report()`: 2 nowe linie na początku | 2 linie |
| `scenario/ScenarioVerifier.hpp` | 4 nowe pola prywatne | 4 linie |
| `scenario/ScenarioVerifier.hpp` | Nowe metody: `beginStep`, `endStepCollection`, `matchExpectation`, `finalizeStep` | ~55 linii |
| `EngineDriver.hpp` | Przepisany `run()` (prostszy — brak `struct Step`) + destruktor + `stepOpen_` | ~45 linii |
| `EngineComponentTest.cpp` | Naprawa `LocalScenario` + 4 Mode 2 parity + 9 cross-run testów | ~90 linii |

**Pliki niezmienione**: `Signal.hpp`, `Scenarios.hpp`, `Spies.hpp`, `SequenceLog.hpp`,
cały kod produkcyjny.

**Istniejące testy niezmienione**: wszystkie `ScenarioFrameworkTest` (Mode 1 nienaruszone),
wszystkie `EngineComponentTest` poza `LocalScenario`, wszystkie `HistorianOnlyTest`,
wszystkie `FactoryOnlyTest`.

---

## 7. Właściwości strictness

| Właściwość | Mechanizm |
|------------|-----------|
| Unexpected signal | `finalizeStep()` raportuje niematched actuals. |
| Signal not received | `matchExpectation()` raportuje brak actual w `stepActuals_`. |
| Wrong order | `matchExpectation()` ordered match — pierwsza niezgodność → "Unexpected signal". |
| Payload mismatch | `matchExpectation()` sprawdza `payloadMatcher`. |
| Zero-expectation semantics | Actual w stimulusie bez expectation → `stepActuals_` → `finalizeStep()` → błąd. |
| Cross-step isolation | Co najwyżej jeden pending step. Nowy Stimulus finalizuje poprzedni. Brak globalnego ledgera. |
| Mode 1 backward compat | `armed_=true` / `collectingActuals_=false` → ścieżka Mode 1 identyczna z obecną. |
| Mode 2 parity | 4 parity tests w `ScenarioFrameworkTest` potwierdzają równoważność kontraktu. |

> **Mode 1** (`setExpected`/`report(armed)`/`verifyComplete`): zachowany w 100%, używany
> przez `ScenarioFrameworkTest` bezpośrednio — nie zmieniony.
>
> **Mode 2** (`beginStep`/`report(collecting)`/`matchExpectation`/`finalizeStep`): nowy,
> używany przez `EngineDriver` dla **wszystkich** scenariuszy (same-run i cross-run).
> Oba tryby istnieją w tym samym `ScenarioVerifier` i nie kolidują.
