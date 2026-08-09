# C++ Design Patterns

![CI](https://github.com/RyszardHalapacz/cpp-design-patterns-examples/actions/workflows/ci.yml/badge.svg)

A practical demonstration of classic Gang of Four design patterns implemented in modern C++ (C++23).
Built as a **fully working, compilable project** — not a collection of snippets or pseudocode.
Every pattern has its own classes, unit tests, and lecture notes.

> Polish version: [README_PL.md](README_PL.md)

---

## Design Patterns

| Pattern | Class | File |
|---|---|---|
| **Singleton** | `ServiceLocator` | `components/core/include/patterns/services/ServiceLocator.hpp` |
| **Service Locator** | `ServiceLocator`, `appLogger()`, `logApp()` | `components/core/include/patterns/services/ServiceLocator.hpp` |
| **Strategy** | `ISortStrategy`, `AscendingSortStrategy`, `DescendingSortStrategy`, `BubbleSortStrategy` | `components/core/include/patterns/strategy/` |
| **Factory** | `SortStrategyFactory` | `components/core/include/patterns/strategy/SortStrategyFactory.hpp` |
| **Observer** | `ISessionObserver`, `Engine`, `SessionAuditObserver` | `components/core/include/patterns/observer/`, `include/patterns/session/` |
| **Facade** | `SessionManagement` | `include/patterns/session/SessionManagement.hpp` |
| **Template Method** | `SessionEstablisher`, `EngineSessionEstablisher` | `include/patterns/session/SessionEstablisher.hpp` |
| **Builder** | `CommandBatchBuilder` | `components/dummy_gui/include/patterns/gui/CommandBatchBuilder.hpp` |
| **Command** | `ICommand`, `AddVectorCommand`, `SortVectorCommand`, `PrintDataCommand` | `components/core/include/patterns/gui/ICommand.hpp` |
| **Adapter** | `DummyGuiAdapter`, `IGui` | `components/dummy_gui/include/patterns/gui/DummyGuiAdapter.hpp` |

---

## What else this project teaches

### C++ project structure
- Multi-component layout: `components/core/` and `components/dummy_gui/` as separate static libraries
- Split into `include/` (headers) and `src/` (implementations) within each component
- Nested namespaces (`namespace patterns::services`)
- Forward declarations to break circular dependencies

### CMake
- Multi-component build: `core_lib` and `dummy_gui_lib` as separate `add_library` targets
- `FetchContent` to download Google Test and yaml-cpp without a system-wide installation
- Version injection via `configure_file` (`.yml.in` → `.yml`)
- Tests in separate subdirectories with their own `CMakeLists.txt`

### Google Test
- `TEST` — basic unit tests
- `TEST_F` — fixture-based tests with shared `SetUp`
- `TEST_P` — parameterized tests (one test body, multiple data sets)
- Stdout capture via `testing::internal::CaptureStdout()`
- 77 tests across 6 test files

### C++23 features
- **`std::expected<T, E>`** — `ServiceLocator` and `SortStrategyFactory` return `expected` instead of throwing; callers handle errors explicitly
- **Monadic operations** — `and_then`, `transform`, `or_else` chained in `SessionEstablisher::establish()` for a clean, linear error-propagation pipeline
- No exceptions in the service or factory layer — all error paths are type-safe and composable

### C++ templates & idioms
- `ServiceLocator` using `std::type_index` as a runtime type key
- `static_assert` for compile-time constraint checking
- **Meyers Singleton** — `static` local variable inside `instance()`
- **`std::function` callbacks** — `DummyGui` stores `std::function` slots instead of raw session pointers; `Configurator` wires lambdas that capture `weak_ptr<SessionManagement>`
- **`std::weak_ptr` observer collection** — `SessionManagement` holds `vector<weak_ptr<ISessionObserver>>`; expired observers are pruned automatically and duplicates are rejected
- **Ownership model** — `Application` owns all top-level objects as `shared_ptr` / `unique_ptr`; `SessionManagement` holds `weak_ptr<Engine>`; `SessionAuditObserver` lifetime is controlled by `Application`, not the session
- **Rule of Zero** — `DummyGuiAdapter` has no destructor; `unique_ptr<DummyGui>` calls `deleteGUI()` automatically via a `std::default_delete<DummyGui>` specialization
- **C-style factory API** (`makeGUI` / `deleteGUI`) — simulates legacy C library interfaces (SDL, curl pattern); wrapped behind `IGui` by the Adapter

---

## Project structure

```
wzorce/
├── components/
│   ├── core/                      # Static library — shared foundation
│   │   ├── include/patterns/
│   │   │   ├── services/          # Logger, FileLogger, DoSomething, ServiceLocator (C++23)
│   │   │   ├── strategy/          # ISortStrategy, 3 implementations, SortStrategyFactory
│   │   │   ├── observer/          # ISessionObserver, SessionEvent
│   │   │   ├── gui/               # IGui, ICommand + concrete commands
│   │   │   └── manifest/          # ManifestWriter
│   │   └── src/
│   └── dummy_gui/                 # Static library — GUI component
│       ├── include/patterns/
│       │   ├── gui/               # DummyGui (C-style API), DummyGuiAdapter, CommandBatchBuilder
│       │   └── config/            # Configurator
│       ├── src/
│       └── tests/                 # test_gui.cpp (CommandBatchBuilder, DummyGuiAdapter, Configurator)
├── include/patterns/              # App-level headers
│   ├── app/                       # Application (configure + run)
│   ├── engine/                    # Engine (BasicEngine<Writer>)
│   └── session/                   # SessionManagement, SessionEstablisher, SessionAuditObserver
├── src/                           # App-level implementations + main.cpp
├── tests/                         # App-level unit tests
│   ├── test_services.cpp          # Logger, FileLogger, ServiceLocator (std::expected API)
│   ├── test_strategy.cpp          # All sort strategies + factory
│   ├── test_engine.cpp            # Engine lifecycle and session events
│   ├── test_session.cpp           # SessionManagement, SessionEstablisher, SessionAuditObserver
│   └── test_gui_owning.cpp        # unique_ptr<DummyGuiAdapter> and IGui virtual dispatch
└── docs/
    ├── en/
    │   ├── patterns_theory/       # Lecture notes in English (Markdown)
    │   │   ├── dummy_gui/         # DummyGui: raw ptr → weak_ptr → component
    │   │   └── service_locator/   # ServiceLocator: basic → std::expected
    │   └── _diagram/              # Class and sequence diagrams (Mermaid)
    └── pl/
        ├── teoria_wzorcow/        # Wykłady po polsku (Markdown)
        │   ├── dunny_gui/
        │   └── serwis locator/
        └── _diagram/              # Diagramy klas i sekwencji (Mermaid)
```

---

## Build & run

**Requirements:** CMake ≥ 3.20, C++23 compiler (GCC 13+ / Clang 17+), internet access (FetchContent downloads GTest and yaml-cpp).

```bash
# Configure
cmake -S . -B build

# Build
cmake --build build -j$(nproc)

# Run the program
./build/patterns

# Run tests
ctest --test-dir build --output-on-failure
```

---

## Lecture notes (`docs/`)

### DummyGui evolution

| File (EN) | File (PL) | Topic |
|---|---|---|
| `docs/en/patterns_theory/dummy_gui/Lecture_DummyGui.md` | `docs/pl/teoria_wzorcow/dunny_gui/Wyklad_DummyGui.md` | Raw pointer baseline, member function pointers |
| `docs/en/patterns_theory/dummy_gui/Lecture_weak_ptr_in_DummyGui.md` | `docs/pl/teoria_wzorcow/dunny_gui/Wyklad_weak_ptr_w_DummyGui.md` | Dangling pointer problem, `weak_ptr` solution |
| `docs/en/patterns_theory/dummy_gui/Lecture_DummyGui_as_component.md` | `docs/pl/teoria_wzorcow/dunny_gui/Wyklad_DummyGui_jako_komponent.md` | Static library, C-style API, `default_delete` |

### ServiceLocator evolution

| File (EN) | File (PL) | Topic |
|---|---|---|
| `docs/en/patterns_theory/service_locator/Lecture_Service_Locator_in_CPP.md` | `docs/pl/teoria_wzorcow/serwis locator/Wyklad_Service_Locator_w_CPP.md` | Basic ServiceLocator in C++ |
| `docs/en/patterns_theory/service_locator/Lecture_ServiceLocator_expected.md` | `docs/pl/teoria_wzorcow/serwis locator/Wyklad_ServiceLocator_expected.md` | Migration from exceptions to `std::expected` |
| `docs/en/patterns_theory/service_locator/Lecture_ServiceLocator_expected_full_version.md` | `docs/pl/teoria_wzorcow/serwis locator/Wyklad_ServiceLocator_expected_pelna_wersja.md` | Full C++23 implementation with monadic ops |

### GoF patterns theory

| File (EN) | File (PL) | Topic |
|---|---|---|
| `docs/en/patterns_theory/Lecture_Singleton_and_Service_Locator.md` | `docs/pl/teoria_wzorcow/Wyklad_Singleton_i_Service_Locator_teoria.md` | Singleton, Service Locator |
| `docs/en/patterns_theory/Lecture_Strategy_and_Factory.md` | `docs/pl/teoria_wzorcow/Wyklad_Strategy_i_Factory_teoria.md` | Strategy, Factory |
| `docs/en/patterns_theory/Lecture_Facade_and_Template_Method.md` | `docs/pl/teoria_wzorcow/Wyklad_Facade_i_Template_Method_teoria.md` | Facade, Template Method |
| `docs/en/patterns_theory/Lecture_Observer.md` | `docs/pl/teoria_wzorcow/Wyklad_Observer_teoria.md` | Observer |

### Diagrams (`docs/*/\_diagram/`)

| File (EN) | File (PL) | Content |
|---|---|---|
| `docs/en/_diagram/class_diagram.md` | `docs/pl/_diagram/diagram_klas.md` | Full class diagram of the system |
| `docs/en/_diagram/sequence_diagrams.md` | `docs/pl/_diagram/diagramy_sekwencji.md` | Sequence diagrams for main scenarios |

Diagrams are written in **Mermaid** syntax and render directly on GitHub.
