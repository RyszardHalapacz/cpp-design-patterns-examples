# Diagram klas UML — relacje między klasami

Diagram pokazuje relacje **statyczne** (jak klasy są ze sobą powiązane
w kodzie), w odróżnieniu od diagramów sekwencji, które pokazują konkretny
przepływ wywołań w czasie.

Blok poniżej to kod [mermaid.js](https://mermaid.js.org/) — renderuje się
automatycznie jako diagram w GitHub, GitLab, Obsidian, VS Code (z wtyczką
Markdown Preview Mermaid Support) i wielu innych narzędziach do podglądu
markdown. Jeśli Twój podgląd tego nie renderuje, kod źródłowy poniżej
nadal czytelnie opisuje wszystkie relacje.

```mermaid
classDiagram
    direction TB

    %% ─── Interfejsy / klasy abstrakcyjne ───────────────────────────────────────
    class IService            { <<interface>> }
    class ISessionObserver    { <<interface>> }
    class ISortStrategy       { <<interface>> }
    class ISortStrategyFactory{ <<interface>> }
    class IGui                { <<interface>> }
    class SessionCoordinator  { <<abstract>> }

    %% ─── Klasy konkretne ────────────────────────────────────────────────────────
    class Application
    class Logger
    class FileLogger
    class DoSomething
    class ServiceLocator
    class Engine
    class EngineHistorian
    class SessionAuditObserver
    class SessionManagement
    class AscendingSortStrategy
    class DescendingSortStrategy
    class BubbleSortStrategy
    class SortStrategyFactory
    class DummyGui
    class DummyGuiAdapter
    class CommandBatchBuilder
    class Configurator
    class EngineSessionCoordinator
    class SortStrategyId { <<enumeration>> }

    %% ─── Application: korzeń grafu obiektów ────────────────────────────────────
    Application *-- IGui                  : unique_ptr
    Application *-- Configurator          : przez wartość
    Application o-- Engine                : shared_ptr
    Application o-- ISortStrategyFactory  : shared_ptr
    Application o-- EngineHistorian       : shared_ptr
    Application o-- SessionManagement     : shared_ptr
    Application o-- SessionAuditObserver  : shared_ptr
    Application ..> EngineSessionCoordinator : tworzy w configure()

    %% ─── Usługi (ServiceLocator) ────────────────────────────────────────────────
    IService <|.. Logger
    IService <|.. FileLogger
    IService <|.. DoSomething
    ServiceLocator o-- IService : mapa shared_ptr

    %% ─── Wzorzec Observer ───────────────────────────────────────────────────────
    ISessionObserver <|.. Engine
    ISessionObserver <|.. SessionAuditObserver
    SessionManagement o-- ISessionObserver : weak_ptr[]
    SessionManagement --> Engine           : weak_ptr

    %% ─── Strategia + Fabryka ────────────────────────────────────────────────────
    ISortStrategy <|.. AscendingSortStrategy
    ISortStrategy <|.. DescendingSortStrategy
    ISortStrategy <|.. BubbleSortStrategy
    Engine *-- ISortStrategy         : unique_ptr
    Engine --> ISortStrategyFactory  : weak_ptr
    Engine --> EngineHistorian       : weak_ptr
    ISortStrategyFactory <|.. SortStrategyFactory
    SortStrategyFactory ..> ISortStrategy : tworzy

    %% ─── Wzorzec Adapter ────────────────────────────────────────────────────────
    IGui <|.. DummyGuiAdapter
    DummyGuiAdapter *-- DummyGui        : unique_ptr
    DummyGui *-- CommandBatchBuilder    : przez wartość

    %% ─── Metoda Szablonowa ──────────────────────────────────────────────────────
    SessionCoordinator <|-- EngineSessionCoordinator
    EngineSessionCoordinator --> SessionManagement   : referencja
    EngineSessionCoordinator --> Engine              : shared_ptr
    EngineSessionCoordinator --> EngineHistorian     : shared_ptr

    %% ─── Configurator ───────────────────────────────────────────────────────────
    Configurator ..> DummyGuiAdapter   : rejestruje handlery
    Configurator ..> SessionManagement : ustala politykę

    classDef interfaceStyle fill:#EEEDFE,stroke:#7F77DD,color:#26215C
    classDef serviceStyle   fill:#E1F5EE,stroke:#0F6E56,color:#0B3D30
    classDef coreStyle      fill:#FAECE7,stroke:#D85A30,color:#4A1B0C
    classDef guiStyle       fill:#FFF6DE,stroke:#C08A00,color:#4A3900
    classDef appStyle       fill:#E8F4FD,stroke:#2980B9,color:#1A3A4A
    classDef newStyle       fill:#FDE8D7,stroke:#B8551A,color:#5C2A0D

    cssClass "IService,ISessionObserver,ISortStrategy,ISortStrategyFactory,IGui,SessionCoordinator" interfaceStyle
    cssClass "Logger,FileLogger,DoSomething,ServiceLocator" serviceStyle
    cssClass "Engine,SessionManagement,SessionAuditObserver,EngineSessionCoordinator,EngineHistorian" coreStyle
    cssClass "DummyGui,DummyGuiAdapter,Configurator,CommandBatchBuilder,AscendingSortStrategy,DescendingSortStrategy,BubbleSortStrategy" guiStyle
    cssClass "Application" appStyle
    cssClass "SortStrategyFactory,SortStrategyId" newStyle
```

## Grupy kolorystyczne

- **niebieski** — `Application` — korzeń grafu obiektów; właściciel wszystkich obiektów najwyższego poziomu
- **fioletowy** — interfejsy/klasy abstrakcyjne (`IService`, `ISessionObserver`, `ISortStrategy`, `ISortStrategyFactory`, `IGui`, `SessionCoordinator`)
- **zielony** — usługi rejestrowane w `ServiceLocator` (`Logger`, `FileLogger`, `DoSomething`, sam `ServiceLocator`)
- **koralowy** — rdzeń logiki sesji (`Engine`, `EngineHistorian`, `SessionManagement`, `SessionAuditObserver`, `EngineSessionCoordinator`)
- **żółty** — GUI, konfiguracja, strategie konkretne (`DummyGui`, `DummyGuiAdapter`, `Configurator`, `CommandBatchBuilder`, trzy klasy strategii)
- **pomarańczowy** — najnowsze dodatki (`SortStrategyFactory`, `SortStrategyId`)

## Legenda notacji UML

| Symbol | Nazwa | Znaczenie | Przykład w kodzie |
|---|---|---|---|
| `▷──` (linia ciągła, pusty trójkąt) | Generalizacja (dziedziczenie) | Klasa pochodna dziedziczy implementację po klasie bazowej | `EngineSessionCoordinator : public SessionCoordinator` |
| `▷┄┄` (linia przerywana, pusty trójkąt) | Realizacja (implementacja interfejsu) | Klasa implementuje czysto abstrakcyjny interfejs | `class Engine : public ISessionObserver` |
| `◆──` (pełny romb) | Kompozycja | Silne posiadanie — część nie istnieje bez całości (`unique_ptr`, pole wartościowe) | `Application` posiada `unique_ptr<IGui>` |
| `◇──` (pusty romb) | Agregacja | Współposiadanie — część może przeżyć właściciela (`shared_ptr`) | `Application o-- Engine` (shared_ptr) |
| `──>` (zwykła strzałka, linia ciągła) | Asocjacja | Jedna klasa trwale trzyma (ewentualnie słabą) referencję do drugiej | `Engine --> EngineHistorian : weak_ptr` |
| `┄┄>` (zwykła strzałka, linia przerywana) | Zależność | Jedna klasa używa drugiej lokalnie (parametr, zmienna lokalna) — bez pola składowego | `Configurator ..> DummyGuiAdapter` |

## Skąd bierze się każda relacja w kodzie

**Własności Application (Application.hpp / Application.cpp)**

```
unique_ptr<IGui>                   gui_          → kompozycja   (wyłączne posiadanie)
Configurator                       configurator_  → kompozycja   (pole wartościowe)
shared_ptr<Engine>                 engine_        → agregacja    (współposiada z koordynatorem)
shared_ptr<ISortStrategyFactory>   factory_       → agregacja    (utrzymuje factory żywą dla weak_ptr Engine)
shared_ptr<EngineHistorian>        historian_     → agregacja    (utrzymuje historiana żywego dla weak_ptr Engine)
shared_ptr<SessionManagement>      session_       → agregacja
shared_ptr<SessionAuditObserver>   audit_         → agregacja    (czas życia obserwatora niezależny od sesji)
EngineSessionCoordinator           (zmienna lokalna) → zależność (tworzony i niszczony w configure())
```

**Engine (Engine.hpp)**

```
unique_ptr<ISortStrategy>          sortStrategy_  → kompozycja  (zamiana strategii zastępuje stary obiekt)
weak_ptr<ISortStrategyFactory>     factory_       → asocjacja   (Application utrzymuje przy życiu; Engine używa per zdarzenie)
weak_ptr<EngineHistorian>          historian_     → asocjacja   (Application utrzymuje przy życiu; disableHistorian() wygasza weak_ptr)
```

**SessionManagement (SessionManagement.hpp)**

```
weak_ptr<Engine>                   engine_        → asocjacja   (Engine posiadany przez Application, nie przez sesję)
vector<weak_ptr<ISessionObserver>> observers_     → agregacja   (wygasłe obserwatory usuwane automatycznie)
```

**Wzorzec Adapter (DummyGuiAdapter.hpp)**

```
DummyGuiAdapter implementuje IGui — Application widzi tylko IGui
unique_ptr<DummyGui> gui_          → kompozycja  (jedyny właściciel; deleteGUI() przez default_delete)
CommandBatchBuilder  batchBuilder_ → kompozycja  (pole wartościowe w DummyGui)
```

**Metoda Szablonowa (SessionCoordinator.hpp)**

```
EngineSessionCoordinator posiada:
  SessionManagement&                session_   → asocjacja  (referencja, nie posiadanie)
  shared_ptr<Engine>                engine_    → agregacja  (współposiada podczas establish())
  shared_ptr<ISortStrategyFactory>  factory_   → agregacja  (współposiada podczas establish())
  shared_ptr<EngineHistorian>       historian_ → agregacja  (współposiada; enableHistorian() tworzy nowy)
```
