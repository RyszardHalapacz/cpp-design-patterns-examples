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

    class IService { <<interface>> }
    class ISessionObserver { <<interface>> }
    class ISortStrategy { <<interface>> }
    class SessionEstablisher { <<abstract>> }

    class Logger
    class FileLogger
    class DoSomething
    class ServiceLocator
    class Engine
    class SessionAuditObserver
    class SessionManagement
    class AscendingSortStrategy
    class DescendingSortStrategy
    class BubbleSortStrategy
    class SortStrategyFactory
    class DummyGui
    class CommandBatchBuilder
    class Configurator
    class EngineSessionEstablisher
    class SortStrategyId { <<enumeration>> }

    IService <|.. Logger
    IService <|.. FileLogger
    IService <|.. DoSomething
    ServiceLocator o-- IService : rejestruje po typie

    ISessionObserver <|.. Engine
    ISessionObserver <|.. SessionAuditObserver
    SessionManagement o-- ISessionObserver : obserwatorzy
    SessionManagement --> Engine : steruje
    SessionManagement ..> SortStrategyId : notify przenosi enum

    ISortStrategy <|.. AscendingSortStrategy
    ISortStrategy <|.. DescendingSortStrategy
    ISortStrategy <|.. BubbleSortStrategy
    Engine *-- ISortStrategy : posiada strategię
    Engine ..> SortStrategyFactory : tworzy strategię
    SortStrategyFactory ..> ISortStrategy : tworzy

    DummyGui --> SessionManagement : woła metody
    DummyGui *-- CommandBatchBuilder : buduje paczki
    DummyGui ..> SortStrategyId : clickSetSortStrategy(id)

    Configurator --> DummyGui : konfiguruje
    Configurator --> SessionManagement : ustala politykę

    SessionEstablisher <|-- EngineSessionEstablisher
    EngineSessionEstablisher --> SessionManagement : łączy
    EngineSessionEstablisher --> Engine : łączy

    classDef interfaceStyle fill:#EEEDFE,stroke:#7F77DD,color:#26215C
    classDef serviceStyle fill:#E1F5EE,stroke:#0F6E56,color:#0B3D30
    classDef coreStyle fill:#FAECE7,stroke:#D85A30,color:#4A1B0C
    classDef guiStyle fill:#FFF6DE,stroke:#C08A00,color:#4A3900
    classDef newStyle fill:#FDE8D7,stroke:#B8551A,color:#5C2A0D

    cssClass "IService,ISessionObserver,ISortStrategy,SessionEstablisher" interfaceStyle
    cssClass "Logger,FileLogger,DoSomething,ServiceLocator" serviceStyle
    cssClass "Engine,SessionManagement,SessionAuditObserver,EngineSessionEstablisher" coreStyle
    cssClass "DummyGui,Configurator,CommandBatchBuilder,AscendingSortStrategy,DescendingSortStrategy,BubbleSortStrategy" guiStyle
    cssClass "SortStrategyFactory,SortStrategyId" newStyle
```

## Grupy kolorystyczne

- **fioletowy** — interfejsy/klasy abstrakcyjne (`IService`, `ISessionObserver`, `ISortStrategy`, `SessionEstablisher`)
- **zielony** — usługi rejestrowane w `ServiceLocator` (`Logger`, `FileLogger`, `DoSomething`, sam `ServiceLocator`)
- **koralowy** — rdzeń logiki sesji (`Engine`, `SessionManagement`, `SessionAuditObserver`, `EngineSessionEstablisher`)
- **żółty** — GUI, konfiguracja, strategie konkretne (`DummyGui`, `Configurator`, `CommandBatchBuilder`, trzy klasy strategii)
- **pomarańczowy** — najnowsze dodatki (`SortStrategyFactory`, `SortStrategyId`)

## Legenda notacji UML

| Symbol | Nazwa | Znaczenie | Przykład w kodzie |
|---|---|---|---|
| `▷──` (linia ciągła, pusty trójkąt) | Generalizacja (dziedziczenie) | Klasa pochodna dziedziczy implementację po klasie bazowej | `EngineSessionEstablisher : public SessionEstablisher` |
| `▷┄┄` (linia przerywana, pusty trójkąt) | Realizacja (implementacja interfejsu) | Klasa implementuje czysto abstrakcyjny interfejs | `class Engine : public ISessionObserver` |
| `◆──` (pełny romb) | Kompozycja | Silne posiadanie — część nie istnieje bez całości (`unique_ptr`, pole wartościowe) | `Engine` posiada `unique_ptr<ISortStrategy>` |
| `◇──` (pusty romb) | Agregacja | Słabe posiadanie — część może istnieć niezależnie od całości (surowy wskaźnik, `shared_ptr`) | `SessionManagement` trzyma `vector<ISessionObserver*>` |
| `──>` (zwykła strzałka, linia ciągła) | Asocjacja | Jedna klasa trwale zna/steruje drugą | `SessionManagement --> Engine` |
| `┄┄>` (zwykła strzałka, linia przerywana) | Zależność | Jedna klasa używa drugiej doraźnie (jedno wywołanie), bez trzymania referencji | `Engine ..> SortStrategyFactory` |

## Skąd bierze się każda relacja w kodzie

- `IService <|.. Logger/FileLogger/DoSomething` — wszystkie trzy dziedziczą po `IService` (realizacja interfejsu), co pozwala `ServiceLocator` trzymać je w jednej, heterogenicznej kolekcji.
- `ServiceLocator o-- IService` — `ServiceLocator` trzyma `unordered_map<type_index, shared_ptr<IService>>`; to agregacja, bo `shared_ptr` oznacza współdzieloną, a nie wyłączną własność.
- `ISessionObserver <|.. Engine/SessionAuditObserver` — obie klasy implementują `onSessionEvent()`.
- `SessionManagement o-- ISessionObserver` — lista obserwatorów to surowe wskaźniki (`vector<ISessionObserver*>`), stąd agregacja, nie kompozycja.
- `Engine *-- ISortStrategy` — `Engine` trzyma `unique_ptr<ISortStrategy>`, czyli wyłączną własność — kompozycja.
- `Engine ..> SortStrategyFactory` i `SortStrategyFactory ..> ISortStrategy` — fabryka jest wołana metodą statyczną, nikt jej nie przechowuje jako pola — to zależność, nie asocjacja.
- `DummyGui *-- CommandBatchBuilder` — pole wartościowe (nie wskaźnik), więc kompozycja.
- `DummyGui/SessionManagement ..> SortStrategyId` — obie klasy przyjmują ten enum jako parametr metody, ale go nie przechowują — zależność.
