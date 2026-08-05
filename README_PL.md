# Wzorce Projektowe w C++17

![CI](https://github.com/RyszardHalapacz/cpp-design-patterns-examples/actions/workflows/ci.yml/badge.svg)

Praktyczna demonstracja klasycznych wzorców projektowych (GoF) zaimplementowanych w C++17.
Projekt jest zbudowany jako **działający, kompilujący się kod** — nie zbiór snippetów ani pseudokodu.
Każdy wzorzec ma własne klasy, testy jednostkowe i materiały teoretyczne.

> 🔗 **Wypróbuj online (bez instalacji):** [Uruchom na Wandbox](https://wandbox.org/permlink/xlLubTrVnRwynA3j)

---

## Wzorce projektowe

| Wzorzec | Klasa | Plik |
|---|---|---|
| **Singleton** | `ServiceLocator` | `include/patterns/services/ServiceLocator.hpp` |
| **Service Locator** | `ServiceLocator`, `appLogger()` | `include/patterns/services/ServiceLocator.hpp` |
| **Strategy** | `ISortStrategy`, `AscendingSortStrategy`, `DescendingSortStrategy`, `BubbleSortStrategy` | `include/patterns/strategy/` |
| **Factory** | `SortStrategyFactory` | `include/patterns/strategy/SortStrategyFactory.hpp` |
| **Observer** | `ISessionObserver`, `Engine`, `SessionAuditObserver` | `include/patterns/observer/`, `include/patterns/session/` |
| **Facade** | `SessionManagement` | `include/patterns/session/SessionManagement.hpp` |
| **Template Method** | `SessionEstablisher`, `EngineSessionEstablisher` | `include/patterns/session/SessionEstablisher.hpp` |
| **Builder** | `CommandBatchBuilder`, `DummyGui` | `include/patterns/gui/` |

---

## Czego uczy ten projekt (poza wzorcami)

### Organizacja projektu C++
- Podział na `include/` (nagłówki) i `src/` (implementacje)
- Zagnieżdżone przestrzenie nazw (`namespace patterns::services`)
- Forward declarations do rozrywania cykli zależności między klasami

### CMake
- Biblioteka statyczna (`wzorce_lib`) linkowana do executable i testów
- Testy w osobnym podkatalogu z własnym `CMakeLists.txt`
- `FetchContent` do pobierania Google Test bez instalacji systemowej

### Google Test
- `TEST` — podstawowe testy jednostkowe
- `TEST_F` — testy z fixture (współdzielony `SetUp`)
- `TEST_P` — testy sparametryzowane (jeden kod testu, wiele zestawów danych)
- Przechwytywanie `stdout` przez `testing::internal::CaptureStdout()`

### Szablony C++
- `ServiceLocator<TService>` z `std::type_index` jako kluczem rejestru
- `static_assert` do weryfikacji constraintów w czasie kompilacji
- `std::dynamic_pointer_cast` i RTTI

### Idiomy C++
- **Meyers Singleton** — `static` lokalna zmienna w metodzie `instance()`
- **`delete this`** — `SessionAuditObserver` niszczy się sam po zakończeniu sesji
- **Member function pointers** — `DummyGui` przechowuje wskaźniki do metod `SessionManagement`

---

## Struktura projektu

```
wzorce/
├── include/patterns/          # Nagłówki — interfejsy i deklaracje
│   ├── services/              # Logger, FileLogger, DoSomething, ServiceLocator
│   ├── strategy/              # ISortStrategy, 3 implementacje, SortStrategyFactory
│   ├── observer/              # ISessionObserver, SessionEvent
│   ├── engine/                # Engine
│   ├── session/               # SessionManagement, SessionEstablisher, SessionAuditObserver
│   ├── gui/                   # Command, CommandBatchBuilder, DummyGui
│   └── config/                # Configurator
├── src/                       # Implementacje (.cpp)
├── tests/                     # Testy jednostkowe (Google Test)
│   ├── CMakeLists.txt
│   ├── test_services.cpp
│   ├── test_strategy.cpp
│   ├── test_engine.cpp
│   ├── test_session.cpp
│   └── test_gui.cpp
├── teoria_theory/             # Wykłady w Markdown (PL + EN)
├── diagram_diagrams/          # Diagramy klas i sekwencji w Mermaid (PL + EN)
└── CMakeLists.txt
```

---

## Jak zbudować i uruchomić

**Wymagania:** CMake ≥ 3.20, kompilator C++17 (GCC / Clang), dostęp do internetu (FetchContent pobiera GTest).

```bash
# Konfiguracja
cmake -S . -B build

# Budowanie
cmake --build build -j$(nproc)

# Uruchomienie programu
./build/wzorce

# Uruchomienie testów
ctest --test-dir build --output-on-failure
```

---

## Materiały dodatkowe

### Wykłady (`teoria_theory/`)

| Plik (EN) | Plik (PL) | Temat |
|---|---|---|
| `Lecture_Singleton_and_Service_Locator.md` | `Wyklad_Singleton_i_Service_Locator_teoria.md` | Singleton, Service Locator |
| `Lecture_Strategy_and_Factory.md` | `Wyklad_Strategy_i_Factory_teoria.md` | Strategy, Factory |
| `Lecture_Facade_and_Template_Method.md` | `Wyklad_Facade_i_Template_Method_teoria.md` | Facade, Template Method |
| `Lecture_Observer.md` | `Wyklad_Observer_teoria.md` | Observer |

### Diagramy (`diagram_diagrams/`)

| Plik (EN) | Plik (PL) | Zawartość |
|---|---|---|
| `class_diagram.md` | `diagram_klas.md` | Diagram klas całego systemu |
| `sequence_diagrams.md` | `diagramy_sekwencji.md` | Diagramy sekwencji dla głównych scenariuszy |

Diagramy są zapisane w składni **Mermaid** — renderują się bezpośrednio na GitHubie.
