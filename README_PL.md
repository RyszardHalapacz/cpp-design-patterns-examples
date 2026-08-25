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

Testy jednostkowe weryfikują pojedyncze klasy i operacje w izolacji.
Test komponentowy traktuje `Engine` jako kompletny **System Under Test (SUT)**: asercje dotyczą
wyłącznie zachowania obserwowalnego na granicy komponentu — wewnętrzna implementacja nie jest
sprawdzana bezpośrednio.

### Architektura testu

Harness buduje minimalne środowisko potrzebne do uruchomienia `Engine`:

```
                EngineComponentTest
                       │
          ┌────────────┴────────────┐
          │                         │
    HistorianSpy              FactorySpy
          ▲                         ▲
          │                         │
          └──────── Engine ─────────┘
```

`Engine` pozostaje nieświadomy tego, że jest testowany.
Produkcyjne zależności są zastąpione dublerami podłączonymi przez te same publiczne interfejsy:

```
IHistorian ──── EngineHistorian   (produkcja)
           └─── HistorianSpy      (test komponentowy)
```

- **`HistorianSpy`** — punkt obserwacyjny na granicy `IHistorian`; rejestruje każdą nazwę komendy
  i każdy snapshot publikowany przez `Engine`
- **`FactorySpy`** — punkt obserwacyjny na granicy `ISortStrategyFactory`; rejestruje, o jakie
  strategie `Engine` prosi, i deleguje do prawdziwej fabryki
- **`EngineDriver`** — dostarcza bodźce do SUT i weryfikuje obserwowalne reakcje na granicy

### Testowanie przepływu

Testy są opisane jako pary sygnałów receive/send:

```
receiveAddVector   →  sendAddVector
receiveSortVector  →  sendSortVector
receiveSetStrategy →  sendSetStrategy
receiveSnapshot    →  sendSnapshot
```

`receive*` dostarcza bodziec do `Engine`.
`send*` weryfikuje oczekiwane zachowanie na granicy komponentu.
Dzięki temu test sprawdza nie tylko pojedyncze wartości zwracane, ale również kolejność
i przepływ całej komunikacji komponentu.

### Uzasadnienie projektu

Fixture używa wielokrotnego dziedziczenia do deklarowania aktywnych kanałów komunikacyjnych:

```cpp
class EngineComponentTest
    : public ::testing::Test,
      public HistorianSpy,  // włącza kanał Engine ↔ Historian
      public FactorySpy {}; // włącza kanał Engine ↔ Factory
```

`EngineDriver` używa `dynamic_cast`, żeby wykryć, które capabilities eksponuje fixture.
Dziedziczenie po spyu włącza cały kanał; usunięcie klasy bazowej wyłącza go —
co jest bardziej czytelne niż przełączanie pojedynczych sygnałów przy większej ich liczbie.

Obecna implementacja jest celowo synchroniczna, co oddziela model testowania od problemów
z synchronizacją. Architektura pozwala w przyszłości przenieść `Engine` do osobnego wątku,
zastępując bezpośrednie wywołania kolejkami komunikatów i asercjami z timeoutem.

### Uruchomienie testu komponentowego

```bash
# Budowanie
cmake --build build --parallel

# Uruchomienie wszystkich testów komponentowych
./build/components/engine/component_test/engine_component_tests

# Uruchomienie jednego przypadku testowego
./build/components/engine/component_test/engine_component_tests --gtest_filter="EngineComponentTest.RunExecutesAllSignals"
```

### Włączanie i wyłączanie kanałów

Usunięcie klasy bazowej z fixture wyłącza cały kanał — wszystkie lambdy `send*` dla tego
spy zwracają `false` i wiersze znikają z diagramu bez modyfikowania `EngineDriver`:

```cpp
class EngineComponentTest
    : public ::testing::Test,
      public HistorianSpy,   // usuń → wyłącza kanał Engine ↔ Historian
      public FactorySpy {};  // usuń → wyłącza kanał Engine ↔ Factory
```

### Przykładowy przebieg (oba kanały aktywne)

```
[Driver] ---receiveAddVector------> [Engine]                                                # [Engine] Event received: session state changed
                                                                                            # [Engine] -> recognized: vector added
                                    [Engine] ---sendAddVector---------> [HistorianSpy]
[Driver] ---receiveSortVector-----> [Engine]                                                # [Engine] Event received: session state changed
                                                                                            # [Engine] -> recognized: sort requested
                                                                                            # [Engine] Sorting with strategy: Ascending
                                    [Engine] ---sendSortVector---------> [HistorianSpy]
[Driver] ---receiveSetStrategy----> [Engine]                                                # [Engine] Event received: session state changed
                                                                                            # [Engine] -> recognized: strategy change requested
                                                                                            # [Engine] Sort strategy set: Descending
                                    [Engine] ---sendSetStrategy-------> [FactorySpy]
[Driver] ---receiveSnapshot-------> [Engine]
                                    [Engine] ---sendSnapshot-----------> [HistorianSpy]
```

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
