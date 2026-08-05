# C++ Design Patterns

![CI](https://github.com/RyszardHalapacz/cpp-design-patterns-examples/actions/workflows/ci.yml/badge.svg)

A practical demonstration of classic Gang of Four design patterns implemented in C++17.
Built as a **fully working, compilable project** — not a collection of snippets or pseudocode.
Every pattern has its own classes, unit tests, and lecture notes.

> 🔗 **Try it online (no installation needed):** [Run on Wandbox](https://wandbox.org/permlink/xlLubTrVnRwynA3j)
> Polish version: [README_PL.md](README_PL.md)

---

## Design Patterns

| Pattern | Class | File |
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

## What else this project teaches

### C++ project structure
- Split into `include/` (headers) and `src/` (implementations)
- Nested namespaces (`namespace patterns::services`)
- Forward declarations to break circular dependencies

### CMake
- Static library (`wzorce_lib`) linked to both the executable and tests
- Tests in a separate subdirectory with their own `CMakeLists.txt`
- `FetchContent` to download Google Test without a system-wide installation

### Google Test
- `TEST` — basic unit tests
- `TEST_F` — fixture-based tests with shared `SetUp`
- `TEST_P` — parameterized tests (one test body, multiple data sets)
- Stdout capture via `testing::internal::CaptureStdout()`

### C++ templates & idioms
- `ServiceLocator<TService>` using `std::type_index` as a runtime type key
- `static_assert` for compile-time constraint checking
- **Meyers Singleton** — `static` local variable inside `instance()`
- **`delete this`** — `SessionAuditObserver` destroys itself when the session closes
- **Member function pointers** — `DummyGui` stores pointers to `SessionManagement` methods

---

## Project structure

```
wzorce/
├── include/patterns/          # Headers — interfaces and declarations
│   ├── services/              # Logger, FileLogger, DoSomething, ServiceLocator
│   ├── strategy/              # ISortStrategy, 3 implementations, SortStrategyFactory
│   ├── observer/              # ISessionObserver, SessionEvent
│   ├── engine/                # Engine
│   ├── session/               # SessionManagement, SessionEstablisher, SessionAuditObserver
│   ├── gui/                   # Command, CommandBatchBuilder, DummyGui
│   └── config/                # Configurator
├── src/                       # Implementations (.cpp)
├── tests/                     # Unit tests (Google Test)
│   ├── CMakeLists.txt
│   ├── test_services.cpp
│   ├── test_strategy.cpp
│   ├── test_engine.cpp
│   ├── test_session.cpp
│   └── test_gui.cpp
├── teoria_theory/             # Lecture notes in Markdown (PL + EN)
├── diagram_diagrams/          # Class and sequence diagrams in Mermaid (PL + EN)
└── CMakeLists.txt
```

---

## Build & run

**Requirements:** CMake ≥ 3.20, C++17 compiler (GCC / Clang), internet access (FetchContent downloads GTest).

```bash
# Configure
cmake -S . -B build

# Build
cmake --build build -j$(nproc)

# Run the program
./build/wzorce

# Run tests
ctest --test-dir build --output-on-failure
```

---

## Additional materials

### Lectures (`teoria_theory/`)

| File (EN) | File (PL) | Topic |
|---|---|---|
| `Lecture_Singleton_and_Service_Locator.md` | `Wyklad_Singleton_i_Service_Locator_teoria.md` | Singleton, Service Locator |
| `Lecture_Strategy_and_Factory.md` | `Wyklad_Strategy_i_Factory_teoria.md` | Strategy, Factory |
| `Lecture_Facade_and_Template_Method.md` | `Wyklad_Facade_i_Template_Method_teoria.md` | Facade, Template Method |
| `Lecture_Observer.md` | `Wyklad_Observer_teoria.md` | Observer |

### Diagrams (`diagram_diagrams/`)

| File (EN) | File (PL) | Content |
|---|---|---|
| `class_diagram.md` | `diagram_klas.md` | Full class diagram of the system |
| `sequence_diagrams.md` | `diagramy_sekwencji.md` | Sequence diagrams for main scenarios |

Diagrams are written in **Mermaid** syntax and render directly on GitHub.
