# Changelog

All notable changes to this project are documented here, in reverse chronological order.

---

## 2026-08-09

### `docs: add CHANGELOG.md and CHANGELOG_PL.md with full commit history` — `f8c7eaa`

- Created `CHANGELOG.md` (English) and `CHANGELOG_PL.md` (Polish)
- Every commit documented with date, SHA, title, and description of what changed and why

---

### `feat: add Adapter pattern (DummyGuiAdapter / IGui)` — `c6c96ff`

**GoF Adapter** introduced between the application layer and the C-style GUI library.

- Added `IGui` (target interface) in `components/core/include/patterns/gui/IGui.hpp` — the application knows only this interface
- Added `DummyGuiAdapter` (adapter) in `components/dummy_gui/` — implements `IGui`, owns a `DummyGui*` obtained via `makeGUI()` / `deleteGUI()`
- `DummyGuiAdapter` applies **Rule of Zero**: no destructor written; `unique_ptr<DummyGui>` with the existing `default_delete<DummyGui>` specialization handles cleanup automatically
- `Configurator::configureGui()` now takes `DummyGuiAdapter&` instead of `DummyGui&`
- `Application` stores `unique_ptr<IGui>` — the concrete adapter type is hidden from the application
- Tests updated to use `DummyGuiAdapter` directly; added `AdapterImplementsIGui` and `AdapterUsableAsIGui` tests

---

### `refactor: decouple DummyGui from SessionManagement via std::function callbacks` — `85d17f3`

**Dependency Inversion** applied to `DummyGui` — the GUI widget no longer knows about `SessionManagement`.

- Removed `#include "SessionManagement.hpp"` from `DummyGui.hpp`
- Replaced internal session pointer with four `std::function` slots: `AddVectorFunc`, `SortVectorFunc`, `PrintDataFunc`, `SetSortStrategyFunc`
- `DummyGui` exposes `connect*()` methods; `Configurator` wires lambdas that capture `weak_ptr<SessionManagement>` and check for expiry before forwarding each call
- `flushBatch()` now iterates the `CommandBatch` and calls `ICommand::execute()` on each command — the old `executeBatch()` / switch statement in `SessionManagement` was removed entirely
- `queue*()` methods pass the already-connected callback directly to `CommandBatchBuilder`
- Tests updated: expired-session scenario verified via `ClickWithExpiredSessionLogsError`

---

### `refactor: migrate SessionManagement observers to weak_ptr collection` — `a3830e0`

**Observer** hardened: the observer list is now a non-owning `weak_ptr` collection.

- `observers_` changed from `vector<ISessionObserver*>` to `vector<weak_ptr<ISessionObserver>>`
- `attach()` / `detach()` / `notify()` all accept `shared_ptr<ISessionObserver>`
- `attach()` prunes expired entries before inserting and rejects duplicates via `wp.lock() == observer`
- `notify()` prunes expired entries before iterating — no dangling pointer risk
- `connectToEngine()` now calls `attach(engine)` directly (Engine is a `shared_ptr`)
- New test: `ConnectToEngineTwiceDoesNotDuplicateObserver` — verifies duplicate guard
- `AuditObserverOwnedExternallyOnSessionClosing` — verifies lifetime independence from session

---

### `refactor: SessionEstablisher::establish() uses C++23 monadic std::expected chain` — `905330e`

**Template Method** + **C++23 monadic `std::expected`** combined in session setup.

- `establish()` now returns `std::expected<void, std::string>`
- The body is a single monadic pipeline:
  ```cpp
  return checkPreconditions()
      .and_then([this](...){ connect();       return {}; })
      .and_then([this](...){ configure();     return {}; })
      .and_then([this](...){ finalizeSetup(); return {}; })
      .transform([](){ logApp("Session established\n"); })
      .or_else([](const std::string& e){ logApp(e + " — aborting\n"); return unexpected(e); });
  ```
- If any step returns an error, subsequent steps are skipped automatically — no `if` chains
- `checkPreconditions()` is a virtual hook that derived classes can override
- Tests updated to call `result.has_value()`

---

### `refactor: replace throw in SortStrategyFactory with std::expected (C++23)` — `2466971`

**Factory** migrated from exceptions to `std::expected`.

- `SortStrategyFactory::create()` return type changed from `unique_ptr<ISortStrategy>` (throwing) to `expected<unique_ptr<ISortStrategy>, std::string>`
- Unknown `SortStrategyId` returns `std::unexpected("SortStrategyFactory: unknown SortStrategyId")` instead of throwing
- `Engine` constructor calls `.value()` (panics on unknown id at startup — acceptable for an enum with no runtime input)
- `Engine::onSessionEvent()` uses `if (auto s = SortStrategyFactory::create(...))` to silently ignore unknown strategy change events
- No try/catch anywhere in the codebase

---

### `refactor: migrate Engine ownership to shared_ptr; SessionManagement uses weak_ptr` — `79e67b9`

Ownership model clarified for the engine.

- `Engine` is now created as `shared_ptr<Engine>` in `Application`
- `SessionManagement::connectToEngine()` takes `shared_ptr<Engine>` and stores it as `weak_ptr<Engine>`
- Every session operation checks `engine_.expired()` / `engine_.lock()` before proceeding — no dangling reference possible
- All tests updated: `Engine engine;` → `auto engine = std::make_shared<Engine>()`

---

### `refactor: move SessionAuditObserver ownership to Application; add logFile audit` — `bd5a704`

**Observer** lifetime decoupled from the session.

- `SessionAuditObserver` is now a `shared_ptr` member of `Application`; the session holds only a `weak_ptr`
- Removed `delete this` from `SessionAuditObserver::onSessionEvent(SessionClosing)` — self-deletion was fragile and hid lifetime bugs
- Added `logFile()` calls in `SessionAuditObserver` for every event type (VectorAdded, SortRequested, PrintRequested, StrategyChangeRequested, SessionClosing) — audit trail now goes to the log file as well as stdout
- New test: `AuditObserverOwnedExternallyOnSessionClosing`

---

### `refactor: introduce Application class wrapping main lifecycle` — `3a71de9`

**Facade** over the startup sequence.

- Added `Application` class in `include/patterns/app/Application.hpp` / `src/app/Application.cpp`
- Two public methods: `configure()` (wires all objects) and `run()` (drives the GUI interaction loop)
- `Application` owns: `shared_ptr<Engine>`, `shared_ptr<SessionManagement>`, `shared_ptr<SessionAuditObserver>`, `unique_ptr<DummyGui>`, `Configurator`
- `main.cpp` reduced to three lines: construct `Application`, call `configure()`, call `run()`

---

## 2026-08-05

### `ci: require GCC 13 on ubuntu-24.04 for C++23 std::expected support` — `a381a4a`

- CI workflow pinned to `gcc-13` / `g++-13` on `ubuntu-24.04`
- Ensures `<expected>` header is available without extra flags

### `ci: add workflow_dispatch trigger` — `c3aba80`

- Added `workflow_dispatch` to `.github/workflows/ci.yml` — allows manual CI runs from the GitHub UI

---

### `refactor: migrate ServiceLocator to std::expected (C++23); expand test coverage` — `0533a0f`

**Service Locator** + **C++23 `std::expected`** introduced together.

- `ServiceLocator::get<T>()` replaced by `tryGet<T>()` returning `expected<shared_ptr<T>, ServiceError>`
- Added `ServiceError` enum: `ServiceNotFound`, `TypeMismatch`
- Added `logFile()` shortcut using `.transform()` monadic operation on `expected`
- Added `provideRuntime()` / `tryGetRuntime()` for type-erased `any` service storage
- Test count expanded: 18 new test cases covering all error paths and shortcuts

---

### `refactor: extract core and dummy_gui as CMake components; add C-style API, unique_ptr support, docs` — `da12e0e`

Build system restructured into proper multi-library layout.

- `components/core/` static library: all strategy, observer, service, engine code
- `components/dummy_gui/` static library: `DummyGui`, `CommandBatchBuilder`, `Configurator`
- Added C-style factory API: `makeGUI()` / `deleteGUI()` in `DummyGui.hpp`
- Added `std::default_delete<DummyGui>` specialization — `unique_ptr<DummyGui>` calls `deleteGUI()` automatically
- Added `BasicDummyGui<Writer>` template + `using DummyGui = BasicDummyGui<>` alias
- Added `CommandBatchBuilder` (Builder pattern) replacing ad-hoc batch construction
- Added GoF Command: `ICommand` interface with `AddVectorCommand`, `SortVectorCommand`, `PrintDataCommand`
- Added initial lecture notes under `docs/`

---

### `refactor: replace raw SessionManagement ptr in DummyGui with weak_ptr; bump versions` — `6b80987`

- `DummyGui` changed from `SessionManagement*` (raw) to `weak_ptr<SessionManagement>`
- Every GUI action locks the weak pointer before use; logs an error if the session has expired
- Fixes a use-after-free if `SessionManagement` is destroyed while the GUI is still alive

---

### `refactor: rename project wzorce→patterns, add manifest CRTP writer, version manifests via CMake` — `501c744`

- Project namespace and binary renamed from `wzorce` to `patterns`
- Added `ComponentManifestWriter` (CRTP) — writes `.yml` manifests with component name, version, build timestamp
- Version strings injected by CMake `configure_file` from `.yml.in` templates
- `BasicEngine<Writer>` template introduced for testability (swappable manifest writer)

---

### `Add C++ design patterns project with tests and CI` — `e436289`

Initial commit.

- Singleton, Service Locator, Strategy, Factory, Observer, Facade, Template Method, Builder patterns
- Google Test integration via `FetchContent`
- GitHub Actions CI (Ubuntu + macOS)
- `SessionManagement`, `Engine`, `DummyGui`, `Configurator`, `SessionEstablisher`, `SessionAuditObserver`
