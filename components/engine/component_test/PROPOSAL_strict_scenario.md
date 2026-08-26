# Proposal: Strict Scenario Verification dla `EngineComponentTest`
# (rev 3 — po review)

---

## 1. Analiza obecnej implementacji

### Co istnieje

**`Endpoint`** — enum z czterema lifeline'ami: Driver, Engine, Historian, Factory.

**`SequenceLog`** — renderuje ASCII sequence diagram. Czysto wizualny. Żadnej weryfikacji.

**`HistorianSpy`** — implementuje `IHistorian`, akumuluje wszystkie wywołania do `commands` i `snapshots`.

**`FactorySpy`** — implementuje `ISortStrategyFactory`, akumuluje `requestedIds`, deleguje do prawdziwej fabryki.

**`EngineDriver`** — trzyma `Engine&` i `::testing::Test* owner_`, ma `std::vector<Signal>`:
```cpp
struct Signal {
    std::string           name;
    Endpoint              from;
    Endpoint              to;
    std::function<bool()> fn;  // robi albo akcję, albo sprawdza spy
};
```
`run()` iteruje po liście, wywołuje `fn()`, loguje diagram jeśli `fn()` zwróci true.

**`EngineComponentTest`** — wielokrotnie dziedziczy z `HistorianSpy` i `FactorySpy`. Fixture IS spym. Non-owning `shared_ptr` trzyma alive weak_ptry engine'a.

**Jedyny test** — `RunExecutesAllSignals` — tworzy `EngineDriver`, woła `driver.run()`, na końcu sprawdza `EXPECT_FALSE(spy->commands.empty())`.

---

## 2. Opis problemu

### Luka 1: brak strict verification (akumulacja zamiast kontrakt)

Obecny `HistorianSpy` po prostu **akumuluje** wywołania. Brak mechanizmu który:

**Przypadek A — Engine robi niespodziewane wywołanie:**
```cpp
void addVector(...) {
    historian_->recordCommand({"addVector", vec});
    historian_->publishSnapshot({...});  // ← nowe, nieoczekiwane
}
```
Spy akumuluje oba. Test **PASS** — nikt nie sprawdza czy `publishSnapshot` miał prawo nastąpić.

**Przypadek B — Engine pomija oczekiwane wywołanie:**
```cpp
void addVector(...) {
    // historian_->recordCommand(...); ← usunięte
}
```
`sendAddVector` sprawdza `spy->commands.back()`. Jeśli poprzedni sygnał cokolwiek wrzucił do spy... test może **PASS** na resztkach.

**Przypadek C — kolejność:**
Sprawdzanie `back()` nie uwzględnia kolejności. Zmiana kolejności wywołań pozostaje niezauważona.

### Luka 2: expectations nie są przypisane do konkretnego stimulusa

To jest **najważniejsza luka** w rev 1 propozycji.

Rev 1 proponował: na początku `run()` wyciągnąć **wszystkie** expectations ze scenariusza:
```cpp
// rev 1 — BŁĄD
for (const auto& sig : scenario)
    if (sig.role == SignalRole::Expectation)
        expectations.push_back(sig);
verifier_.setExpected(std::move(expectations));
```

Przy scenariuszu:
```
receiveAddVector        ← Stimulus
expectHistorianCommand  ← Expectation
receivePublishSnapshot  ← Stimulus
expectHistorianSnapshot ← Expectation
```

Verifier widzi globalną kolejkę: `[recordCommand, publishSnapshot]`.

Zepsuty Engine:
```
receiveAddVector:
    → recordCommand    (oczekiwane #1 ✓)
    → publishSnapshot  (oczekiwane #2 ✓)  ← BUG: za wcześnie!

receivePublishSnapshot:
    → NIC              ← BUG
```

Verifier: wszystko odebrane w kolejności → **PASS**. A powinien być FAIL.

**Rozwiązanie:** expectations są przypisane do konkretnego stimulusa. Każda para `Stimulus + następujące Expectations` to osobny `Step`. Weryfikacja odbywa się na końcu każdego stepu, nie na końcu całego scenariusza.

### Luka 3 — problem architektoniczny w `fn`

`fn` w `Signal` ma podwójną naturę:
- dla `Driver → Engine`: wykonuje akcję
- dla `Engine → Historian/Factory`: sprawdza stan spy po fakcie

Brak granicy między "co test robi" a "co Engine powinien zrobić". Naprawione przez `SignalRole`.

---

## 3. Proponowana architektura

### Kluczowe rozróżnienie: `SignalRole`

```
SignalRole::Stimulus    — test aktywnie wywołuje Engine
SignalRole::Expectation — opis tego, co musi wyjść z Engine podczas obsługi poprzedniego Stimulus
```

### Step-based verification (główna zmiana względem rev 1)

Scenariusz jest grupowany w kroki. Każdy krok = jeden Stimulus + wszystkie następujące po nim Expectations (do kolejnego Stimulus lub końca).

```
Scenariusz:
  Stimulus A
  Expectation 1      ┐
  Expectation 2      ├─ Step 1: {stimulus: A, expectations: [1, 2]}
                     ┘
  Stimulus B         ┐
  Expectation 3      ├─ Step 2: {stimulus: B, expectations: [3]}
                     ┘
```

Dla każdego kroku oddzielnie:
1. `verifier_.setExpected(step.expectations)` — uzbrojenie tylko na ten krok
2. `step.stimulus.action(engine_)` — wykonanie
3. `verifier_.verifyComplete()` — sprawdzenie brakujących sygnałów z tego kroku

Dzięki temu `publishSnapshot` wysłany podczas `addVector` jest natychmiast wykrywany:
```
Step 1 expectations: [recordCommand]
Engine podczas Step 1 wysyła: publishSnapshot
→ nextExpected_ >= expectations_.size() LUB name mismatch
→ FAIL: Unexpected signal
```

### Expectation przed pierwszym Stimulus = błąd scenariusza

Framework strict nie może cicho ignorować nieprawidłowych sygnałów. Expectation bez poprzedzającego Stimulus to błędnie napisany scenariusz — `ADD_FAILURE()` natychmiast:

```
Malformed scenario:
  Expectation "recordCommand" appears before any Stimulus
```

### Komponenty

```
┌─────────────────────────────────────────────────────┐
│                   Scenario                           │
│  [ Stimulus | Expectation | Expectation | Stimulus ] │
└───────────────┬─────────────────────────────────────┘
                │ grupowanie w Steps
                ▼
┌───────────────────────┐
│    EngineDriver       │  Per step:
│    .run(scenario)     │    setExpected(step.expectations)
│                       │    action(engine)
│                       │    verifyComplete()
└─────────┬─────────────┘
          │ armed = true (per step)
          ▼
┌─────────────────────┐
│  ScenarioVerifier   │  ← trzyma ordered queue expectations PER STEP
│  .report(actual)    │    przyjmuje zgłoszenia od spyów,
│  .verifyComplete()  │    porównuje z kolejnymi oczekiwanymi
└─────────▲───────────┘
          │ .report(SignalDescriptor)
    ┌─────┴─────┐
    │           │
┌───┴───┐  ┌───┴───┐
│Hist.  │  │Fact.  │   ← wywołane przez Engine podczas
│ Spy   │  │ Spy   │     execucji stimulusa → raportują
└───────┘  └───────┘     do verifier natychmiast
```

---

## 4. Przepływ krok po kroku

### Poprawny Engine

```
Scenario: AddVector({1,2,3}) + PublishSnapshot(1)
= [
    receiveAddVector({1,2,3}),     ← Stimulus     ┐ Step 1
    expectHistorianCommand(...),   ← Expectation   ┘
    receivePublishSnapshot(),      ← Stimulus     ┐ Step 2
    expectHistorianSnapshot(1),    ← Expectation   ┘
  ]

SetUp():
  verifier_ armed=false
  engine_->setFactory(factorySpy_) ← factory.create(Ascending) → IGNORED (not armed)

driver.run(scenario):

  ── Step 1 ──────────────────────────────────────
  verifier_.setExpected([expectHistorianCommand("addVector", {1,2,3})])
    armed=true, confused=false, nextExpected_=0

  CaptureStdout
  sig.action(engine_)
    → engine.onSessionEvent(VectorAdded)
      → engine.addVector({1,2,3})
        → historian.recordCommand({"addVector",{1,2,3}})
          → verifier_.report({Engine, Historian, "recordCommand", cmd})
            → name match ✓, payload match ✓
            → pendingRows_.push_back(...)
            → nextExpected_ = 1
  GetCapturedStdout → captured
  SequenceLog::logFlow(Driver, Engine, "receiveAddVector", captured)
  verifier_.flushDiagramRows()
    → SequenceLog::logFlow(Engine, Historian, "recordCommand")
  verifier_.verifyComplete()
    nextExpected_(1) == size(1) → PASS, armed=false

  ── Step 2 ──────────────────────────────────────
  verifier_.setExpected([expectHistorianSnapshot(1)])
    armed=true, confused=false, nextExpected_=0

  CaptureStdout
  sig.action(engine_)
    → engine.publishSnapshot()
      → historian.publishSnapshot(snap{vectorCount=1})
        → verifier_.report({Engine, Historian, "publishSnapshot", snap})
          → name match ✓, payload match ✓
          → nextExpected_ = 1
  GetCapturedStdout → captured
  SequenceLog::logFlow(Driver, Engine, "receivePublishSnapshot", captured)
  verifier_.flushDiagramRows()
  verifier_.verifyComplete() → PASS
```

### Zepsuty Engine — publishSnapshot wysłany za wcześnie

```
  ── Step 1 ──────────────────────────────────────
  verifier_.setExpected([expectHistorianCommand("addVector",...)])
    nextExpected_=0, size=1

  sig.action(engine_)
    → engine.addVector(...)
      → historian.recordCommand(...)
        → verifier_.report({..., "recordCommand"})
          → match ✓ → nextExpected_=1
      → historian.publishSnapshot(...)   ← BUG: za wcześnie
        → verifier_.report({..., "publishSnapshot"})
          → nextExpected_(1) >= size(1)
          → ADD_FAILURE: "Unexpected signal: publishSnapshot"
          → confused_=true

  verifier_.verifyComplete()
    confused_=true → suppress

  ── Step 2 ──────────────────────────────────────
  verifier_.setExpected([expectHistorianSnapshot(1)])
    armed=true, confused=false  ← reset na nowy step

  sig.action(engine_)
    → engine.publishSnapshot()
      → historian.publishSnapshot(...)
        → verifier_.report({..., "publishSnapshot"})
          → match ✓ → nextExpected_=1

  verifier_.verifyComplete() → PASS

Wynik końcowy:
  FAIL: Unexpected signal:
    from:   Engine
    to:     Historian
    signal: publishSnapshot
  (Step 2 przechodzi — każdy step startuje z czystym statem verifiera)
```

### Semantyka `armed_` — jawna deklaracja

Strict verification obowiązuje **wyłącznie wewnątrz aktywnego stepu** (między `setExpected()` a `verifyComplete()`).

Oznacza to że:
- `engine_->setFactory(factorySpy_)` w SetUp() → wywołuje `factory->create(Ascending)` → **ignorowane** (verifier nieuzbrojony)
- `engine_->addVector(...)` wywołane przed `driver.run()` → wywołuje `historian->recordCommand(...)` → **ignorowane**
- Verifier nie udaje, że pilnuje całego lifetime komponentu. Pilnuje wyłącznie kontraktu komunikacji zdefiniowanego w scenariuszu.

---

## 5. Wykrywanie błędów

### 5.1 Unexpected signal

Engine wywołuje collaboratora, ale w bieżącym stepie nie ma już oczekiwanych sygnałów (lub nazwa nie pasuje):

```
ADD_FAILURE:
  Unexpected signal:
    from:   Engine
    to:     Historian
    signal: publishSnapshot
```

### 5.2 Signal not received

Step się kończy, ale Engine nie wywołał oczekiwanego sygnału:

```
ADD_FAILURE:
  Signal not received:
    from:   Engine
    to:     Historian
    signal: recordCommand
```

### 5.3 Błędna kolejność (wewnątrz stepu)

Engine wywołuje w innej kolejności niż deklaruje scenariusz:

```
Step expectations: [recordCommand, publishSnapshot]
Engine wysyła:     [publishSnapshot, recordCommand]

verifier_.report({..., "publishSnapshot"})
  → expected[0].name = "recordCommand" ← mismatch
  → ADD_FAILURE:
      Unexpected signal:
        received: Engine -> Historian : publishSnapshot
        expected: Engine -> Historian : recordCommand
  → confused_=true
```

### 5.4 Błędny payload

Sygnał trafił we właściwe miejsce, ale dane się nie zgadzają:

```
ADD_FAILURE:
  Signal payload mismatch:
    signal: Engine -> Historian : recordCommand
```
Po tym `nextExpected_++` (sygnał był — tylko złe dane). Weryfikacja kolejności kontynuowana.

### 5.5 Malformed scenario

Expectation przed pierwszym Stimulus:

```
ADD_FAILURE:
  Malformed scenario:
    Expectation "recordCommand" appears before any Stimulus
```
`run()` zwraca natychmiast — nie wykonuje żadnego stepu.

---

## 6. Integracja z istniejącymi elementami

### Signal

```cpp
struct Signal {
    SignalRole  role;     // Stimulus | Expectation
    std::string name;
    Endpoint    from;
    Endpoint    to;
    std::function<void(Engine&)>         action;         // tylko Stimulus
    std::function<bool(const std::any&)> payloadMatcher; // tylko Expectation
};
```

### EngineDriver

`run()` grupuje scenario w Steps. `owner_` i `dynamic_cast` znikają. Malformed detection w fazie grupowania.

### Gotowe kolekcje scenariuszy

```cpp
driver.run(Scenarios::AddVector({1,2,3}));
driver.run(Scenarios::FullEngineFlow());
driver.run({
    receiveVectorAdded({10, 20}),
    expectHistorianCommand("addVector", {{10, 20}}),
});
```

### Historian i Factory

Spye przyjmują `ScenarioVerifier&` w konstruktorze. Każde wywołanie → natychmiast `verifier_.report()`. Wywołania przed `run()` są ignorowane (`armed_ = false`).

---

## 7. Proponowane pliki — kompletna treść

### `component_test/scenario/Signal.hpp` — NOWY

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
```

---

### `component_test/scenario/SequenceLog.hpp` — NOWY

```cpp
#pragma once
#include <algorithm>    // std::max
#include <format>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>  // std::string_view in labelOf
#include <utility>      // std::unreachable
#include <vector>
#include "Signal.hpp"

// ─── SequenceLog ──────────────────────────────────────────────────────────────
// Prints ASCII sequence-diagram rows with fixed lifeline columns.
//
//   col 0        col 36        col 72            col 92
//   [Driver]     [Engine]      [HistorianSpy]    # comment
//                [Engine]      [FactorySpy]      # comment

class SequenceLog {
public:
    static constexpr int kDriverCol   =  0;
    static constexpr int kEngineCol   = 36;
    static constexpr int kObserverCol = 72;
    static constexpr int kCommentCol  = 92;

    static void logFlow(Endpoint           from,
                        Endpoint           to,
                        const std::string& signal,
                        const std::string& captured = {})
    {
        int  fromC = colOf(from);
        int  toC   = colOf(to);
        auto fromL = std::string(labelOf(from));
        auto toL   = std::string(labelOf(to));

        std::string line;

        if (fromC <= toC) {
            int arrowLen = toC - (fromC + static_cast<int>(fromL.size()));
            int fills    = std::max(arrowLen - 6 - static_cast<int>(signal.size()), 0);
            line = std::format("{}{} ---{}{}> {}",
                spaces(fromC), fromL, signal, std::string(fills, '-'), toL);
        } else {
            int arrowLen = fromC - (toC + static_cast<int>(toL.size()));
            int fills    = std::max(arrowLen - 6 - static_cast<int>(signal.size()), 0);
            line = std::format("{}{} <---{}{} {}",
                spaces(toC), toL, signal, std::string(fills, '-'), fromL);
        }

        std::vector<std::string> remarks;
        {
            std::istringstream iss(captured);
            std::string ln;
            while (std::getline(iss, ln))
                if (!ln.empty()) remarks.push_back(ln);
        }

        if (!remarks.empty()) {
            int gap = std::max(kCommentCol - static_cast<int>(line.size()), 2);
            line += std::format("{:<{}}# {}", "", gap, remarks[0]);
        }
        std::cout << line << '\n';

        for (std::size_t i = 1; i < remarks.size(); ++i)
            std::cout << std::format("{:<{}}# {}\n", "", kCommentCol, remarks[i]);
    }

private:
    static std::string spaces(int n) { return std::string(std::max(n, 0), ' '); }

    static constexpr std::string_view labelOf(Endpoint e) noexcept {
        switch (e) {
            case Endpoint::Driver:    return "[Driver]";
            case Endpoint::Engine:    return "[Engine]";
            case Endpoint::Historian: return "[HistorianSpy]";
            case Endpoint::Factory:   return "[FactorySpy]";
        }
        std::unreachable();
    }

    static constexpr int colOf(Endpoint e) noexcept {
        switch (e) {
            case Endpoint::Driver:    return kDriverCol;
            case Endpoint::Engine:    return kEngineCol;
            case Endpoint::Historian: return kObserverCol;
            case Endpoint::Factory:   return kObserverCol;
        }
        std::unreachable();
    }
};
```

---

### `component_test/scenario/ScenarioVerifier.hpp` — NOWY

```cpp
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
```

---

### `component_test/scenario/Spies.hpp` — NOWY

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

class HistorianSpy : public patterns::historian::IHistorian {
public:
    explicit HistorianSpy(ScenarioVerifier& verifier) : verifier_(verifier) {}

    void recordCommand(const patterns::historian::CommandHistory& cmd) override {
        verifier_.report({
            .from    = Endpoint::Engine,
            .to      = Endpoint::Historian,
            .name    = "recordCommand",
            .payload = std::any{cmd}
        });
    }

    void publishSnapshot(const patterns::historian::EngineSnapshot& snap) override {
        verifier_.report({
            .from    = Endpoint::Engine,
            .to      = Endpoint::Historian,
            .name    = "publishSnapshot",
            .payload = std::any{snap}
        });
    }

private:
    ScenarioVerifier& verifier_;
};

// ─── FactorySpy ───────────────────────────────────────────────────────────────
// Reports every create() call to ScenarioVerifier.
// Delegates to real SortStrategyFactory so Engine gets a working strategy.
//
// Note: setFactory() calls factory->create(Ascending) during SetUp.
// That call is silently ignored (verifier not armed yet).

class FactorySpy : public patterns::strategy::ISortStrategyFactory {
public:
    explicit FactorySpy(ScenarioVerifier& verifier) : verifier_(verifier) {}

    [[nodiscard]] std::expected<std::unique_ptr<patterns::strategy::ISortStrategy>, std::string>
    create(patterns::strategy::SortStrategyId id) override {
        verifier_.report({
            .from    = Endpoint::Engine,
            .to      = Endpoint::Factory,
            .name    = "create",
            .payload = std::any{id}
        });
        return real_.create(id);
    }

private:
    ScenarioVerifier&                       verifier_;
    patterns::strategy::SortStrategyFactory real_;
};
```

---

### `component_test/scenario/Scenarios.hpp` — NOWY

```cpp
#pragma once
#include <iterator>     // std::make_move_iterator
#include <optional>
#include <utility>      // std::move
#include <vector>

#include "patterns/engine/Engine.hpp"
#include "patterns/historian/IHistorian.hpp"
#include "patterns/observer/SessionEvent.hpp"
#include "patterns/strategy/SortStrategyId.hpp"
#include "Signal.hpp"

// ═══════════════════════════════════════════════════════════════════════════════
// Stimulus builders — Driver → Engine
// ═══════════════════════════════════════════════════════════════════════════════

inline Signal receiveVectorAdded(std::vector<int> data = {1, 2, 3}) {
    return {
        .role   = SignalRole::Stimulus,
        .name   = "receiveAddVector",
        .from   = Endpoint::Driver,
        .to     = Endpoint::Engine,
        .action = [data](patterns::engine::Engine& e) {
            patterns::observer::SessionEvent ev;
            ev.type       = patterns::observer::SessionEventType::VectorAdded;
            ev.vectorData = data;
            e.onSessionEvent(ev);
        }
    };
}

inline Signal receiveSortRequested(size_t index = 0) {
    return {
        .role   = SignalRole::Stimulus,
        .name   = "receiveSortRequested",
        .from   = Endpoint::Driver,
        .to     = Endpoint::Engine,
        .action = [index](patterns::engine::Engine& e) {
            patterns::observer::SessionEvent ev;
            ev.type  = patterns::observer::SessionEventType::SortRequested;
            ev.index = index;
            e.onSessionEvent(ev);
        }
    };
}

inline Signal receiveStrategyChange(patterns::strategy::SortStrategyId id) {
    return {
        .role   = SignalRole::Stimulus,
        .name   = "receiveStrategyChange",
        .from   = Endpoint::Driver,
        .to     = Endpoint::Engine,
        .action = [id](patterns::engine::Engine& e) {
            patterns::observer::SessionEvent ev;
            ev.type       = patterns::observer::SessionEventType::StrategyChangeRequested;
            ev.strategyId = id;
            e.onSessionEvent(ev);
        }
    };
}

inline Signal receivePublishSnapshot() {
    return {
        .role   = SignalRole::Stimulus,
        .name   = "receivePublishSnapshot",
        .from   = Endpoint::Driver,
        .to     = Endpoint::Engine,
        .action = [](patterns::engine::Engine& e) { e.publishSnapshot(); }
    };
}

// ═══════════════════════════════════════════════════════════════════════════════
// Expectation builders — Engine → collaborator
// ═══════════════════════════════════════════════════════════════════════════════

// Expects historian.recordCommand() with given commandName.
// data: if provided, also validates the payload vector.
inline Signal expectHistorianCommand(
        std::string                     commandName,
        std::optional<std::vector<int>> data = std::nullopt)
{
    return {
        .role   = SignalRole::Expectation,
        .name   = "recordCommand",
        .from   = Endpoint::Engine,
        .to     = Endpoint::Historian,
        .payloadMatcher = [commandName = std::move(commandName), data]
                          (const std::any& payload) -> bool {
            const auto* cmd =
                std::any_cast<patterns::historian::CommandHistory>(&payload);
            if (!cmd)                            return false;
            if (cmd->commandName != commandName) return false;
            if (data && cmd->data != *data)      return false;
            return true;
        }
    };
}

// Expects historian.publishSnapshot().
// vectorCount: if provided, validates snap.vectorCount.
inline Signal expectHistorianSnapshot(
        std::optional<size_t> vectorCount = std::nullopt)
{
    return {
        .role   = SignalRole::Expectation,
        .name   = "publishSnapshot",
        .from   = Endpoint::Engine,
        .to     = Endpoint::Historian,
        .payloadMatcher = [vectorCount](const std::any& payload) -> bool {
            const auto* snap =
                std::any_cast<patterns::historian::EngineSnapshot>(&payload);
            if (!snap)                                            return false;
            if (vectorCount && snap->vectorCount != *vectorCount) return false;
            return true;
        }
    };
}

// Expects factory.create() called with the given SortStrategyId.
inline Signal expectFactoryCreate(patterns::strategy::SortStrategyId id) {
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

// ═══════════════════════════════════════════════════════════════════════════════
// Pre-built scenario collections
// ═══════════════════════════════════════════════════════════════════════════════

namespace Scenarios {

// Engine receives VectorAdded → must record command with correct payload.
inline std::vector<Signal> AddVector(std::vector<int> data = {1, 2, 3}) {
    return {
        receiveVectorAdded(data),
        expectHistorianCommand("addVector", data),
    };
}

// Engine receives SortRequested → must record "sortVector" command.
// Precondition: engine already has a vector at `index`.
inline std::vector<Signal> SortVector(size_t index = 0) {
    return {
        receiveSortRequested(index),
        expectHistorianCommand("sortVector"),
    };
}

// Engine receives StrategyChangeRequested(id)
//   → must call factory.create(id)          } same step — both
//   → then record "setSortStrategy" command  } checked in order
inline std::vector<Signal> SetStrategy(patterns::strategy::SortStrategyId id) {
    return {
        receiveStrategyChange(id),
        expectFactoryCreate(id),
        expectHistorianCommand("setSortStrategy"),
    };
}

// Engine receives publishSnapshot()
//   → must call historian.publishSnapshot() with optional vectorCount check.
inline std::vector<Signal> PublishSnapshot(
        std::optional<size_t> vectorCount = std::nullopt)
{
    return {
        receivePublishSnapshot(),
        expectHistorianSnapshot(vectorCount),
    };
}

// Full flow: AddVector → SortVector → SetStrategy → PublishSnapshot.
// Each sub-scenario forms its own step; expectations cannot bleed across steps.
inline std::vector<Signal> FullEngineFlow(
        std::vector<int>                   data  = {1, 2, 3},
        patterns::strategy::SortStrategyId strat =
            patterns::strategy::SortStrategyId::Descending)
{
    std::vector<Signal> scenario;
    auto append = [&](std::vector<Signal> part) {
        scenario.insert(scenario.end(),
                        std::make_move_iterator(part.begin()),
                        std::make_move_iterator(part.end()));
    };
    append(AddVector(data));
    append(SortVector(0));
    append(SetStrategy(strat));
    append(PublishSnapshot(1));
    return scenario;
}

} // namespace Scenarios
```

---

### `component_test/EngineDriver.hpp` — NOWY

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
// Malformed scenario: an Expectation appearing before any Stimulus is an error —
// the framework does not silently ignore it (ADD_FAILURE + return).

class EngineDriver {
public:
    EngineDriver(patterns::engine::Engine& engine, ScenarioVerifier& verifier)
        : engine_(engine), verifier_(verifier) {}

    void run(const std::vector<Signal>& scenario) {
        struct Step {
            const Signal*       stimulus;
            std::vector<Signal> expectations;
        };

        // Group scenario into steps.
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
};
```

---

### `component_test/EngineComponentTest.cpp` — ZMIENIONY

```cpp
#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>  // EXPECT_NONFATAL_FAILURE
#include <memory>

#include "patterns/engine/Engine.hpp"
#include "patterns/historian/IHistorian.hpp"
#include "patterns/services/ServiceLocator.hpp"
#include "patterns/services/Logger.hpp"
#include "patterns/strategy/SortStrategyId.hpp"

#include "scenario/Signal.hpp"
#include "scenario/ScenarioVerifier.hpp"
#include "scenario/Spies.hpp"
#include "scenario/Scenarios.hpp"
#include "EngineDriver.hpp"

using namespace patterns::services;
using namespace patterns::strategy;
using namespace patterns::historian;

// ═══════════════════════════════════════════════════════════════════════════════
// ScenarioFrameworkTest
// ─────────────────────────────────────────────────────────────────────────────
// Tests the verification framework itself using EXPECT_NONFATAL_FAILURE.
// Verifies that ScenarioVerifier correctly detects every class of contract
// violation — without involving a real Engine.
//
// These are regression tests for the framework, not for Engine behaviour.
// ═══════════════════════════════════════════════════════════════════════════════

class ScenarioFrameworkTest : public ::testing::Test {
protected:
    ScenarioVerifier verifier_;

    // ── Helpers: build SignalDescriptors with real payload types ───────────────

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

// Actual signal arrives with no expectation registered → Unexpected signal.
TEST_F(ScenarioFrameworkTest, UnexpectedSignal_NoExpectation) {
    verifier_.setExpected({});
    EXPECT_NONFATAL_FAILURE(
        verifier_.report(historianCommand("addVector")),
        "Unexpected signal"
    );
}

// Expected signal never arrives → Signal not received.
TEST_F(ScenarioFrameworkTest, SignalNotReceived) {
    verifier_.setExpected({expectHistorianCommand("addVector")});
    EXPECT_NONFATAL_FAILURE(
        verifier_.verifyComplete(),
        "Signal not received"
    );
}

// Signals arrive in wrong order within a step.
// Expected: recordCommand then publishSnapshot.
// Received: publishSnapshot first.
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

// Correct signal name and endpoint, but wrong payload data.
TEST_F(ScenarioFrameworkTest, PayloadMismatch) {
    verifier_.setExpected({expectHistorianCommand("addVector", {{1, 2, 3}})});
    EXPECT_NONFATAL_FAILURE(
        verifier_.report(historianCommand("addVector", {9, 9, 9})),
        "payload mismatch"
    );
}

// Regression test for rev 1 bug: signal produced during Step 1 must not
// satisfy an expectation belonging to Step 2.
//
// Simulates: publishSnapshot sent during addVector processing (wrong step).
// Step 1 knows only about recordCommand, so publishSnapshot is Unexpected.
TEST_F(ScenarioFrameworkTest, SignalFromWrongStep_RegressionRev1) {
    // Step 1 scope: only recordCommand expected.
    verifier_.setExpected({expectHistorianCommand("addVector")});

    // recordCommand arrives → OK.
    verifier_.report(historianCommand("addVector"));

    // publishSnapshot arrives still inside Step 1 — no expectation for it.
    EXPECT_NONFATAL_FAILURE(
        verifier_.report(historianSnapshot(0)),
        "Unexpected signal"
    );
}

// ═══════════════════════════════════════════════════════════════════════════════
// EngineComponentTest
// ─────────────────────────────────────────────────────────────────────────────
// Component tests for Engine against its real collaborators (via spies).
// Uses the strict scenario framework; every outbound call must be declared.
// ═══════════════════════════════════════════════════════════════════════════════

class EngineComponentTest : public ::testing::Test {
protected:
    void SetUp() override {
        [[maybe_unused]] auto r = ServiceLocator::instance().provide<Logger>(
            std::make_shared<Logger>());

        engine_       = std::make_shared<patterns::engine::Engine>();
        historianSpy_ = std::make_shared<HistorianSpy>(verifier_);
        factorySpy_   = std::make_shared<FactorySpy>(verifier_);

        engine_->setHistorian(historianSpy_);
        // setFactory internally calls factory->create(Ascending).
        // verifier_ is not armed → silently ignored.
        engine_->setFactory(factorySpy_);
    }

    ScenarioVerifier                          verifier_;
    std::shared_ptr<patterns::engine::Engine> engine_;
    std::shared_ptr<HistorianSpy>             historianSpy_;
    std::shared_ptr<FactorySpy>               factorySpy_;
};

// ─── Framework self-check (uses real Engine + EngineDriver) ──────────────────

// Expectation before the first Stimulus is a malformed scenario.
// The framework must report this rather than silently ignore it.
TEST_F(EngineComponentTest, MalformedScenario_ExpectationBeforeStimulus) {
    EngineDriver driver(*engine_, verifier_);
    EXPECT_NONFATAL_FAILURE(
        driver.run({
            expectHistorianCommand("addVector"),  // ← before any Stimulus
            receiveVectorAdded({1, 2, 3}),
        }),
        "Malformed scenario"
    );
}

// ─── Engine component contracts ───────────────────────────────────────────────

// Contract: receiveAddVector → historian.recordCommand("addVector", data)
TEST_F(EngineComponentTest, AddVector) {
    EngineDriver driver(*engine_, verifier_);
    driver.run(Scenarios::AddVector({1, 2, 3}));
}

// Contract: receiveSortRequested → historian.recordCommand("sortVector")
// Vector seeded before scenario — pre-run calls are not verified.
TEST_F(EngineComponentTest, SortVector) {
    engine_->addVector({5, 3, 1});

    EngineDriver driver(*engine_, verifier_);
    driver.run(Scenarios::SortVector(0));
}

// Contract: receiveStrategyChange(Descending)
//   → factory.create(Descending)               } same step,
//   → historian.recordCommand("setSortStrategy") } checked in order
TEST_F(EngineComponentTest, SetStrategy) {
    EngineDriver driver(*engine_, verifier_);
    driver.run(Scenarios::SetStrategy(SortStrategyId::Descending));
}

// Contract: receivePublishSnapshot → historian.publishSnapshot(vectorCount=1)
TEST_F(EngineComponentTest, PublishSnapshot) {
    engine_->addVector({1, 2, 3});

    EngineDriver driver(*engine_, verifier_);
    driver.run(Scenarios::PublishSnapshot(1));
}

// Full communication contract: AddVector → Sort → ChangeStrategy → Snapshot.
// Each sub-scenario is a separate step; no cross-step signal leakage possible.
TEST_F(EngineComponentTest, FullEngineFlow) {
    EngineDriver driver(*engine_, verifier_);
    driver.run(Scenarios::FullEngineFlow());
}

// Ad-hoc local scenario — no pre-built collection needed.
TEST_F(EngineComponentTest, LocalScenario) {
    EngineDriver driver(*engine_, verifier_);
    driver.run({
        receiveVectorAdded({10, 20, 30}),
        expectHistorianCommand("addVector", {{10, 20, 30}}),
    });
}
```

---

### `component_test/CMakeLists.txt` — ZMIENIONY

```cmake
add_executable(engine_component_tests EngineComponentTest.cpp)

# Needed so that #include "scenario/..." resolves relative to this directory.
target_include_directories(engine_component_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(engine_component_tests
    PRIVATE
        engine_lib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(engine_component_tests DISCOVERY_MODE PRE_TEST)
```

---

## 8. Wpływ na istniejące testy

| Plik | Zmiana |
|------|--------|
| `components/engine/tests/test_engine.cpp` | **Brak** |
| `tests/test_strategy.cpp` | **Brak** |
| `tests/test_session.cpp` | **Brak** |
| `tests/test_services.cpp` | **Brak** |
| `components/dummy_gui/tests/test_gui.cpp` | **Brak** |
| `components/engine/component_test/EngineComponentTest.cpp` | **Pełna zamiana** — 1 stary test zastąpiony 12 testami (5 framework + 1 malformed + 6 component) |

---

## 9. Ryzyka i edge case'y

**`setFactory` wywołuje `create(Ascending)` przed scenariuszem** — obsługiwane przez `armed_=false`. ✓

**`SortVector` bez wcześniejszego `addVector`** — Engine loguje błąd, nie woła historiana. `verifyComplete()` zgłasza "Signal not received". Ujawnia brakujące precondition.

**`SetStrategy` ma dwa expectations w jednym stepie** — `factory.create` i `historian.recordCommand` oba należą do tego samego Stimulus. Sprawdzane sekwencyjnie w kolejności deklaracji.

**Expectation przed pierwszym Stimulus** — to błąd scenariusza, nie silent drop. `ADD_FAILURE("Malformed scenario")` + `return`. Testowane przez `MalformedScenario_ExpectationBeforeStimulus`. ✓

**`confused_` i cascading failures** — przy błędzie kolejności jeden czytelny failure, reszta stepu pominięta. `confused_` jest resetowane przez `setExpected()` na początku każdego kolejnego stepu. ✓

**Payload `std::any`** — `std::any_cast<T>(&payload)` zwraca `nullptr` przy type mismatch. Bezpieczne. Typ musi być identyczny — nie ma problemu bo spye tworzą `std::any{cmd}` bezpośrednio.

**`flushDiagramRows()` nie może być w `report()`** — gdyby `SequenceLog::logFlow` był wywoływany wewnątrz `report()` (wywoływanego podczas CaptureStdout), diagram trafiłby do bufora. `report()` kolejkuje do `pendingRows_`, `flushDiagramRows()` drukuje dopiero po `GetCapturedStdout()`. ✓

---

## 10. Dlaczego nie `StrictMock<HistorianMock> + EXPECT_CALL`

| Aspekt | StrictMock + EXPECT_CALL | Proponowane rozwiązanie |
|--------|--------------------------|------------------------|
| **Model** | Per-metoda, per-obiekt | Per-scenariusz, komunikacja jako całość |
| **Reużycie** | `EXPECT_CALL` powtórzony w każdym TEST | `driver.run(Scenarios::AddVector())` |
| **Kompozycja** | Trudna — makra nie są data | `std::vector<Signal>` — łączyć, parametryzować |
| **Step isolation** | Brak — expectations globalne per test | Każdy Stimulus ma własną izolowaną weryfikację |
| **Sequence diagram** | Nie istnieje | Wbudowany w runner |
| **Lokalny scenariusz** | Tak samo verbose jak pełny | `{ receiveVectorAdded(), expectHistorianCommand() }` |
| **Zależność** | Wymaga `GTest::gmock` | Tylko `GTest::gtest_main` |
| **Framework self-test** | Trudny (StrictMock weryfikuje GMock, nie siebie) | `ScenarioFrameworkTest` testuje verifier bezpośrednio |

---

## Tabela zachowań

| Przypadek | Obecne zachowanie | Proponowane zachowanie |
|-----------|------------------|----------------------|
| Engine wywołuje collaboratora bez deklaracji w bieżącym stepie | spy akumuluje, **PASS** | `FAIL: Unexpected signal` |
| Sygnał deklarowany, ale Engine go nie wywołuje | może **PASS** (resztki w spy) | `FAIL: Signal not received` |
| Engine wywołuje w złej kolejności | akumulacja, **PASS** | `FAIL: received X, expected Y` |
| Sygnał z właściwego stepu ale z innego stimulusa | **PASS** (rev 1 też) | `FAIL: Unexpected signal` ← kluczowa naprawa |
| Payload nie zgadza się | nie sprawdzane | `FAIL: Signal payload mismatch` |
| Expectation przed pierwszym Stimulus | silent drop (rev 2) | `FAIL: Malformed scenario` |
| Wszystko zgodne ze scenariuszem | **PASS** | **PASS** |
| Gotowy scenariusz | `driver.run()` hardkodowane sygnały | `driver.run(Scenarios::FullEngineFlow())` |
| Lokalny scenariusz ad-hoc | nieobsługiwane | `driver.run({ receiveVectorAdded(), ... })` |
| Framework wykrywa własne błędy | nie testowane | `ScenarioFrameworkTest` — 5 dedykowanych testów |

---

## FILES TO MODIFY

| Plik | Zakres |
|------|--------|
| `components/engine/component_test/EngineComponentTest.cpp` | Pełna zamiana |
| `components/engine/component_test/CMakeLists.txt` | Dodanie `target_include_directories` |

## FILES TO ADD

```
components/engine/component_test/EngineDriver.hpp
components/engine/component_test/scenario/Signal.hpp
components/engine/component_test/scenario/SequenceLog.hpp
components/engine/component_test/scenario/ScenarioVerifier.hpp
components/engine/component_test/scenario/Spies.hpp
components/engine/component_test/scenario/Scenarios.hpp
```

## FILES TO REMOVE

Brak — obecny `EngineComponentTest.cpp` jest modyfikowany, nie usuwany.
