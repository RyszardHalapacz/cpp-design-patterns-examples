# Wzorce Projektowe w C++

![CI](https://github.com/RyszardHalapacz/cpp-design-patterns-examples/actions/workflows/ci.yml/badge.svg)

Praktyczna demonstracja klasycznych wzorców projektowych (GoF) zaimplementowanych w nowoczesnym C++ (C++23).
Projekt jest zbudowany jako **działający, kompilujący się kod** — nie zbiór snippetów ani pseudokodu.
Każdy wzorzec ma własne klasy, testy jednostkowe i materiały teoretyczne.

> Wersja angielska: [README.md](README.md)

---

## Wzorce projektowe

| Wzorzec | Klasa | Plik |
|---|---|---|
| **Singleton** | `ServiceLocator` | `components/core/include/patterns/services/ServiceLocator.hpp` |
| **Service Locator** | `ServiceLocator`, `appLogger()`, `logApp()` | `components/core/include/patterns/services/ServiceLocator.hpp` |
| **Strategy** | `ISortStrategy`, `AscendingSortStrategy`, `DescendingSortStrategy`, `BubbleSortStrategy` | `components/core/include/patterns/strategy/` |
| **Factory** | `SortStrategyFactory` | `components/core/include/patterns/strategy/SortStrategyFactory.hpp` |
| **Observer** | `ISessionObserver`, `Engine`, `SessionAuditObserver` | `components/core/include/patterns/observer/`, `include/patterns/session/` |
| **Facade** | `SessionManagement` | `include/patterns/session/SessionManagement.hpp` |
| **Template Method** | `SessionCoordinator`, `EngineSessionCoordinator` | `include/patterns/session/SessionCoordinator.hpp` |
| **Builder** | `CommandBatchBuilder` | `components/dummy_gui/include/patterns/gui/CommandBatchBuilder.hpp` |
| **Command** | `ICommand`, `AddVectorCommand`, `SortVectorCommand`, `PrintDataCommand` | `components/core/include/patterns/gui/ICommand.hpp` |
| **Adapter** | `DummyGuiAdapter`, `IGui` | `components/dummy_gui/include/patterns/gui/DummyGuiAdapter.hpp` |

---

## Czego uczy ten projekt (poza wzorcami)

W skrócie projekt demonstruje również:

- **C++23** — `std::expected`, operacje monadyczne, concepts
- **Własność i czas życia** — grafy własności `shared_ptr`/`weak_ptr`, Rule of Zero, `weak_ptr` jako mechanizm odłączania
- **Wielokomponentowy CMake** — osobne biblioteki, `FetchContent`, właściwe umieszczenie `enable_testing()`
- **Wzorce GoF współpracujące** — wszystkie wzorce zintegrowane w jeden spójny system, nie izolowane snippety
- **Testy jednostkowe i komponentowe** — unit testy Google Test oraz harness komponentowy dla `Engine`
- **CI i dokumentacja** — GitHub Actions, diagramy Mermaid, dwujęzyczne materiały wykładowe

---

### Organizacja projektu C++
- Wielokomponentowa struktura: `components/core/`, `components/engine/` i `components/dummy_gui/` jako osobne biblioteki statyczne/interfejsowe
- Podział na `include/` (nagłówki) i `src/` (implementacje) wewnątrz każdego komponentu
- Zagnieżdżone przestrzenie nazw (`namespace patterns::services`)
- Forward declarations do rozrywania cykli zależności między klasami

### CMake
- Budowanie wielokomponentowe: `core_lib`, `engine_lib` (INTERFACE) i `dummy_gui_lib` jako osobne cele `add_library`
- `enable_testing()` wywołane przed wszystkimi `add_subdirectory` — dzięki temu CTest widzi testy z każdego komponentu
- `FetchContent` do pobierania Google Test i yaml-cpp bez instalacji systemowej
- Wstrzykiwanie wersji przez `configure_file` (`.yml.in` → `.yml`)
- Makra ze ścieżkami manifestów w czasie kompilacji (`ENGINE_MANIFEST_PATH`, `DUMMY_GUI_MANIFEST_PATH`)
- Testy w osobnych podkatalogach z własnym `CMakeLists.txt`

### Google Test
- `TEST` — podstawowe testy jednostkowe
- `TEST_F` — testy z fixture (współdzielony `SetUp`)
- `TEST_P` — testy sparametryzowane (jeden kod testu, wiele zestawów danych)
- Przechwytywanie `stdout` przez `testing::internal::CaptureStdout()`
- 93 testy w 8 plikach testowych — weryfikacja na żywo: `ctest --test-dir build`

### Cechy C++23
- **`std::expected<T, E>`** — metody `ServiceLocator` i `SortStrategyFactory` zwracają `expected` zamiast rzucać wyjątkami; wywołujący obsługuje błędy jawnie
- **Operacje monadyczne** — `and_then`, `transform`, `or_else` połączone łańcuchem w `SessionCoordinator::establish()` tworzą liniowy, czytelny potok obsługi błędów
- Brak wyjątków w warstwie serwisów i fabryk — wszystkie ścieżki błędów są typowo bezpieczne i kompozytowalne

### Szablony i idiomy C++
- `ServiceLocator` z `std::type_index` jako kluczem rejestru w czasie wykonania
- **`concept ServiceType`** — `std::derived_from<TService, IService>` ogranicza parametry szablonu `ServiceLocator` w miejscu deklaracji, zastępując `static_assert` czytelniejszymi komunikatami kompilatora
- **Meyers Singleton** — `static` lokalna zmienna w metodzie `instance()`
- **Callbacki `std::function`** — `DummyGui` przechowuje sloty `std::function` zamiast surowych wskaźników do sesji; `Configurator` podpina lambdy przechwytujące `weak_ptr<SessionManagement>`
- **Kolekcja obserwatorów `weak_ptr`** — `SessionManagement` trzyma `vector<weak_ptr<ISessionObserver>>`; wygasłe obserwatory są automatycznie usuwane, a duplikaty odrzucane
- **`weak_ptr` jako mechanizm odłączania** — `Application` jest właścicielem `shared_ptr<IHistorian>`; `EngineSessionCoordinator` i `Engine` trzymają po `weak_ptr`; `disableHistorian()` czyści `weak_ptr` Engine'u — historyk przestaje nagrywać bez zniszczenia obiektu; `enableHistorian()` ponownie podłącza istniejącą instancję
- **Model własności** — `Application` jest właścicielem wszystkich obiektów najwyższego poziomu jako `shared_ptr` / `unique_ptr`; `SessionManagement` trzyma `weak_ptr<Engine>`; czas życia `SessionAuditObserver` jest kontrolowany przez `Application`, nie przez sesję
- **Rule of Zero** — `DummyGuiAdapter` nie ma destruktora; `unique_ptr<DummyGui>` automatycznie wywołuje `deleteGUI()` dzięki specjalizacji `std::default_delete<DummyGui>`
- **C-style API fabryki** (`makeGUI` / `deleteGUI`) — symulacja interfejsów bibliotek C (wzorzec SDL, curl); ukryta za `IGui` przez Adapter

---

## Testy komponentowe — Engine jako SUT

Poza klasycznymi testami jednostkowymi projekt zawiera test komponentowy dla `Engine`
([`components/engine/component_test/EngineComponentTest.cpp`](components/engine/component_test/EngineComponentTest.cpp)).

Testy jednostkowe weryfikują izolowane klasy w pełnej izolacji od ich współpracowników.
Test komponentowy traktuje `Engine` jako kompletny **System Under Test (SUT)**: asercje dotyczą
wyłącznie zachowania obserwowalnego na granicy komponentu — `Engine` nie jest podklasowany,
nie ma dostępu do jego prywatnego stanu i żadne wewnętrzne wywołania nie są przechwytywane.

### Architektura testu

```
              EngineTestBase (::testing::Test)
                     │
        ┌────────────┼────────────┐
        │            │            │
 EngineComponent  Historian    Factory
 Test             OnlyTest     OnlyTest
 ─────────────    ─────────    ──────────
 HistorianSpy     HistorianSpy  FactorySpy
 FactorySpy
```

Produkcyjne zależności są zastąpione dublerami podłączonymi przez te same publiczne interfejsy:

```
IHistorian           ──── HistorianSpy         (kanał ON)
                     └─── NullHistorian         (kanał OFF)

ISortStrategyFactory ──── FactorySpy           (kanał ON)
                     └─── SortStrategyFactory   (kanał OFF, prawdziwa fabryka)
```

- **`HistorianSpy`** — punkt obserwacyjny na granicy `IHistorian`; raportuje każde
  wywołanie `recordCommand` i `publishSnapshot` do `ScenarioVerifier`
- **`FactorySpy`** — punkt obserwacyjny na granicy `ISortStrategyFactory`; raportuje
  wywołania `create()` do `ScenarioVerifier` i deleguje do prawdziwej fabryki
- **`ScenarioExecutor`** — koordynuje cykl życia kroków; wywoływany przez obiekty endpoint
- **`ScenarioVerifier`** — wymusza ścisłe, uporządkowane dopasowanie sygnałów

### Endpoint DSL — wykonywalny diagram sekwencji

Testy są zapisane jako sekwencja wywołań `receive()` na trzech typowanych obiektach endpoint:

```cpp
engine    // EngineEndpoint    — dostępny we wszystkich fixture'ach (member EngineTestBase)
historian // HistorianEndpoint — dostępny gdy fixture dziedziczy po HistorianSpy
factory   // FactoryEndpoint   — dostępny gdy fixture dziedziczy po FactorySpy
```

`engine.receive(descriptor)` dostarcza **bodziec** do `Engine` — zawsze jest wykonany.
`historian.receive(signal)` i `factory.receive(signal)` deklarują **oczekiwania** — są
dopasowywane do sygnałów, które faktycznie przekroczyły granicę komponentu w danym kroku.

Ciało testu czyta się jak **wykonywalny diagram sekwencji**:
każda linia `engine.receive()` to bodziec; linie poniżej to sygnały, które `Engine` musi
wysłać w odpowiedzi, zadeklarowane w dokładnej kolejności ich wystąpienia:

```cpp
TEST_F(EngineComponentTest, FullEngineFlow) {
    engine.receive(engine.addVector({1, 2, 3}));
    historian.receive(historian.addVector({1, 2, 3}));

    engine.receive(engine.sortVector(0));
    historian.receive(historian.sortVector());

    engine.receive(engine.strategyChange(SortStrategyId::Descending));
    factory.receive(factory.create(SortStrategyId::Descending));   // dwaj współpracownicy,
    historian.receive(historian.setSortStrategy());                 // w dokładnej kolejności

    engine.receive(engine.publishSnapshot());
    historian.receive(historian.publishSnapshot(1));
}
```

`strategyChange` angażuje dwóch współpracowników w jednym kroku — obaj muszą być zadeklarowani, w kolejności.

### Ścisła weryfikacja z zachowaniem kolejności

Weryfikacja jest **ścisła i uporządkowana w obrębie każdego kroku**:

| Błąd | Wyzwalacz | Komunikat |
|---|---|---|
| Niezadeklarowany sygnał z aktywnego kanału | actual bez oczekiwania | `"Unexpected signal"` |
| Sygnały w złej kolejności | niezgodność metadanych | `"Unexpected signal"` |
| Oczekiwany sygnał nigdy nie nadszedł | oczekiwanie niespełnione | `"Signal not received"` |

Pusta lista oczekiwań dla aktywnego kanału oznacza **ścisłe zero** — nie "nie obchodzi mnie":

```cpp
engine.receive(engine.addVector({1, 2, 3}));
// historian.receive() pominięte — oba kanały aktywne, zero oczekiwań
// Engine wywołuje historian.recordCommand() → "Unexpected signal"
```

### Strukturalne diagnostyki payload

Niezgodności payload generują strukturalne diff'y na poziomie pól, nie tylko pass/fail:

```
Signal payload mismatch

Expected signal:
  from:         Engine
  to:           Historian
  name:         recordCommand
  payload type: CommandHistory

Actual signal:
  from:         Engine
  to:           Historian
  name:         recordCommand
  payload type: CommandHistory

Mismatches:
  payload.data[0]:
    expected: 99
    actual:   1
  payload.data[1]:
    expected: 99
    actual:   2
  payload.data[2]:
    expected: 99
    actual:   3
```

Każdy `payloadMatcher` zwraca `PayloadMatchResult = std::expected<void, PayloadMismatch>`.
`SignalMismatchFormatter` przekształca strukturalny błąd w powyższy komunikat, przekazywany
do `ADD_FAILURE()`.

### Częściowe oczekiwania na snapshot

`publishSnapshot` niesie pełną strukturę `EngineSnapshot`. `ExpectedEngineSnapshot` pozwala
testowi sprawdzić tylko wybrane pola — nieokreślone pola nie są weryfikowane:

```cpp
historian.receive(historian.publishSnapshot(1));                                    // tylko vectorCount
historian.receive(historian.publishSnapshot({.running = true, .vectorCount = 2})); // dwa pola
historian.receive(historian.publishSnapshot());                                     // dowolny snapshot
```

### Topologia przez fixture

Aktywne kanały są określone w czasie kompilacji przez listę klas bazowych fixture'a:

```cpp
class EngineComponentTest : public EngineTestBase,
                             public HistorianSpy,    // kanał historian ON
                             public FactorySpy   {}; // kanał factory ON

class HistorianOnlyTest   : public EngineTestBase,
                             public HistorianSpy  {}; // tylko historian aktywny

class FactoryOnlyTest     : public EngineTestBase,
                             public FactorySpy    {}; // tylko factory aktywny
```

`EngineTestBase::SetUp()` używa `dynamic_cast` do wykrycia, które spy'e eksponuje konkretny
fixture, i buduje `ActiveChannels` na tej podstawie:

```cpp
auto* h = dynamic_cast<HistorianSpy*>(this);
auto* f = dynamic_cast<FactorySpy*>(this);
channels_ = {h != nullptr, f != nullptr};
```

Oczekiwania dla nieaktywnych kanałów są cicho pomijane — to samo ciało scenariusza działa
bez zmian we wszystkich topologiach fixture.

### Czas życia — weak_ptr i keepery

`Engine` przechowuje `historian` i `factory` przez `weak_ptr`. Spy'e są pod-obiektami klasy
bazowej fixture'a (nie są alokowane na stercie). `EngineTestBase` trzyma `historianKeeper_`
i `factoryKeeper_` — `shared_ptr`y z no-op deleter'em, które utrzymują blok kontrolny przez
cały czas życia fixture'a:

```cpp
historianKeeper_ = std::shared_ptr<IHistorian>(h, [](auto*){});  // no-op deleter
engine_->setHistorian(historianKeeper_);   // weak_ptr Engine'u ma żywy blok kontrolny
```

`TearDown()` resetuje `engine_` zanim destruktory klas bazowych zniszczą pod-obiekty spy:

```
1. TearDown()      → engine_.reset()   Engine zniszczony; keepery i spy'e nadal żyją
2. ~FactorySpy()
3. ~HistorianSpy()
4. ~EngineTestBase()                   keepery zniszczone
```

### Integracja z Google Test

Testy używają `TEST_F` ze współdzielonym `SetUp`/`TearDown` w każdym fixture'ze. Brak GMock —
brak obiektów mock, brak `EXPECT_CALL`, brak action sequence. `EXPECT_NONFATAL_FAILURE`
jest używany przez framework self-testy do weryfikacji, że naruszenia kontraktu generują
oczekiwane komunikaty błędów.

### Uruchomienie testu komponentowego

```bash
# Budowanie
cmake --build build --parallel

# Uruchomienie wszystkich testów komponentowych
./build/components/engine/component_test/engine_component_tests

# Uruchomienie konkretnego fixture'a
./build/components/engine/component_test/engine_component_tests \
    --gtest_filter="HistorianOnlyTest.*"
```

Implementacja jest celowo synchroniczna, co oddziela model testowania od problemów
z współbieżnością. Architektura pozwala w przyszłości przenieść `Engine` do osobnego wątku,
zastępując bezpośrednie wywołania kolejkami komunikatów i oczekiwaniami z timeoutem.

---

## Struktura projektu

```
wzorce/
├── components/
│   ├── core/                      # Biblioteka statyczna — wspólna podstawa
│   │   ├── include/patterns/
│   │   │   ├── services/          # Logger, FileLogger, DoSomething, ServiceLocator (C++23)
│   │   │   ├── strategy/          # ISortStrategy, 3 implementacje, SortStrategyFactory
│   │   │   ├── observer/          # ISessionObserver, SessionEvent
│   │   │   ├── gui/               # IGui, ICommand + konkretne komendy
│   │   │   ├── historian/         # EngineHistorian, CommandHistory, EngineSnapshot
│   │   │   └── manifest/          # ManifestWriter
│   │   └── src/
│   ├── engine/                    # Biblioteka interfejsowa — komponent Engine
│   │   ├── include/patterns/
│   │   │   └── engine/            # BasicEngine<Writer>, Engine (alias)
│   │   ├── engine.yml.in
│   │   ├── tests/                 # test_engine.cpp (cykl życia, zdarzenia sesji)
│   │   └── component_test/        # EngineComponentTest.cpp (Engine jako SUT)
│   └── dummy_gui/                 # Biblioteka statyczna — komponent GUI
│       ├── include/patterns/
│       │   └── gui/               # DummyGui (C-style API), DummyGuiAdapter, CommandBatchBuilder
│       ├── src/
│       └── tests/                 # test_gui.cpp (CommandBatchBuilder, DummyGuiAdapter)
├── include/patterns/              # Nagłówki na poziomie aplikacji
│   ├── app/                       # Application (configure + run)
│   └── session/                   # SessionManagement, SessionCoordinator, SessionAuditObserver
├── src/                           # Implementacje aplikacji + main.cpp
├── tests/                         # Testy jednostkowe aplikacji
│   ├── test_services.cpp          # Logger, FileLogger, ServiceLocator (API std::expected)
│   ├── test_strategy.cpp          # Wszystkie strategie sortowania + fabryka
│   ├── test_session.cpp           # SessionManagement, SessionCoordinator, SessionAuditObserver
│   ├── test_gui_owning.cpp        # unique_ptr<DummyGuiAdapter> i dispatch wirtualny przez IGui
│   └── test_establish_signals.cpp # Kroki Template Method w SessionCoordinator
└── docs/
    ├── en/
    │   ├── patterns_theory/       # Wykłady po angielsku (Markdown)
    │   │   ├── dummy_gui/         # DummyGui: surowy ptr → weak_ptr → komponent
    │   │   └── service_locator/   # ServiceLocator: podstawowy → std::expected
    │   └── _diagram/              # Diagramy klas i sekwencji (Mermaid)
    └── pl/
        ├── teoria_wzorcow/        # Wykłady po polsku (Markdown)
        │   ├── dunny_gui/
        │   └── serwis locator/
        └── _diagram/              # Diagramy klas i sekwencji (Mermaid)
```

---

## Jak zbudować i uruchomić

**Wymagania:** CMake ≥ 3.20, kompilator C++23 (GCC 13+ / Clang 17+), dostęp do internetu (FetchContent pobiera GTest i yaml-cpp).

```bash
# Konfiguracja
cmake -S . -B build

# Budowanie
cmake --build build -j$(nproc)

# Uruchomienie programu
./build/patterns

# Uruchomienie testów
ctest --test-dir build --output-on-failure
```

---

## Wykłady (`docs/`)

### Ewolucja DummyGui

| Plik (EN) | Plik (PL) | Temat |
|---|---|---|
| `docs/en/patterns_theory/dummy_gui/Lecture_DummyGui.md` | `docs/pl/teoria_wzorcow/dunny_gui/Wyklad_DummyGui.md` | Surowy wskaźnik, wskaźniki do metod składowych |
| `docs/en/patterns_theory/dummy_gui/Lecture_weak_ptr_in_DummyGui.md` | `docs/pl/teoria_wzorcow/dunny_gui/Wyklad_weak_ptr_w_DummyGui.md` | Problem wiszącego wskaźnika, rozwiązanie z `weak_ptr` |
| `docs/en/patterns_theory/dummy_gui/Lecture_DummyGui_as_component.md` | `docs/pl/teoria_wzorcow/dunny_gui/Wyklad_DummyGui_jako_komponent.md` | Biblioteka statyczna, C-style API, `default_delete` |

### Ewolucja ServiceLocatora

| Plik (EN) | Plik (PL) | Temat |
|---|---|---|
| `docs/en/patterns_theory/service_locator/Lecture_Service_Locator_in_CPP.md` | `docs/pl/teoria_wzorcow/serwis locator/Wyklad_Service_Locator_w_CPP.md` | Podstawowy ServiceLocator w C++ |
| `docs/en/patterns_theory/service_locator/Lecture_ServiceLocator_expected.md` | `docs/pl/teoria_wzorcow/serwis locator/Wyklad_ServiceLocator_expected.md` | Migracja z wyjątków na `std::expected` |
| `docs/en/patterns_theory/service_locator/Lecture_ServiceLocator_expected_full_version.md` | `docs/pl/teoria_wzorcow/serwis locator/Wyklad_ServiceLocator_expected_pelna_wersja.md` | Pełna implementacja C++23 z operacjami monadycznymi |

### Teoria wzorców GoF

| Plik (EN) | Plik (PL) | Temat |
|---|---|---|
| `docs/en/patterns_theory/Lecture_Singleton_and_Service_Locator.md` | `docs/pl/teoria_wzorcow/Wyklad_Singleton_i_Service_Locator_teoria.md` | Singleton, Service Locator |
| `docs/en/patterns_theory/Lecture_Strategy_and_Factory.md` | `docs/pl/teoria_wzorcow/Wyklad_Strategy_i_Factory_teoria.md` | Strategy, Factory |
| `docs/en/patterns_theory/Lecture_Facade_and_Template_Method.md` | `docs/pl/teoria_wzorcow/Wyklad_Facade_i_Template_Method_teoria.md` | Facade, Template Method |
| `docs/en/patterns_theory/Lecture_Observer.md` | `docs/pl/teoria_wzorcow/Wyklad_Observer_teoria.md` | Observer |

### Diagramy (`docs/*/\_diagram/`)

| Plik (EN) | Plik (PL) | Zawartość |
|---|---|---|
| `docs/en/_diagram/class_diagram.md` | `docs/pl/_diagram/diagram_klas.md` | Diagram klas całego systemu |
| `docs/en/_diagram/sequence_diagrams.md` | `docs/pl/_diagram/diagramy_sekwencji.md` | Diagramy sekwencji dla głównych scenariuszy |

Diagramy są zapisane w składni **Mermaid** — renderują się bezpośrednio na GitHubie.
