# Wykład — Protokoły komunikacji pomiędzy warstwami

## Wprowadzenie

Warstwy w projekcie nie komunikują się dowolnie. Każda granica ma własny protokół, czyli ustalony sposób wywołania oraz określone dane, które mogą przez nią przejść.

W tym projekcie istnieją dwa różne sposoby komunikacji pomiędzy `DummyGui` i `SessionManagement`:

1. bezpośrednie wywołanie metod sesji,
2. odroczone wykonanie przez `CommandBatchBuilder`.

To ważne, ponieważ Builder nie jest jedyną drogą komunikacji.

---

# Granica 1 — Użytkownik → DummyGui

## Protokół

Użytkownik wywołuje publiczne metody GUI.

Przykłady trybu bezpośredniego:

```cpp
clickAddVector(...)
clickSortVector(...)
clickPrintData()
clickSetSortStrategy(...)
```

Przykłady trybu kolejkowanego:

```cpp
queueAddVector(...)
queueSortVector(...)
queuePrintData(...)
flushBatch()
```

## Co jest przekazywane?

- dane nowego wektora,
- indeks wektora,
- identyfikator strategii,
- informacja o żądaniu wydruku.

`DummyGui` przyjmuje żądanie użytkownika, ale nie wykonuje samodzielnie logiki biznesowej.

---

# Granica 2A — DummyGui → SessionManagement bezpośrednio

Jest to natychmiastowy protokół komunikacji.

`DummyGui` posiada globalne wskaźniki do metod `SessionManagement`. Wskaźniki te są początkowo puste, a następnie `Configurator` przypisuje tylko te operacje, które mają być dostępne.

Przykładowe wywołania:

```cpp
(session_->*addVectorFunc_)(vec);
(session_->*sortVectorFunc_)(index);
(session_->*printDataFunc_)();
(session_->*setSortStrategyFunc_)(id);
```

## Co jest przekazywane?

- `const std::vector<int>&` — wektor do dodania,
- `size_t` — indeks wektora do posortowania,
- `SortStrategyId` — wybrana strategia,
- brak argumentu dla wydruku.

## Charakter protokołu

Operacja wykonywana jest od razu:

```text
DummyGui
    │
    │ wywołanie wskaźnika do metody
    ▼
SessionManagement
```

W tym wariancie nie powstają:

- `Command`,
- `CommandBatch`,
- `CommandBatchBuilder`.

---

# Granica 2B — DummyGui → CommandBatchBuilder

Jest to alternatywny protokół używany do odroczonego wykonania wielu poleceń.

GUI korzysta z metod:

```cpp
queueAddVector(...)
queueSortVector(...)
queuePrintData(...)
```

Każde wywołanie dodaje kolejny obiekt `Command` do Buildera.

## Co jest przekazywane?

- typ polecenia,
- parametry potrzebne do jego późniejszego wykonania.

Powstaje:

```text
Command
```

Builder nie wykonuje operacji. Jedynie przechowuje ich opis.

---

# Granica 3 — CommandBatchBuilder → CommandBatch

## Protokół

```cpp
build()
```

Builder przenosi zgromadzone polecenia do nowej paczki:

```cpp
CommandBatch result = std::move(commands_);
```

## Co jest przekazywane?

```text
std::vector<Command>
```

Po wykonaniu `build()`:

- `CommandBatch` posiada wszystkie polecenia,
- Builder zostaje opróżniony.

---

# Granica 4 — DummyGui → SessionManagement przez batch

## Protokół

Metoda:

```cpp
flushBatch()
```

buduje paczkę, a następnie wywołuje:

```cpp
(session_->*executeBatchFunc_)(batch);
```

Po stronie `SessionManagement` protokół ma postać:

```cpp
executeBatch(const CommandBatch& batch)
```

## Co jest przekazywane?

```cpp
const CommandBatch&
```

SessionManagement odczytuje po kolei wszystkie obiekty `Command`.

---

# Granica 5 — SessionManagement → obserwatorzy

`SessionManagement` nie przekazuje obserwatorom obiektu `Command`.

Na podstawie aktualnego polecenia tworzy nowy:

```text
SessionEvent
```

Następnie wywołuje:

```cpp
notify(event);
```

Każdy obserwator otrzymuje:

```cpp
const SessionEvent&
```

## Co zawiera SessionEvent?

Zależnie od rodzaju operacji może zawierać:

- typ zdarzenia,
- dane wektora,
- indeks wektora,
- inne dane potrzebne obserwatorom.

Ten sam obiekt zdarzenia jest przekazywany kolejno do wszystkich obserwatorów.

---

# Granica 6 — SessionManagement → Engine

`Engine` implementuje protokół obserwatora:

```cpp
onSessionEvent(const SessionEvent& event)
```

## Co jest przekazywane?

```cpp
const SessionEvent&
```

`Engine` analizuje `event.type` i wykonuje odpowiednią operację:

```text
VectorAdded
    → addVector()

SortRequested
    → sortVector()

PrintRequested
    → printData()
```

`Engine` nie zna:

- GUI,
- Buildera,
- `CommandBatch`,
- sposobu, w jaki żądanie zostało utworzone.

Zna wyłącznie `SessionEvent`.

---

# Granica 7 — Engine → ISortStrategy

Jeżeli zdarzenie wymaga sortowania, `Engine` przekazuje dane do aktualnej strategii.

## Protokół

```cpp
(*sortStrategy_)(vector);
```

## Co jest przekazywane?

```cpp
std::vector<int>&
```

Strategia otrzymuje bezpośredni dostęp do wektora i sortuje go w miejscu.

`Engine` nie zna konkretnego algorytmu. Widzi jedynie interfejs `ISortStrategy`.

---

# Granica 8 — SessionManagement → SessionAuditObserver

`SessionAuditObserver` również implementuje:

```cpp
onSessionEvent(const SessionEvent& event)
```

Otrzymuje dokładnie ten sam event, który otrzymuje `Engine`.

## Co jest przekazywane?

```cpp
const SessionEvent&
```

Audytor:

- rozpoznaje typ zdarzenia,
- przygotowuje wpis diagnostyczny,
- przekazuje informację do loggera.

Nie wykonuje logiki biznesowej i nie modyfikuje danych `Engine`.

---

# Podsumowanie protokołów

| Granica | Protokół | Co jest przekazywane |
|---|---|---|
| Użytkownik → DummyGui | metody `click...()` lub `queue...()` | dane użytkownika |
| DummyGui → SessionManagement bezpośrednio | wskaźniki do metod | wektor, indeks, `SortStrategyId` lub brak argumentu |
| DummyGui → CommandBatchBuilder | metody `queue...()` | typ polecenia i jego parametry |
| CommandBatchBuilder → CommandBatch | `build()` | `std::vector<Command>` |
| DummyGui → SessionManagement przez batch | `executeBatch()` | `const CommandBatch&` |
| SessionManagement → Engine | `onSessionEvent()` | `const SessionEvent&` |
| SessionManagement → SessionAuditObserver | `onSessionEvent()` | ten sam `const SessionEvent&` |
| Engine → ISortStrategy | `operator()` | `std::vector<int>&` |

---

# Pełny przepływ

```text
Użytkownik
    │
    ▼
DummyGui
    │
    ├── tryb bezpośredni
    │       │
    │       └──────────────► SessionManagement
    │                            │
    │                            │ SessionEvent
    │                            ├────────► Engine
    │                            │            │
    │                            │            └────► ISortStrategy
    │                            │
    │                            └────────► SessionAuditObserver
    │
    └── tryb kolejkowany
            │
            ▼
      CommandBatchBuilder
            │
            │ build()
            ▼
       CommandBatch
            │
            │ executeBatch()
            └──────────────► SessionManagement
                                 │
                                 │ SessionEvent
                                 ├────────► Engine
                                 └────────► SessionAuditObserver
```

# Najważniejszy wniosek

`DummyGui` może komunikować się z `SessionManagement` na dwa sposoby:

- bezpośrednio przez przypisane wskaźniki do metod,
- pośrednio przez `CommandBatchBuilder` i `CommandBatch`.

Builder jest więc dodatkowym mechanizmem do grupowania i odroczonego wykonania poleceń, a nie obowiązkową częścią każdej komunikacji z sesją.
