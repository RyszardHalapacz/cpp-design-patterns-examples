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
| **Template Method** | `SessionCoordinator`, `EngineSessionCoordinator` | `include/patterns/session/SessionCoordinator.hpp` |
| **Builder** | `CommandBatchBuilder` | `components/dummy_gui/include/patterns/gui/CommandBatchBuilder.hpp` |
| **Command** | `ICommand`, `AddVectorCommand`, `SortVectorCommand`, `PrintDataCommand` | `components/core/include/patterns/gui/ICommand.hpp` |
| **Adapter** | `DummyGuiAdapter`, `IGui` | `components/dummy_gui/include/patterns/gui/DummyGuiAdapter.hpp` |

---

## What else this project teaches

At a glance, the project also covers:

- **C++23** — `std::expected`, monadic operations, concepts
- **Ownership & lifetime** — `shared_ptr`/`weak_ptr` ownership graphs, Rule of Zero, `weak_ptr` as a disconnect mechanism
- **Multi-component CMake** — separate libraries, `FetchContent`, `enable_testing()` placement
- **GoF patterns working together** — all patterns integrated into one coherent system, not isolated snippets
- **Unit + component testing** — Google Test unit tests and a strict `Engine` component test harness with selective channel activation
- **CI + documentation** — GitHub Actions workflow, Mermaid diagrams, bilingual lecture notes

---

### C++ project structure
- Multi-component layout: `components/core/`, `components/engine/`, and `components/dummy_gui/` as separate static/interface libraries
- Split into `include/` (headers) and `src/` (implementations) within each component
- Nested namespaces (`namespace patterns::services`)
- Forward declarations to break circular dependencies

### CMake
- Multi-component build: `core_lib`, `engine_lib` (INTERFACE), and `dummy_gui_lib` as separate `add_library` targets
- `enable_testing()` placed before all `add_subdirectory` calls so every component's tests are discovered by CTest
- `FetchContent` to download Google Test and yaml-cpp without a system-wide installation
- Version injection via `configure_file` (`.yml.in` → `.yml`)
- Compile-time manifest path macros (`ENGINE_MANIFEST_PATH`, `DUMMY_GUI_MANIFEST_PATH`)
- Tests in separate subdirectories with their own `CMakeLists.txt`

### Google Test
- `TEST` — basic unit tests
- `TEST_F` — fixture-based tests with shared `SetUp`
- `TEST_P` — parameterized tests (one test body, multiple data sets)
- Stdout capture via `testing::internal::CaptureStdout()`
- 126 tests across 8 test files — verify live with `ctest --test-dir build`

### C++23 features
- **`std::expected<T, E>`** — `ServiceLocator` and `SortStrategyFactory` return `expected` instead of throwing; callers handle errors explicitly
- **Monadic operations** — `and_then`, `transform`, `or_else` chained in `SessionCoordinator::establish()` for a clean, linear error-propagation pipeline
- No exceptions in the service or factory layer — all error paths are type-safe and composable

### C++ templates & idioms
- `ServiceLocator` using `std::type_index` as a runtime type key
- **`concept ServiceType`** — `std::derived_from<TService, IService>` constrains `ServiceLocator` template parameters at the declaration site, replacing `static_assert` with cleaner compiler errors
- **Meyers Singleton** — `static` local variable inside `instance()`
- **`std::function` callbacks** — `DummyGui` stores `std::function` slots instead of raw session pointers; `Configurator` wires lambdas that capture `weak_ptr<SessionManagement>`
- **`std::weak_ptr` observer collection** — `SessionManagement` holds `vector<weak_ptr<ISessionObserver>>`; expired observers are pruned automatically and duplicates are rejected
- **`weak_ptr` as disconnect mechanism** — `Application` owns `shared_ptr<IHistorian>`; `EngineSessionCoordinator` and `Engine` each hold a `weak_ptr`; `disableHistorian()` clears Engine's `weak_ptr` so the historian stops recording without destroying it; `enableHistorian()` re-wires the existing instance back
- **Ownership model** — `Application` owns all top-level objects as `shared_ptr` / `unique_ptr`; `SessionManagement` holds `weak_ptr<Engine>`; `SessionAuditObserver` lifetime is controlled by `Application`, not the session
- **Rule of Zero** — `DummyGuiAdapter` has no destructor; `unique_ptr<DummyGui>` calls `deleteGUI()` automatically via a `std::default_delete<DummyGui>` specialization
- **C-style factory API** (`makeGUI` / `deleteGUI`) — simulates legacy C library interfaces (SDL, curl pattern); wrapped behind `IGui` by the Adapter

---

## Component testing — Engine as SUT

Beyond classic unit tests the project includes a component test for `Engine`
([`components/engine/component_test/EngineComponentTest.cpp`](components/engine/component_test/EngineComponentTest.cpp)).

Unit tests verify isolated classes and operations.
The component test treats `Engine` as a complete **System Under Test (SUT)**: only behaviour
observable at the component boundary is asserted — internals are not inspected directly.

### Test architecture

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

`Engine` remains unaware it is under test.
Production dependencies are replaced by test doubles wired through the same public interfaces:

```
IHistorian           ──── HistorianSpy      (channel ON)
                     └─── NullHistorian     (channel OFF)

ISortStrategyFactory ──── FactorySpy        (channel ON)
                     └─── SortStrategyFactory (channel OFF, real)
```

- **`HistorianSpy`** — observation point on the `IHistorian` boundary; reports every
  `recordCommand` and `publishSnapshot` call to `ScenarioVerifier`
- **`FactorySpy`** — observation point on the `ISortStrategyFactory` boundary; reports
  `create()` calls to `ScenarioVerifier` and delegates to the real factory
- **`EngineDriver`** — sends stimuli to the SUT and runs per-step strict verification

### Scenario protocol — Stimulus + Expectation

Tests are expressed as sequences of typed signals:

```cpp
driver.run({
    receiveVectorAdded({1, 2, 3}),                   // Stimulus  → drives Engine
    expectHistorianCommand("addVector", {1, 2, 3}),  // Expectation → must emerge
});
```

`receive*` delivers a stimulus to `Engine`.
`expect*` declares the exact signal that must cross the component boundary in response.

Verification is **strict and ordered within each step**:
- Undeclared signal from an active channel → `"Unexpected signal"` (immediate failure)
- Signals arriving in wrong order → `"Unexpected signal"`
- Expected signal that never arrives → `"Signal not received"`

### Selective channel activation

Fixture topology is declared via multiple inheritance — no configuration code needed:

```cpp
class EngineComponentTest : public EngineTestBase,
                             public HistorianSpy,    // historian channel ON
                             public FactorySpy   {}; // factory channel ON

class HistorianOnlyTest   : public EngineTestBase,
                             public HistorianSpy  {}; // only historian ON

class FactoryOnlyTest     : public EngineTestBase,
                             public FactorySpy    {}; // only factory ON
```

`EngineTestBase::SetUp()` uses `dynamic_cast` to detect which spies the concrete fixture
provides, then builds an `ActiveChannels` object that carries the topology to `EngineDriver`:

```cpp
auto* h = dynamic_cast<HistorianSpy*>(this);
auto* f = dynamic_cast<FactorySpy*>(this);
channels_ = {h != nullptr, f != nullptr};
```

`ActiveChannels` serves two roles:
1. **Engine wiring** — active spy is connected; inactive channel gets a real or null collaborator
2. **Expectation filtering** — `EngineDriver` silently skips `expect*` entries for inactive endpoints (not a failure)

The same pre-built scenario works unchanged across all fixture topologies:

```
Scenarios::SetStrategy(Descending):
  receiveStrategyChange(...)    Stimulus  → always executed
  expectFactoryCreate(...)      Factory ON  → VERIFY  |  Factory OFF → SKIP
  expectHistorianCommand(...)   Historian ON  → VERIFY  |  Historian OFF → SKIP
```

### Empty contract semantics

An empty expectation list for an active channel means **strict zero**: any signal from
that channel produces `"Unexpected signal"`. Empty ≠ "don't care":

```cpp
TEST_F(EngineComponentTest, LocalScenario) {
    driver.run({
        receiveVectorAdded({10, 20, 30}),
      //  expectHistorianCommand(...)     ← commented out
    });
    // Both channels active + zero expectations = zero-tolerance.
    // Engine calls historian.recordCommand() → "Unexpected signal".
}
```

### Lifetime — weak_ptr and keepers

`Engine` stores `historian` and `factory` via `weak_ptr`. Spies are base-class sub-objects
of the fixture (not heap-allocated). `EngineTestBase` holds `historianKeeper_` and
`factoryKeeper_` — `shared_ptr`s with a no-op deleter that keep the control block alive for
the fixture's entire lifetime:

```cpp
historianKeeper_ = std::shared_ptr<IHistorian>(h, [](auto*){});  // no-op deleter
engine_->setHistorian(historianKeeper_);   // Engine's weak_ptr has a live control block
```

`TearDown()` resets `engine_` before base-class destructors destroy spy sub-objects:

```
1. TearDown()      → engine_.reset()   Engine destroyed; keepers and spies still alive
2. ~FactorySpy()
3. ~HistorianSpy()
4. ~EngineTestBase()                   keepers destroyed
```

### Running the component test

```bash
# Build
cmake --build build --parallel

# Run all component tests
./build/components/engine/component_test/engine_component_tests

# Run a specific fixture
./build/components/engine/component_test/engine_component_tests \
    --gtest_filter="HistorianOnlyTest.*"
```

### Sample output (FullEngineFlow, both channels active)

```
[Driver] ---receiveVectorAdded-----------> [Engine]                       # [Engine] Event received: session state changed
                                           [Engine] ---recordCommand-----> [HistorianSpy]
[Driver] ---receiveSortRequested---------> [Engine]                       # [Engine] Event received: session state changed
                                           [Engine] ---recordCommand-----> [HistorianSpy]
[Driver] ---receiveStrategyChange--------> [Engine]                       # [Engine] Event received: session state changed
                                           [Engine] ---create------------> [FactorySpy]
                                           [Engine] ---recordCommand-----> [HistorianSpy]
[Driver] ---receivePublishSnapshot-------> [Engine]
                                           [Engine] ---publishSnapshot---> [HistorianSpy]
```

The current implementation is deliberately synchronous, separating the testing model from
concurrency concerns. The architecture allows `Engine` to move to a dedicated thread in the
future, with direct calls replaced by message queues and timeout-based expectations.

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
│   │   │   ├── historian/         # EngineHistorian, CommandHistory, EngineSnapshot
│   │   │   └── manifest/          # ManifestWriter
│   │   └── src/
│   ├── engine/                    # Interface library — Engine component
│   │   ├── include/patterns/
│   │   │   └── engine/            # BasicEngine<Writer>, Engine (alias)
│   │   ├── engine.yml.in
│   │   ├── tests/                 # test_engine.cpp (lifecycle, session events)
│   │   └── component_test/        # EngineComponentTest.cpp (Engine as SUT)
│   └── dummy_gui/                 # Static library — GUI component
│       ├── include/patterns/
│       │   └── gui/               # DummyGui (C-style API), DummyGuiAdapter, CommandBatchBuilder
│       ├── src/
│       └── tests/                 # test_gui.cpp (CommandBatchBuilder, DummyGuiAdapter)
├── include/patterns/              # App-level headers
│   ├── app/                       # Application (configure + run)
│   └── session/                   # SessionManagement, SessionCoordinator, SessionAuditObserver
├── src/                           # App-level implementations + main.cpp
├── tests/                         # App-level unit tests
│   ├── test_services.cpp          # Logger, FileLogger, ServiceLocator (std::expected API)
│   ├── test_strategy.cpp          # All sort strategies + factory
│   ├── test_session.cpp           # SessionManagement, SessionCoordinator, SessionAuditObserver
│   ├── test_gui_owning.cpp        # unique_ptr<DummyGuiAdapter> and IGui virtual dispatch
│   └── test_establish_signals.cpp # SessionCoordinator Template Method steps
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
