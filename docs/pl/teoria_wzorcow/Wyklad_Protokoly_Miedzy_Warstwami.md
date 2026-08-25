# Wykład -- Protokoły komunikacji pomiędzy warstwami

## Wprowadzenie

Dobrze zaprojektowany system nie polega na tym, że klasy znają swoje
wnętrze. Każda warstwa udostępnia jedynie **protokół komunikacji**
(kontrakt), dzięki któremu kolejna warstwa wie:

-   jakie operacje może wykonać,
-   jakie dane przekazać,
-   czego może oczekiwać w odpowiedzi.

Dzięki temu każdą warstwę można rozwijać niezależnie.

------------------------------------------------------------------------

# Granica 1 -- Użytkownik → DummyGui

### Protokół

Wywołania metod GUI.

Przekazywane są dane biznesowe:

-   liczby wektora,
-   numer strategii,
-   polecenie wydruku.

Przykład:

``` cpp
queueAddVector(...)
queueSortVector(...)
queuePrintData()
```

GUI nie wykonuje logiki biznesowej.

------------------------------------------------------------------------

# Granica 2 -- DummyGui → CommandBatchBuilder

### Protokół

Tworzenie obiektów `Command`.

Przekazywane są:

-   typ polecenia,
-   parametry polecenia.

Powstaje:

``` text
Command
```

Builder jedynie gromadzi polecenia.

------------------------------------------------------------------------

# Granica 3 -- CommandBatchBuilder → CommandBatch

### Protokół

``` cpp
build()
```

Przekazywane jest:

``` text
vector<Command>
```

przez `std::move()`.

Od tego momentu Builder jest pusty.

------------------------------------------------------------------------

# Granica 4 -- DummyGui → SessionManagement

### Protokół

``` cpp
executeBatch(batch)
```

Przekazywany jest:

``` text
const CommandBatch&
```

SessionManagement nie zna GUI.

Wie jedynie jak wykonać listę poleceń.

------------------------------------------------------------------------

# Granica 5 -- SessionManagement → Engine / Observer

Najważniejsza granica projektu.

Command zostaje zamieniony na:

``` text
SessionEvent
```

Każdy observer otrzymuje

``` cpp
const SessionEvent&
```

Nie zna Command.

Nie zna GUI.

Nie zna Buildera.

Zna wyłącznie zdarzenie.

------------------------------------------------------------------------

# Granica 6 -- Engine → ISortStrategy

Engine posiada jedynie wskaźnik:

``` cpp
ISortStrategy*
```

Wywołuje

``` cpp
(*strategy)(vector);
```

Przekazywane są:

-   referencja do wektora.

Engine nie zna konkretnego algorytmu.

------------------------------------------------------------------------

# Granica 7 -- SessionManagement → SessionAuditObserver

Przekazywany jest dokładnie ten sam

``` text
SessionEvent
```

Audytor zapisuje informację do loggera.

Nie wpływa na logikę biznesową.

------------------------------------------------------------------------

# Co przechodzi przez poszczególne granice?

  Granica                         Protokół         Dane
  ------------------------------- ---------------- ---------------------------
  User → DummyGui                 metody GUI       dane użytkownika
  DummyGui → Builder              Command          typ polecenia + parametry
  Builder → CommandBatch          build()          vector`<Command>`{=html}
  DummyGui → SessionManagement    executeBatch()   CommandBatch
  SessionManagement → Observers   notify()         SessionEvent
  Engine → Strategy               operator()       vector`<int>`{=html}&
  SessionManagement → Audit       notify()         SessionEvent

------------------------------------------------------------------------

# Dlaczego jest to dobre rozwiązanie?

Każda warstwa zna tylko swój protokół.

Na przykład:

-   Engine nie zna GUI.
-   GUI nie zna Engine.
-   Observer nie zna Command.
-   Strategy nie zna SessionManagement.
-   Audit nie zna Engine.

Każdy element widzi wyłącznie dane przekazywane przez granicę.

------------------------------------------------------------------------

# Najważniejsza myśl

Architektura projektu opiera się na jasno zdefiniowanych granicach.

Każda granica posiada własny protokół komunikacji oraz własny typ
danych:

``` text
User
    │
    ▼
DummyGui
    │  Command
    ▼
CommandBatchBuilder
    │  vector<Command>
    ▼
CommandBatch
    │
    ▼
SessionManagement
    │  SessionEvent
    ├──────────────► Engine
    │                  │
    │                  └──► ISortStrategy
    │
    └──────────────► SessionAuditObserver
```

Dzięki temu warstwy są luźno powiązane, łatwe do wymiany i proste do
testowania.
