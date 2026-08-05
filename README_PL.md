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
| **Template Method** | `SessionEstablisher`, `EngineSessionEstablisher` | `include/patterns/session/SessionEstablisher.hpp` |
| **Builder** | `CommandBatchBuilder`, `DummyGui` | `components/dummy_gui/include/patterns/gui/` |

---

## Czego uczy ten projekt (poza wzorcami)

### Organizacja projektu C++
- Wielokomponentowa struktura: `components/core/` i `components/dummy_gui/` jako osobne biblioteki statyczne
- Podział na `include/` (nagłówki) i `src/` (implementacje) wewnątrz każdego komponentu
- Zagnieżdżone przestrzenie nazw (`namespace patterns::services`)
- Forward declarations do rozrywania cykli zależności między klasami

### CMake
- Budowanie wielokomponentowe: `core_lib` i `dummy_gui_lib` jako osobne cele `add_library`
- `FetchContent` do pobierania Google Test i yaml-cpp bez instalacji systemowej
- Wstrzykiwanie wersji przez `configure_file` (`.yml.in` → `.yml`)
- Testy w osobnych podkatalogach z własnym `CMakeLists.txt`

### Google Test
- `TEST` — podstawowe testy jednostkowe
- `TEST_F` — testy z fixture (współdzielony `SetUp`)
- `TEST_P` — testy sparametryzowane (jeden kod testu, wiele zestawów danych)
- Przechwytywanie `stdout` przez `testing::internal::CaptureStdout()`
- 75 testów w 6 plikach testowych

### Cechy C++23
- **`std::expected<T, E>`** — metody `ServiceLocator` zwracają `expected` zamiast rzucać wyjątkami; wywołujący obsługuje błędy jawnie
- **Operacje monadyczne** — `.transform()` na `expected` do zwięzłej propagacji błędów (np. `logFile`)
- Brak wyjątków w warstwie serwisów — wszystkie ścieżki błędów są typowo bezpieczne i kompozytowalne

### Szablony i idiomy C++
- `ServiceLocator` z `std::type_index` jako kluczem rejestru w czasie wykonania
- `static_assert` do weryfikacji constraintów w czasie kompilacji
- **Meyers Singleton** — `static` lokalna zmienna w metodzie `instance()`
- **`delete this`** — `SessionAuditObserver` niszczy się sam po zakończeniu sesji
- **Wskaźniki do metod składowych** — `DummyGui` przechowuje wskaźniki do metod `SessionManagement`
- **`std::weak_ptr`** — `DummyGui` trzyma nieposiadającą referencję do `SessionManagement`; sprawdza wygaśnięcie przed każdą operacją
- **Specjalizacja `std::default_delete`** — pozwala `std::unique_ptr<DummyGui>` automatycznie wywołać `deleteGUI()`, bez własnego deletera w każdym miejscu użycia
- **C-style API fabryki** (`makeGUI` / `deleteGUI`) — symulacja interfejsów bibliotek C (wzorzec SDL, curl)

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
│   │   │   ├── gui/               # Command (typ współdzielony)
│   │   │   └── manifest/          # ManifestWriter
│   │   └── src/
│   └── dummy_gui/                 # Biblioteka statyczna — komponent GUI
│       ├── include/patterns/
│       │   ├── gui/               # CommandBatchBuilder, DummyGui + C-style API
│       │   └── config/            # Configurator
│       ├── src/
│       └── tests/                 # test_gui.cpp (CommandBatchBuilder, DummyGui, Configurator)
├── include/patterns/              # Nagłówki na poziomie aplikacji
│   ├── engine/                    # Engine (BasicEngine<Writer>)
│   └── session/                   # SessionManagement, SessionEstablisher, SessionAuditObserver
├── src/                           # Implementacje aplikacji + main.cpp
├── tests/                         # Testy jednostkowe aplikacji
│   ├── test_services.cpp          # Logger, FileLogger, ServiceLocator (API std::expected)
│   ├── test_strategy.cpp          # Wszystkie strategie sortowania + fabryka
│   ├── test_engine.cpp            # Cykl życia Engine i zdarzenia sesji
│   ├── test_session.cpp           # SessionManagement, SessionEstablisher, SessionAuditObserver
│   └── test_gui_owning.cpp        # unique_ptr<DummyGui> przez specjalizację default_delete
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
