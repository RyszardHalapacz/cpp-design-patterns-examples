# PROPOSAL: Typed Endpoint API — ewolucja frameworka testów komponentowych Engine

**Status:** draft
**Data:** 2026-08-27
**Poprzednie propozycje:** PROPOSAL_strict_scenario.md, PROPOSAL_selective_channel_activation.md,
PROPOSAL_cross_run_scenario.md

---

## 1. Analiza obecnej architektury frameworka

### Pliki i odpowiedzialności

| Plik | Rola |
|------|------|
| `Signal.hpp` | Typy podstawowe: `Signal`, `SignalDescriptor`, `Endpoint`, `SignalRole`, `ActiveChannels` |
| `ScenarioVerifier.hpp` | Silnik weryfikacji: Mode 1 (legacy) i Mode 2 (per-step) |
| `Spies.hpp` | `HistorianSpy`, `FactorySpy` — przechwytują rzeczywiste wywołania, raportują do verifier |
| `Scenarios.hpp` | Builderzy Stimulus i Expectation; gotowe scenariusze `Scenarios::*` |
| `EngineDriver.hpp` | Parser strumienia `Signal`; wykonuje scenariusze przeciwko żywemu Engine |
| `SequenceLog.hpp` | Diagram sekwencji ASCII |
| `EngineComponentTest.cpp` | Fixture'y + testy komponentowe + regression testy frameworka |

### Przepływ danych

```
TEST_F
  └─ driver.run({ Stimulus, Expectation... })
       ├─ Stimulus → verifier.beginStep()
       │              → engine.onSessionEvent(...)    ← spy przechwytuje
       │              → verifier.endStepCollection()  ← spy zbuforował actuals
       └─ Expectation → verifier.matchExpectation(sig)
                         ← porównuje z actuals w kolejności
  TearDown / ~EngineDriver → verifier.finalizeStep()
```

### Topologia przez dziedziczenie (mechanizm kluczowy)

`EngineTestBase::SetUp()` wykrywa aktywne kanały przez `dynamic_cast`:
```cpp
auto* h = dynamic_cast<HistorianSpy*>(this);
auto* f = dynamic_cast<FactorySpy*>(this);
channels_ = {h != nullptr, f != nullptr};
```
Expectation dla nieaktywnego kanału jest cicho pomijana — nie generuje "Signal not received".
Ten sam `Scenarios::SetStrategy(id)` działa w `EngineComponentTest` (oba kanały),
`HistorianOnlyTest` (factory pominięte) i `FactoryOnlyTest` (historian pominięty).

### Silne strony obecnego frameworka

- strict ordering weryfikacji (Mode 2: `stepActuals_` + indeks)
- wykrywanie Unexpected signal i Signal not received
- payload matching (lambda + `std::any`)
- czytelne diagnostyki
- topologia = skład fixture (dziedziczenie)
- selective channel activation
- sequence logging (ASCII diagram)
- cross-run step lifetime (`run()` to fragment, nie granica kroku)
- `ScenarioFrameworkTest` — regression testy samego frameworka
- spie przechwytują rzeczywiste wywołania produkcyjnego Engine

---

## 2. Problemy obecnego `driver.run({...})`

### 2.1 Boilerplate opakowujący

```cpp
EngineDriver driver(*engine_, verifier_, channels_);
driver.run(Scenarios::AddVector({1, 2, 3}));
```

Użytkownik testu musi:
- skonstruować `EngineDriver` z trzema argumentami
- owinąć każdy fragment w `driver.run({...})`
- wiedzieć o `verifier_` i `channels_`

### 2.2 Stringi zamiast typów

```cpp
expectHistorianCommand("addVector", {{1, 2, 3}});
```
Nazwa `"addVector"` nie jest sprawdzana przez kompilator. Literówka → test przechodzi mimo błędu.

### 2.3 Model Stimulus → Expectations zakodowany w strukturze listy

```cpp
driver.run({
    receiveVectorAdded({1, 2, 3}),        // Stimulus
    expectHistorianCommand("addVector"),    // Expectation
});
```
Model umysłowy użytkownika to "lista sygnałów", a nie "interakcja między komponentami".
Trudno rozszerzyć na sygnały inicjowane przez Engine.

### 2.4 `Scenarios::*` jako pre-built collections mają ograniczoną czytelność

Dla prostego testu:
```cpp
driver.run(Scenarios::AddVector({1, 2, 3}));
```
Trzeba skakać do `Scenarios.hpp` żeby zobaczyć co faktycznie jest weryfikowane.

---

## 3. Docelowy model mentalny

**TEST_F jest scenariuszem.** Każda linia testu to deklaracja komunikacji:

```
kto.receive(co)     — ktoś odbiera wiadomość
kto.send(co)        — ktoś wysyła wiadomość (przyszłość: spontaniczne sygnały)
```

Model sekwencji:
```
engine.receive(X)      ← wykonuje akcję na Engine, zbiera actuals
historian.receive(Y)   ← deklaruje oczekiwaną odpowiedź (matchExpectation)
factory.receive(Z)     ← deklaruje oczekiwaną odpowiedź (matchExpectation)
engine.receive(W)      ← zamyka poprzedni krok, otwiera nowy
```

Brak `driver.run()`. Brak stringów. Brak `Stimulus`/`Expectation` w publicznym API.

---

## 4. Projekt publicznego DSL / API

### Kompletny test

```cpp
TEST_F(EngineComponentTest, AddVector)
{
    engine.receive(engine.addVector({1, 2, 3}));
    historian.receive(historian.addVector({1, 2, 3}));
}

TEST_F(EngineComponentTest, SetStrategy)
{
    engine.receive(engine.strategyChange(SortStrategyId::Descending));
    factory.receive(factory.create(SortStrategyId::Descending));
    historian.receive(historian.setSortStrategy());
}

TEST_F(EngineComponentTest, FullEngineFlow)
{
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

### Przyszły scenariusz spontaniczny (Engine generuje sygnał)

```cpp
TEST_F(EngineComponentTest, HardwareFailure)
{
    engine.send(engine.hardwareFailure(ErrorCode::MotorFailure));
    historian.receive(historian.recordError(ErrorCode::MotorFailure));
    gui.receive(gui.showError(ErrorCode::MotorFailure));
}
```

`engine.send()` otwiera krok bez zewnętrznego stimulus — sygnał pochodzi z Engine.

---

## 5. Projekt endpointów — typy i odpowiedzialności

### 5.1 `EngineEndpoint`

Publiczny member `EngineTestBase`. Eksponuje:
- builderów deskryptorów stimulusa (każdy mapuje na konkretne wywołanie `Engine`)
- `receive(Descriptor)` — wykonuje stimulus, otwiera krok
- `send(Descriptor)` — przyszłość: spontaniczny sygnał z Engine

```cpp
class EngineEndpoint {
public:
    struct Descriptor {
        std::string                             name;
        std::function<void(Engine&)>            action;
    };

    // Deskryptory stimulusa — domain-level, nie interface-level
    Descriptor addVector(std::vector<int> data);
    Descriptor sortVector(size_t index = 0);
    Descriptor strategyChange(patterns::strategy::SortStrategyId id);
    Descriptor publishSnapshot();

    // Wykonuje stimulus; zamyka poprzedni krok jeśli był otwarty
    void receive(Descriptor d);

    // Przyszłość — spontaniczny sygnał inicjowany przez Engine
    // void send(Descriptor d);

    void attach(ScenarioExecutor& ex, patterns::engine::Engine& engine);

private:
    ScenarioExecutor*             executor_ = nullptr;
    patterns::engine::Engine*     engine_   = nullptr;
};
```

### 5.2 `HistorianEndpoint`

Publiczny member `HistorianSpy`. Eksponuje domain-level metody — nie interface-level.
`IHistorian::recordCommand("addVector", ...)` staje się `historian.addVector(...)`.

```cpp
class HistorianEndpoint {
public:
    using Sig = Signal;

    // Deskryptory expectation — domain-level
    Sig addVector(std::vector<int> data);
    Sig sortVector();
    Sig setSortStrategy();
    Sig publishSnapshot(std::optional<size_t> vectorCount = {});

    // Deklaruje expectation dla bieżącego kroku
    void receive(Sig sig);

    void attach(ScenarioExecutor& ex);

private:
    ScenarioExecutor* executor_ = nullptr;
};
```

Każda metoda zwraca gotowy `Signal` z wypełnionym `payloadMatcher` — tak jak robią to
dziś `expectHistorianCommand(...)` itp., ale bez stringów w publicznym API testu.

**Uwaga o "braku stringów":** eliminacja dotyczy wyłącznie publicznego DSL (ciała TEST_F).
Wewnątrz `HistorianEndpoint::addVector()` nadal użyte są literały `"recordCommand"` i
`"addVector"` — to implementacja adaptera testowego, nie protokół publiczny. Stringi są
scentralizowane w jednym miejscu (endpoint), zamiast powtarzać się w każdym teście.
Zmiana nazwy komendy w Engine wymaga edycji dokładnie jednej metody w endpoincie.

### 5.3 `FactoryEndpoint`

Publiczny member `FactorySpy`.

```cpp
class FactoryEndpoint {
public:
    Signal create(patterns::strategy::SortStrategyId id);
    void   receive(Signal sig);
    void   attach(ScenarioExecutor& ex);

private:
    ScenarioExecutor* executor_ = nullptr;
};
```

### 5.4 Wspólna konwencja

Każdy endpoint:
- ma metodę `attach(ScenarioExecutor&)` — wywoływaną przez `EngineTestBase::SetUp()`
- buduje `Signal` (wewnętrzny typ) z `payloadMatcher` — typesafe, bez stringów do identyfikacji
- deleguje do `ScenarioExecutor` (`executeStimulus` lub `declareExpectation`)

---

## 6. Topologia fixture przez dziedziczenie

Mechanizm pozostaje identyczny z obecnym. `EngineTestBase::SetUp()` wykrywa obecność
spy przez `dynamic_cast` i podłącza endpointy:

```cpp
void SetUp() override {
    engine_ = std::make_shared<Engine>();

    auto* h = dynamic_cast<HistorianSpy*>(this);
    auto* f = dynamic_cast<FactorySpy*>(this);
    channels_ = {h != nullptr, f != nullptr};

    executor_ = std::make_unique<ScenarioExecutor>(verifier_, channels_);

    // Engine endpoint
    engine.attach(*executor_, *engine_);

    // Historian channel
    if (h) {
        h->attachVerifier(verifier_);      // spy → verifier (bez zmian)
        h->historian.attach(*executor_);   // endpoint → executor
        historianKeeper_ = shared_ptr<IHistorian>(h, [](auto*){});
        engine_->setHistorian(historianKeeper_);
    } else {
        historianKeeper_ = make_shared<NullHistorian>();
        engine_->setHistorian(historianKeeper_);
    }

    // Factory channel
    if (f) {
        f->attachVerifier(verifier_);
        f->factory.attach(*executor_);
        factoryKeeper_ = shared_ptr<ISortStrategyFactory>(f, [](auto*){});
        engine_->setFactory(factoryKeeper_);
    } else {
        factoryKeeper_ = make_shared<SortStrategyFactory>();
        engine_->setFactory(factoryKeeper_);
    }
}
```

Topologie (bez żadnych zmian w mechanizmie):

```cpp
// Oba kanały
class EngineComponentTest : public EngineTestBase,
                             public HistorianSpy, public FactorySpy {};

// Tylko historian
class HistorianOnlyTest : public EngineTestBase, public HistorianSpy {};

// Tylko factory
class FactoryOnlyTest : public EngineTestBase, public FactorySpy {};
```

W teście `HistorianOnlyTest` identyfikator `factory` nie istnieje — kompilator sam
blokuje użycie nieaktywnego kanału. Jeśli `FactorySpy` nie jest bazą, `factory` nie
jest memberem.

---

## 7. Modelowanie `send()` i `receive()`

| Wywołanie | Semantyka | Akcja w executorze |
|-----------|-----------|-------------------|
| `engine.receive(d)` | Driver wysyła do Engine | `executeStimulus(Driver→Engine)` |
| `historian.receive(s)` | Deklaracja: Engine wysyła do Historian | `declareExpectation(Engine→Historian)` |
| `factory.receive(s)` | Deklaracja: Engine wysyła do Factory | `declareExpectation(Engine→Factory)` |
| `engine.send(d)` | Engine inicjuje spontaniczny sygnał | `executeStimulus(Engine→Engine)` lub inne źródło |

`receive()` na dowolnym endpoincie oznacza: **"deklaruję, że ten endpoint odbiera tę wiadomość"**.

Dla `engine.receive()` deklaracja = jednocześnie wykonanie (Driver jest aktorem testowym).
Dla `historian.receive()` / `factory.receive()` deklaracja = expectation (spy zbuforował, verifier porównuje).

`send()` jest symetryczne: **"deklaruję, że ten endpoint wysyła tę wiadomość"**.
Dla `engine.send()` — engine wygenerował sygnał bez zewnętrznego stimulus (przyszłość).
Wtedy nadawcą jest `Endpoint::Engine`, a nie `Endpoint::Driver`.

---

## 8. Odróżnienie expected od actual

| Rola | Obiekt | Źródło |
|------|--------|--------|
| **actual** | `SignalDescriptor` | Spy: `IHistorian::recordCommand()`, `ISortStrategyFactory::create()` |
| **expected** | `Signal` (Expectation) | Endpoint: `historian.addVector(...)`, `factory.create(...)` |

Spy nadal przechwytuje rzeczywiste wywołania i raportuje do `ScenarioVerifier` przez
istniejące `report()`. To się nie zmienia.

`HistorianEndpoint::addVector(data)` produkuje `Signal` z `payloadMatcher` — to jest
expected. Verifier porównuje expected vs actual.

Spy i Endpoint żyją w tym samym obiekcie (`HistorianSpy`), ale są to dwa odrębne
mechanizmy z osobnymi odpowiedzialnościami:

- `HistorianSpy` (spy, implementuje `IHistorian`) — przechwytuje actuals
- `HistorianSpy::historian` (`HistorianEndpoint`) — deklaruje expectations

Nie ma ryzyka pomylenia, bo są to różne metody: spy override'uje `recordCommand()`,
endpoint udostępnia `historian.addVector()` i `historian.receive()`.

---

## 9. Strict ordering bez `driver.run()`

Mechanizm pozostaje ten sam co w Mode 2 `ScenarioVerifier`. Zmienia się tylko miejsce
wywołania:

**Dziś:**
```
EngineDriver::run() → verifier_.beginStep() / endStepCollection() / matchExpectation() / finalizeStep()
```

**Docelowo:**
```
EngineEndpoint::receive()    → ScenarioExecutor::executeStimulus()
                                 → verifier_.beginStep() / endStepCollection()
HistorianEndpoint::receive() → ScenarioExecutor::declareExpectation()
                                 → verifier_.matchExpectation()
~ScenarioExecutor()          → verifier_.finalizeStep()
```

`ScenarioExecutor` to `EngineDriver` pozbawiony `run()` i z nowym publicznym API.

Kolejność jest zapewniona przez:
1. `stepActuals_` — zbuforowane synchronicznie podczas wykonania stimulus
2. `nextActualInStep_` — indeks, każde `matchExpectation()` go inkrementuje
3. `finalizeStep()` — raportuje nadmiarowe actuals

Strict ordering nie wymaga `driver.run()` — wynika ze struktury wywołań w TEST_F.

---

## 10. Obsługa spontanicznych / asynchronicznych sygnałów

### Kluczowe rozróżnienie: logiczny nadawca vs mechanizm triggera

Dla `engine.receive(d)` te dwie rzeczy są tożsame:
- **Logiczny nadawca:** Driver (to Driver wywołuje Engine)
- **Mechanizm triggera:** to samo wywołanie (`e.onSessionEvent(...)`)

Dla `engine.send()` te dwie rzeczy są **rozdzielne**:
- **Logiczny nadawca:** Engine (to Engine inicjuje sygnał, np. wskutek awarii hardware)
- **Mechanizm triggera:** coś zewnętrznego wobec Engine — fake hardware, callback, timer —
  co test musi wstrzyknąć osobno, zanim Engine wyemituje sygnał

Mylenie tych dwóch ról prowadzi do błędnego modelu, w którym `engine.send()` oznacza
"test każe Engine wysłać coś do siebie". To nie jest właściwa interpretacja.

### Proponowana sygnatura `send()`

```cpp
// engine.send(trigger, descriptor)
//
// trigger    — co test robi, żeby sprowokować Engine do emisji sygnału
//              (może być: wywołanie fake hardware, inject callbacka, tick clocka, itp.)
// descriptor — opisuje logiczny sygnał inicjowany przez Engine
//              (do sequence diagram i weryfikacji topologii kroku)

engine.send(
    [&]{ fakeHardware.triggerMotorFailure(); },      // trigger (test-specific)
    engine.hardwareFailure(ErrorCode::MotorFailure)   // logical signal (Engine sends)
);
historian.receive(historian.recordError(ErrorCode::MotorFailure));
gui.receive(gui.showError(ErrorCode::MotorFailure));
```

Trigger jest lambdą `void()` (nie `void(Engine&)`) — może odwoływać się do dowolnych
fake/stub obiektów z fixture. Engine.send() otwiera krok (beginStep), wywołuje trigger,
a framework obserwuje co Engine emituje do collaboratorów.

W sequence diagram nadawcą jest `[Engine]`, nie `[Driver]`:
```
                [Engine] ---hardwareFailure---> [HistorianSpy]
                [Engine] ---hardwareFailure---> [GuiSpy]
```

### Kontrakt: trigger musi być synchroniczny

Obecna implementacja `ScenarioVerifier` (Mode 2) zbiera actuals synchronicznie
podczas wykonania stimulus. `engine.send()` musi zachować tę właściwość —
trigger jest synchroniczny, Engine emituje sygnały zanim trigger wraca.

Dla asynchronicznych zdarzeń (wątek sprzętowy, timer) wymagana jest osobna analiza
— poza zakresem tego propozalu.

### Status: przyszłościowe rozszerzenie

`engine.send()` nie jest implementowane w bieżącym propozalu implementacyjnym.
Architektura `ScenarioExecutor::executeStimulus()` przyjmuje `from` i `to` jako
parametry — dodanie `send()` to nowa metoda `EngineEndpoint` + nowa lambda-based
sygnatura, bez zmian w `ScenarioVerifier` ani w Spies.

---

## 11. Zachowanie `ScenarioVerifier`, Spies, sequence logging

### ScenarioVerifier

**Bez zmian.** Mode 2 (beginStep / endStepCollection / matchExpectation / finalizeStep)
pozostaje główną ścieżką. `ScenarioFrameworkTest` przechodzi bez modyfikacji.

Jedyna potencjalna zmiana: dodanie guardu w `beginStep()` (zob. TODO w kodzie —
odrębna kwestia, niezwiązana z tym proposalem).

### Spies

`HistorianSpy` i `FactorySpy` otrzymują nowego publicznego membera (`historian`, `factory`).
Implementacja `IHistorian` / `ISortStrategyFactory` pozostaje bez zmian.
`attachVerifier()` pozostaje bez zmian.

Nowy `attachEndpointExecutor(ScenarioExecutor&)` — lub połączone z `attachVerifier()`.

### SequenceLog

Bez zmian. `ScenarioExecutor::executeStimulus()` loguje stimulus (tak jak `EngineDriver`).
`ScenarioExecutor::declareExpectation()` loguje expectation (tak jak `EngineDriver`).

---

## 12. Zmiany `SignalRole`

`SignalRole::Stimulus|Expectation` pozostaje jako typ wewnętrzny `Signal`.

W nowym publicznym API `SignalRole` jest **niejawny**:
- `engine.receive()` → wewnętrznie otwiera krok (Stimulus path)
- `historian.receive()` → wewnętrznie `matchExpectation()` (Expectation path)

Użytkownik testu nigdy nie widzi `SignalRole`. Jest to implementation detail.

`Signal` jako typ (z `payloadMatcher`, `from`, `to`, `name`) pozostaje — jest zwracany
przez metody endpointów (`historian.addVector(...)`, `factory.create(...)`).

---

## 13. Proponowane typy i odpowiedzialności

```
ScenarioExecutor        — koordynacja kroków; zastępuje EngineDriver jako silnik
EngineEndpoint          — test handle dla Engine; stimulus execution
HistorianEndpoint       — test handle dla Historian; typed expectation builders
FactoryEndpoint         — test handle dla Factory; typed expectation builders
HistorianSpy            — bez zmian (spy) + nowy member: historian: HistorianEndpoint
FactorySpy              — bez zmian (spy) + nowy member: factory: FactoryEndpoint
EngineTestBase          — nowy public member: engine: EngineEndpoint
                          nowy private member: executor_: unique_ptr<ScenarioExecutor>
ScenarioVerifier        — bez zmian
Signal / SignalDescriptor — bez zmian
SequenceLog             — bez zmian
```

### ScenarioExecutor — szkic interfejsu

```cpp
class ScenarioExecutor {
public:
    ScenarioExecutor(ScenarioVerifier& verifier, ActiveChannels channels);
    ~ScenarioExecutor();  // finalizePendingStep()

    // Wywołany przez EngineEndpoint::receive() / send()
    void executeStimulus(Endpoint from, Endpoint to,
                         const std::string& name,
                         std::function<void(patterns::engine::Engine&)> action,
                         patterns::engine::Engine& engine);

    // Wywołany przez HistorianEndpoint::receive() / FactoryEndpoint::receive()
    void declareExpectation(Signal sig);

private:
    void finalizePendingStep();

    ScenarioVerifier& verifier_;
    ActiveChannels    channels_;
    bool              stepOpen_ = false;
};
```

---

## 14. Przykładowe fragmenty API w C++

### EngineEndpoint::receive() — szkic implementacji

```cpp
void EngineEndpoint::receive(Descriptor d) {
    // Koordynacja (beginStep) jest w executorze
    executor_->executeStimulus(
        Endpoint::Driver, Endpoint::Engine, d.name, d.action, *engine_
    );
}
```

### HistorianEndpoint::addVector() — szkic implementacji

```cpp
Signal HistorianEndpoint::addVector(std::vector<int> data) {
    return {
        .role   = SignalRole::Expectation,
        .name   = "recordCommand",
        .from   = Endpoint::Engine,
        .to     = Endpoint::Historian,
        .payloadMatcher = [data](const std::any& payload) -> bool {
            const auto* cmd =
                std::any_cast<patterns::historian::CommandHistory>(&payload);
            return cmd
                && cmd->commandName == "addVector"
                && cmd->data        == data;
        }
    };
}
```

Identyczna logika co dziś w `expectHistorianCommand("addVector", data)` —
ale bez stringa w publicznym API testu.

### HistorianEndpoint::receive() — szkic implementacji

```cpp
void HistorianEndpoint::receive(Signal sig) {
    executor_->declareExpectation(std::move(sig));
}
```

### ScenarioExecutor::executeStimulus() — szkic

```cpp
void ScenarioExecutor::executeStimulus(
        Endpoint from, Endpoint to, const std::string& name,
        std::function<void(patterns::engine::Engine&)> action,
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
```

---

## 15. Przykłady kompletnych TEST_F

### EngineComponentTest (oba kanały)

```cpp
TEST_F(EngineComponentTest, AddVector)
{
    engine.receive(engine.addVector({1, 2, 3}));
    historian.receive(historian.addVector({1, 2, 3}));
}

TEST_F(EngineComponentTest, SortVector)
{
    engine_->addVector({5, 3, 1});                // precondition

    engine.receive(engine.sortVector(0));
    historian.receive(historian.sortVector());
}

TEST_F(EngineComponentTest, SetStrategy)
{
    engine.receive(engine.strategyChange(SortStrategyId::Descending));
    factory.receive(factory.create(SortStrategyId::Descending));
    historian.receive(historian.setSortStrategy());
}

TEST_F(EngineComponentTest, PublishSnapshot)
{
    engine_->addVector({1, 2, 3});                // precondition

    engine.receive(engine.publishSnapshot());
    historian.receive(historian.publishSnapshot(1));
}

TEST_F(EngineComponentTest, FullEngineFlow)
{
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

### HistorianOnlyTest (factory kanał nieaktywny)

```cpp
TEST_F(HistorianOnlyTest, SetStrategy)
{
    engine.receive(engine.strategyChange(SortStrategyId::Descending));
    // factory.receive(...) nie kompiluje się — FactorySpy nie jest bazą,
    //                       `factory` nie istnieje w tym fixture
    historian.receive(historian.setSortStrategy());
    // expectation factory.create() jest po prostu nieobecna — nie ma "Signal not received"
    // bo fixture nie deklaruje tego kanału
}
```

Różnica wobec obecnego `Scenarios::SetStrategy`: nie trzeba pisać `expectFactoryCreate()`
i liczyć na filtrowanie przez `channels_`. Brak deklaracji = brak oczekiwania.

Uwaga: to wymaga osobnych testów dla `HistorianOnlyTest` i `FactoryOnlyTest` (nie można
reużyć jednego `Scenarios::SetStrategy` dla obu topologii). Patrz sekcja 17 (trade-off).

### Self-test frameworka

```cpp
TEST_F(EngineComponentTest, MalformedScenario_ExpectationBeforeStimulus)
{
    EXPECT_NONFATAL_FAILURE(
        historian.receive(historian.addVector({1, 2, 3})),
        "Expectation before stimulus"
    );
}
```

---

## 16. Plan migracji obecnych scenariuszy

### Faza 1 — Addytywna (bez breaking changes)

- Dodaj `ScenarioExecutor` jako refaktoryzację `EngineDriver` (te same pola, nowe publiczne API)
- Dodaj `EngineEndpoint`, `HistorianEndpoint`, `FactoryEndpoint`
- Dodaj `historian` jako member `HistorianSpy`, `factory` jako member `FactorySpy`
- Dodaj `engine` jako member `EngineTestBase`
- `EngineDriver` nadal istnieje i kompiluje się (backward compat)
- Napisz nowe testy używając nowego API

### Faza 2 — Migracja testów

- Migruj istniejące `TEST_F` jeden po drugim do nowego API
- `Scenarios::*` i `driver.run()` nadal działają

### Faza 3 — Cleanup

- Usuń `EngineDriver` i `Scenarios::*` gdy żaden test już ich nie używa
- Usuń `SignalRole` z publicznego API (staje się private w `ScenarioExecutor`)

---

## 17. Backward compatibility podczas migracji

`EngineDriver` i `Scenarios::*` mogą współistnieć z nowym API przez cały czas fazy 1 i 2.
Nie ma breaking change — oba API produkują identyczne wywołania `ScenarioVerifier`.

Jedyna niekompatybilność: `Scenarios::SetStrategy` dla `HistorianOnlyTest` z nowym API
wymaga osobnego testu (brak `factory.receive(...)` zamiast filtrowania przez `channels_`).
Jest to trade-off: czytelność testu vs reużywalność scenariusza. Patrz sekcja 20.

---

## 18. Istniejące testy frameworka — co rozszerzyć

`ScenarioFrameworkTest` — **bez zmian**. Testuje `ScenarioVerifier` bezpośrednio.

`EngineComponentTest` — nowe regression testy dla nowego API (wszystkie obecne testy
przeniesione lub uzupełnione):

| Test do dodania | Co weryfikuje |
|----------------|---------------|
| `EndpointApi_ExpectationBeforeStimulus` | `historian.receive()` bez wcześniejszego `engine.receive()` → błąd |
| `EndpointApi_UnexpectedSignal` | stimulus bez expectations → "Unexpected signal" |
| `EndpointApi_SignalNotReceived` | expectation bez actuals → "Signal not received" |
| `EndpointApi_PayloadMismatch` | złe dane → "payload mismatch" |
| `EndpointApi_WrongOrder` | expectations w złej kolejności → "Unexpected signal" |
| `EndpointApi_MultiStep` | wiele kroków w jednym TEST_F → poprawna izolacja |
| `EndpointApi_StepClosedByNextStimulus` | drugi `engine.receive()` zamyka poprzedni krok |

---

## 19. Nowe regression testy dla nowego modelu

Regression testy endpointów (testują `ScenarioExecutor` + endpointy bezpośrednio,
bez pełnego Engine):

```cpp
// Podobnie jak ScenarioFrameworkTest ale przez nowe API
class EndpointApiTest : public EngineTestBase, public HistorianSpy, public FactorySpy {};

TEST_F(EndpointApiTest, beginStep_CalledTwice_WithoutFinalize_ReportsError)
{
    engine.receive(engine.addVector({1, 2, 3}));
    // drugi receive() bez expectations → powinien finalizeStep()
    // i nie powinien zgubić actuals (TODO guard w beginStep)
    EXPECT_NONFATAL_FAILURE(
        engine.receive(engine.addVector({1, 2, 3})),
        "Unexpected signal"   // ze starego kroku
    );
}
```

---

## 20. Ryzyka i trade-offy

### Trade-off: reużywalność scenariuszy vs czytelność testu

**Dziś:** `Scenarios::SetStrategy(id)` działa dla wszystkich topologii dzięki filtrowi `channels_`.
Jeden scenariusz = wiele topologii.

**Docelowo:** `HistorianOnlyTest::SetStrategy` nie ma `factory.receive(...)` — brak
tej linii oznacza brak oczekiwania (poprawne zachowanie). Ale oznacza to,
że nie można reużyć jednego scenariusza dla `EngineComponentTest` i `HistorianOnlyTest`.

**Rozwiązanie:** Shared scenarios mogą być wyrażone jako metody pomocnicze fixture'a:
```cpp
void setStrategyScenario(SortStrategyId id) {
    engine.receive(engine.strategyChange(id));
    if constexpr (/* has FactorySpy */) factory.receive(factory.create(id));
    historian.receive(historian.setSortStrategy());
}
```
Ale to dodaje złożoność. Alternatywnie: zaakceptować duplikację testów między topologiami
jako cenę za czytelność.

### Ryzyko: `HistorianEndpoint` mapowanie domain → protocol

Metody `addVector()`, `sortVector()`, `setSortStrategy()` mapują na string
`commandName` wewnętrznie. Zmiana nazwy komendy w Engine wymaga zmiany w endpoincie.
Jest to jednak jawna zależność w jednym miejscu zamiast rozrzuconych stringów w testach.

### Ryzyko: Rozmiar publicznego interfejsu endpointów

Każda nowa operacja Engine wymaga nowej metody w `HistorianEndpoint`/`FactoryEndpoint`.
Jest to pozytywne (kompilator wychwytuje brakujące mapowania), ale wymaga synchronizacji
z rozwojem Engine.

### Ryzyko: `engine_` vs `engine` — dwa obiekty

`EngineTestBase` ma `engine_` (shared_ptr, private) i `engine` (EngineEndpoint, public).
Preconditions muszą być nadal ustawiane przez `engine_->addVector(...)`:
```cpp
TEST_F(EngineComponentTest, SortVector)
{
    engine_->addVector({5, 3, 1});  // precondition (raw access)
    engine.receive(engine.sortVector(0));  // scenariusz
    historian.receive(historian.sortVector());
}
```
To rozróżnienie między "setup state" a "scenariusz" jest czytelne i pożądane.
`engine_` pozostaje protected, `engine` jest public — intencja jest jasna.

### Ryzyko: `TearDown` i lifetime `ScenarioExecutor`

`ScenarioExecutor` musi być zniszczony **przed** spy sub-objectami (które są bazami fixture).
Jeśli `executor_` jest `unique_ptr` w `EngineTestBase`, jego destruktor wywoła `finalizeStep()`.
`TearDown()` musi jawnie zresetować `engine_` (jak dziś) PRZED zniszczeniem `executor_`.
Kolejność destrukcji musi być zapewniona przez `TearDown()`, nie przez kolejność deklaracji memberów.

---

## Recommended architecture

### Rekomendacja

**Immediate-execution endpoint model** z `ScenarioExecutor` jako wewnętrznym koordynatorem.

Uzasadnienie:

1. **Brak ukrytych mechanizmów.** Każde `engine.receive()` wykonuje się natychmiast.
   Błędy są raportowane w miejscu wywołania, nie w `TearDown`. Mental model jest prosty:
   "piszę komunikację, framework weryfikuje ją na bieżąco".

2. **Minimalna zmiana architektury.** `ScenarioVerifier`, `Spies`, `SequenceLog`, `ActiveChannels`
   pozostają bez zmian. `ScenarioExecutor` to `EngineDriver` ze zmienionym interfejsem wywołującym.
   Istniejące 84 testy nie są dotknięte w fazie 1.

3. **Typesafe bez metaprogramowania.** Typed descriptors (`HistorianEndpoint::addVector(...)`)
   to proste metody zwracające `Signal` z lambdą. Kompilator sprawdza nazwy — nie ma stringów.
   Żadnych template metaprogramming, żadnych CRTP, żadnych reflection.

4. **Topologia przez dziedziczenie — bez zmian.** `dynamic_cast` w `SetUp()` pozostaje.
   Jeśli `FactorySpy` nie jest bazą, `factory` nie istnieje. Kompilator blokuje
   użycie nieaktywnego kanału — to lepsza gwarancja niż filtrowanie runtime (`channels_`).

5. **Symetryczny model send/receive.** `engine.send()` to naturalne rozszerzenie —
   ta sama infrastruktura, inny `from` endpoint. Nie wymaga przebudowy.

6. **Czytelność TEST_F.** Każdy krok komunikacji to jedna para linii. Pełen flow Engine
   jest czytelny jako sekwencja "kto do kogo" bez znajomości wewnętrznych stringów protokołu.

### Co NIE jest rekomendowane

- **Builder/deferred execution (TearDown plays):** błędy zbyt daleko od deklaracji, trudny debugging
- **Makra DSL:** ukrywają mechanizm, utrudniają debugowanie w IDE
- **Single unified `receive()` z szablonami:** nadmierna złożoność dla niewielkiej korzyści
- **Usunięcie `Scenarios::*` od razu:** backward compat przez fazy 1-2 jest ważny

### Kolejne kroki

Po akceptacji propozalu: odrębny **proposal implementacyjny** opisujący dokładne
sygnatury klas, pliki, kolejność zmian i coverage nowych testów.
