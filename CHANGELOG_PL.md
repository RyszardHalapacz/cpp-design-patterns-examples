# Historia zmian

Wszystkie istotne zmiany w projekcie są tutaj opisane, od najnowszych do najstarszych.

---

## 2026-08-09

### `feat: add Adapter pattern (DummyGuiAdapter / IGui)` — `c6c96ff`

Wprowadzenie wzorca **Adapter** (GoF) między warstwą aplikacji a biblioteką GUI z C-style API.

- Dodano `IGui` (interfejs docelowy) w `components/core/include/patterns/gui/IGui.hpp` — aplikacja zna tylko ten interfejs
- Dodano `DummyGuiAdapter` (adapter) w `components/dummy_gui/` — implementuje `IGui`, posiada wewnętrznie `DummyGui*` tworzony przez `makeGUI()` i niszczony przez `deleteGUI()`
- `DummyGuiAdapter` stosuje **Rule of Zero**: brak ręcznego destruktora; `unique_ptr<DummyGui>` ze specjalizacją `default_delete<DummyGui>` sam wywołuje `deleteGUI()` przy zniszczeniu
- `Configurator::configureGui()` przyjmuje teraz `DummyGuiAdapter&` zamiast `DummyGui&`
- `Application` przechowuje `unique_ptr<IGui>` — konkretny typ adaptera jest ukryty przed aplikacją
- Testy zaktualizowane do używania `DummyGuiAdapter`; dodano testy `AdapterImplementsIGui` i `AdapterUsableAsIGui`

---

### `refactor: decouple DummyGui from SessionManagement via std::function callbacks` — `85d17f3`

Zastosowanie **Dependency Inversion** — widget GUI nie zna już `SessionManagement`.

- Usunięto `#include "SessionManagement.hpp"` z `DummyGui.hpp`
- Surowy wskaźnik do sesji zastąpiono czterema slotami `std::function`: `AddVectorFunc`, `SortVectorFunc`, `PrintDataFunc`, `SetSortStrategyFunc`
- `DummyGui` udostępnia metody `connect*()`; `Configurator` podpina lambdy przechwytujące `weak_ptr<SessionManagement>`, sprawdzające wygaśnięcie przed każdym wywołaniem
- `flushBatch()` iteruje `CommandBatch` i wywołuje `ICommand::execute()` na każdej komendzie — stary `executeBatch()` z instrukcją `switch` w `SessionManagement` został całkowicie usunięty
- Metody `queue*()` przekazują już podpięty callback bezpośrednio do `CommandBatchBuilder`
- Testy: scenariusz wygasłej sesji weryfikowany przez `ClickWithExpiredSessionLogsError`

---

### `refactor: migrate SessionManagement observers to weak_ptr collection` — `a3830e0`

Wzmocnienie wzorca **Observer** — lista obserwatorów jest teraz kolekcją `weak_ptr`.

- `observers_` zmienione z `vector<ISessionObserver*>` na `vector<weak_ptr<ISessionObserver>>`
- `attach()` / `detach()` / `notify()` przyjmują `shared_ptr<ISessionObserver>`
- `attach()` usuwa wygasłe wpisy przed dodaniem i odrzuca duplikaty przez porównanie `wp.lock() == observer`
- `notify()` czyści wygasłe wpisy przed iteracją — brak ryzyka wiszącego wskaźnika
- `connectToEngine()` wywołuje teraz `attach(engine)` bezpośrednio (Engine jest `shared_ptr`)
- Nowy test: `ConnectToEngineTwiceDoesNotDuplicateObserver` — weryfikuje zabezpieczenie przed duplikatami
- `AuditObserverOwnedExternallyOnSessionClosing` — weryfikuje niezależność czasu życia od sesji

---

### `refactor: SessionEstablisher::establish() uses C++23 monadic std::expected chain` — `905330e`

**Template Method** + **monadyczny `std::expected` C++23** połączone w inicjalizacji sesji.

- `establish()` zwraca teraz `std::expected<void, std::string>`
- Ciało metody to jeden potok monadyczny:
  ```cpp
  return checkPreconditions()
      .and_then([this](...){ connect();       return {}; })
      .and_then([this](...){ configure();     return {}; })
      .and_then([this](...){ finalizeSetup(); return {}; })
      .transform([](){ logApp("Session established\n"); })
      .or_else([](const std::string& e){ logApp(e + " — aborting\n"); return unexpected(e); });
  ```
- Jeśli którykolwiek krok zwróci błąd, kolejne są automatycznie pomijane — bez łańcuchów `if`
- `checkPreconditions()` to wirtualny hook, który klasy pochodne mogą nadpisać
- Testy zaktualizowane do sprawdzania `result.has_value()`

---

### `refactor: replace throw in SortStrategyFactory with std::expected (C++23)` — `2466971`

**Factory** przeniesiona z wyjątków na `std::expected`.

- Zwracany typ `SortStrategyFactory::create()` zmieniony z `unique_ptr<ISortStrategy>` (z rzucaniem) na `expected<unique_ptr<ISortStrategy>, std::string>`
- Nieznany `SortStrategyId` zwraca `std::unexpected("SortStrategyFactory: unknown SortStrategyId")` zamiast rzucać wyjątkiem
- Konstruktor `Engine` wywołuje `.value()` (awaria przy nieznanym id w chwili startu — akceptowalne dla enuma bez zewnętrznego wejścia)
- `Engine::onSessionEvent()` używa `if (auto s = SortStrategyFactory::create(...))` do cichego ignorowania nieznanych zdarzeń zmiany strategii
- W całej bazie kodu nie ma żadnego `try/catch`

---

### `refactor: migrate Engine ownership to shared_ptr; SessionManagement uses weak_ptr` — `79e67b9`

Uściślenie modelu własności silnika.

- `Engine` jest teraz tworzony jako `shared_ptr<Engine>` w `Application`
- `SessionManagement::connectToEngine()` przyjmuje `shared_ptr<Engine>` i przechowuje go jako `weak_ptr<Engine>`
- Każda operacja sesji sprawdza `engine_.expired()` / `engine_.lock()` przed wykonaniem — brak możliwości wiszącej referencji
- Wszystkie testy zaktualizowane: `Engine engine;` → `auto engine = std::make_shared<Engine>()`

---

### `refactor: move SessionAuditObserver ownership to Application; add logFile audit` — `bd5a704`

Czas życia **Obserwatora** oddzielony od sesji.

- `SessionAuditObserver` jest teraz składową `shared_ptr` w `Application`; sesja trzyma jedynie `weak_ptr`
- Usunięto `delete this` z `SessionAuditObserver::onSessionEvent(SessionClosing)` — samozniszczenie było kruche i ukrywało błędy związane z czasem życia
- Dodano wywołania `logFile()` w `SessionAuditObserver` dla każdego typu zdarzenia (VectorAdded, SortRequested, PrintRequested, StrategyChangeRequested, SessionClosing) — ślad audytu trafia teraz zarówno na stdout, jak i do pliku dziennika
- Nowy test: `AuditObserverOwnedExternallyOnSessionClosing`

---

### `refactor: introduce Application class wrapping main lifecycle` — `3a71de9`

**Fasada** nad sekwencją startową.

- Dodano klasę `Application` w `include/patterns/app/Application.hpp` / `src/app/Application.cpp`
- Dwie publiczne metody: `configure()` (podpina wszystkie obiekty) i `run()` (steruje pętlą interakcji GUI)
- `Application` posiada: `shared_ptr<Engine>`, `shared_ptr<SessionManagement>`, `shared_ptr<SessionAuditObserver>`, `unique_ptr<DummyGui>`, `Configurator`
- `main.cpp` zredukowany do trzech linii: stworzenie `Application`, wywołanie `configure()`, wywołanie `run()`

---

## 2026-08-05

### `ci: require GCC 13 on ubuntu-24.04 for C++23 std::expected support` — `a381a4a`

- Workflow CI przypięty do `gcc-13` / `g++-13` na `ubuntu-24.04`
- Zapewnia dostępność nagłówka `<expected>` bez dodatkowych flag

### `ci: add workflow_dispatch trigger` — `c3aba80`

- Dodano `workflow_dispatch` do `.github/workflows/ci.yml` — umożliwia ręczne uruchamianie CI z poziomu interfejsu GitHub

---

### `refactor: migrate ServiceLocator to std::expected (C++23); expand test coverage` — `0533a0f`

**Service Locator** + **`std::expected` C++23** wprowadzone razem.

- `ServiceLocator::get<T>()` zastąpiony przez `tryGet<T>()` zwracający `expected<shared_ptr<T>, ServiceError>`
- Dodano enum `ServiceError`: `ServiceNotFound`, `TypeMismatch`
- Dodano skrót `logFile()` używający operacji monadycznej `.transform()` na `expected`
- Dodano `provideRuntime()` / `tryGetRuntime()` do przechowywania serwisów bez typów (przez `any`)
- Pokrycie testami rozszerzone: 18 nowych przypadków testowych obejmujących wszystkie ścieżki błędów i skróty

---

### `refactor: extract core and dummy_gui as CMake components; add C-style API, unique_ptr support, docs` — `da12e0e`

System budowania zrestrukturyzowany w układ wielu bibliotek.

- Biblioteka statyczna `components/core/`: cały kod strategii, obserwatora, serwisów, silnika
- Biblioteka statyczna `components/dummy_gui/`: `DummyGui`, `CommandBatchBuilder`, `Configurator`
- Dodano C-style factory API: `makeGUI()` / `deleteGUI()` w `DummyGui.hpp`
- Dodano specjalizację `std::default_delete<DummyGui>` — `unique_ptr<DummyGui>` automatycznie wywołuje `deleteGUI()`
- Dodano szablon `BasicDummyGui<Writer>` + alias `using DummyGui = BasicDummyGui<>`
- Dodano `CommandBatchBuilder` (wzorzec Builder) zastępujący doraźną konstrukcję paczek komend
- Dodano GoF Command: interfejs `ICommand` z `AddVectorCommand`, `SortVectorCommand`, `PrintDataCommand`
- Dodano pierwsze materiały wykładowe w `docs/`

---

### `refactor: replace raw SessionManagement ptr in DummyGui with weak_ptr; bump versions` — `6b80987`

- `DummyGui` zmieniony z surowego `SessionManagement*` na `weak_ptr<SessionManagement>`
- Każda akcja GUI blokuje słaby wskaźnik przed użyciem; loguje błąd, jeśli sesja wygasła
- Naprawia użycie zwolnionej pamięci, gdy `SessionManagement` jest niszczony podczas gdy GUI wciąż żyje

---

### `refactor: rename project wzorce→patterns, add manifest CRTP writer, version manifests via CMake` — `501c744`

- Przestrzeń nazw i nazwa binarki projektu zmienione z `wzorce` na `patterns`
- Dodano `ComponentManifestWriter` (CRTP) — zapisuje manifesty `.yml` z nazwą komponentu, wersją i znacznikiem czasu budowania
- Ciągi wersji wstrzykiwane przez CMake `configure_file` z szablonów `.yml.in`
- Wprowadzono szablon `BasicEngine<Writer>` dla testowalności (wymienny zapis manifestu)

---

### `Add C++ design patterns project with tests and CI` — `e436289`

Pierwszy commit.

- Wzorce: Singleton, Service Locator, Strategy, Factory, Observer, Facade, Template Method, Builder
- Integracja Google Test przez `FetchContent`
- CI na GitHub Actions (Ubuntu + macOS)
- Klasy: `SessionManagement`, `Engine`, `DummyGui`, `Configurator`, `SessionEstablisher`, `SessionAuditObserver`
