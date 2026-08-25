# Wykład: Zdarzenia sesji i funkcje pomocnicze

# Wprowadzenie

Oprócz głównych klas w projekcie występują niewielkie elementy
wspierające architekturę:

-   `SessionEvent`
-   `SessionEventType`
-   `DoSomething`
-   `appLogger()`
-   `appFileLogger()`
-   `appDoSomething()`

Choć są niewielkie, odpowiadają za komunikację pomiędzy modułami oraz
upraszczają korzystanie z usług.

------------------------------------------------------------------------

# `SessionEventType`

`SessionEventType` określa **jaki rodzaj zdarzenia wystąpił**.

Przykład:

``` cpp
enum class SessionEventType
{
    VectorAdded,
    SortRequested,
    PrintRequested,
    StrategyChangeRequested,
    SessionClosing
};
```

To odpowiednik `CommandType`, ale dla komunikacji pomiędzy komponentami.

------------------------------------------------------------------------

# `SessionEvent`

`SessionEvent` jest strukturą opisującą pojedyncze zdarzenie.

Przykład:

``` cpp
struct SessionEvent
{
    SessionEventType type;
    std::vector<int> vectorData;
    size_t index = 0;
    SortStrategyId strategyId = SortStrategyId::Ascending;
};
```

Zawiera wszystkie informacje potrzebne obserwatorowi do obsługi
zdarzenia.

------------------------------------------------------------------------

# `Command` a `SessionEvent`

Na pierwszy rzut oka wyglądają podobnie, ale mają zupełnie inne zadania.

## `Command`

Opisuje **co użytkownik chce wykonać**.

Powstaje w `CommandBatchBuilder`.

Przepływ:

``` text
GUI
 │
 ▼
CommandBatchBuilder
 │
 ▼
Command
```

------------------------------------------------------------------------

## `SessionEvent`

Opisuje **co właśnie wydarzyło się w sesji**.

Powstaje w `SessionManagement`.

Przepływ:

``` text
SessionManagement
 │
 ▼
SessionEvent
 │
 ▼
Observer
```

Najprościej:

-   `Command` = polecenie.
-   `SessionEvent` = zdarzenie.

------------------------------------------------------------------------

# Kto tworzy zdarzenia?

Najważniejszym producentem jest:

``` text
SessionManagement
```

Przykład:

``` cpp
notify(SessionEvent{
    SessionEventType::VectorAdded,
    vec,
    0
});
```

Klasa tworzy obiekt zdarzenia i przekazuje go wszystkim obserwatorom.

------------------------------------------------------------------------

# Kto odbiera zdarzenia?

Każda klasa implementująca:

``` cpp
ISessionObserver
```

otrzymuje:

``` cpp
void onSessionEvent(const SessionEvent& event);
```

W projekcie są to między innymi:

-   `Engine`
-   `SessionAuditObserver`

------------------------------------------------------------------------

# Observer i `SessionEvent`

Przepływ wygląda następująco:

``` text
DummyGui
      │
      ▼
SessionManagement
      │
      ▼
notify(event)
      │
 ┌────┴─────────┐
 ▼              ▼
Engine   SessionAuditObserver
```

Każdy obserwator otrzymuje ten sam obiekt `SessionEvent`, ale może
zareagować inaczej.

------------------------------------------------------------------------

# Funkcje pomocnicze

W projekcie znajdują się krótkie funkcje:

``` cpp
appLogger()
appFileLogger()
appDoSomething()
```

Ich zadaniem jest uproszczenie dostępu do `ServiceLocator`.

Bez nich należałoby pisać:

``` cpp
ServiceLocator::instance()
    .get<Logger>()
    .log("Hello");
```

Dzięki funkcji pomocniczej wystarczy:

``` cpp
appLogger().log("Hello");
```

------------------------------------------------------------------------

# `appLogger()`

``` cpp
inline Logger& appLogger()
{
    return ServiceLocator::instance().get<Logger>();
}
```

Zwraca referencję do globalnego loggera.

------------------------------------------------------------------------

# `appFileLogger()`

``` cpp
inline FileLogger& appFileLogger()
{
    return ServiceLocator::instance().get<FileLogger>();
}
```

Zapewnia prosty dostęp do loggera plikowego.

------------------------------------------------------------------------

# `appDoSomething()`

``` cpp
inline DoSomething& appDoSomething()
{
    return ServiceLocator::instance().get<DoSomething>();
}
```

Pokazuje, że `ServiceLocator` może przechowywać dowolne usługi, a nie
tylko loggery.

------------------------------------------------------------------------

# Klasa `DoSomething`

`DoSomething` jest prostą usługą dydaktyczną.

Nie realizuje logiki biznesowej -- jej jedynym zadaniem jest pokazanie,
że `ServiceLocator` potrafi przechowywać dowolne klasy dziedziczące po
`IService`.

------------------------------------------------------------------------

# Podsumowanie

-   `SessionEventType` określa rodzaj zdarzenia.
-   `SessionEvent` przenosi dane zdarzenia do obserwatorów.
-   `Command` opisuje polecenie użytkownika, natomiast `SessionEvent`
    opisuje zdarzenie rozsyłane przez `SessionManagement`.
-   `SessionManagement` tworzy zdarzenia, a klasy implementujące
    `ISessionObserver` je odbierają.
-   `appLogger()`, `appFileLogger()` i `appDoSomething()` upraszczają
    korzystanie z usług zarejestrowanych w `ServiceLocator`.
