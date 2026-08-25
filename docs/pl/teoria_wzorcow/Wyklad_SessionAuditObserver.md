# Wykład: `SessionAuditObserver`

## Wprowadzenie

`SessionAuditObserver` jest niewielką klasą, ale pełni bardzo ważną rolę
dydaktyczną.

Pokazuje, że obserwator **nie musi wykonywać logiki biznesowej**. Jego
zadaniem jest reagowanie na zdarzenia publikowane przez
`SessionManagement` i wykonywanie własnej, niezależnej
odpowiedzialności.

------------------------------------------------------------------------

# Rola klasy

Najkrócej:

> `SessionAuditObserver` obserwuje przebieg sesji i zapisuje informacje
> o zdarzeniach (audyt), ale nie wpływa na działanie aplikacji.

Nie:

-   sortuje danych,
-   dodaje wektorów,
-   zmienia strategii,
-   steruje `Engine`.

Jedynie reaguje na zdarzenia.

------------------------------------------------------------------------

# Implementuje `ISessionObserver`

Podobnie jak `Engine`, klasa dziedziczy po:

``` cpp
ISessionObserver
```

Dzięki temu `SessionManagement` traktuje oba obiekty identycznie.

``` text
                ISessionObserver
                       ▲
          ┌────────────┴────────────┐
          │                         │
          ▼                         ▼
      Engine             SessionAuditObserver
```

------------------------------------------------------------------------

# Najważniejsza metoda

``` cpp
void onSessionEvent(const SessionEvent& event)
```

To właśnie tę metodę wywołuje `SessionManagement` podczas:

``` cpp
notify(event);
```

Każdy zarejestrowany obserwator otrzymuje to samo zdarzenie.

------------------------------------------------------------------------

# Jak wygląda przepływ?

``` text
SessionManagement
        │
        ▼
 notify(event)
        │
 ┌──────┴────────┐
 ▼               ▼
Engine   SessionAuditObserver
 │               │
 │               ▼
 │        appLogger().log(...)
 ▼
wykonanie logiki
biznesowej
```

------------------------------------------------------------------------

# Czy audytor uruchamia logger?

Tak.

W obecnym przykładzie `SessionAuditObserver` jest klientem loggera.

Po odebraniu zdarzenia wywołuje:

``` text
event
   │
   ▼
SessionAuditObserver
   │
   ▼
appLogger().log(...)
```

------------------------------------------------------------------------

# Czy tylko audytor korzysta z loggera?

Nie.

Z loggera korzysta wiele klas:

``` text
Engine
        ─────► Logger

SessionManagement
        ─────► Logger

Configurator
        ─────► Logger

SessionEstablisher
        ─────► Logger

SessionAuditObserver
        ─────► Logger

PlacementEngine
        ─────► Logger
```

Każda loguje własne informacje.

------------------------------------------------------------------------

# Dlaczego więc istnieje `SessionAuditObserver`?

Ponieważ jego odpowiedzialność jest zupełnie inna.

`Engine` loguje podczas wykonywania swojej pracy.

Natomiast `SessionAuditObserver` istnieje wyłącznie po to, aby reagować
na zdarzenia publikowane przez `SessionManagement`.

To bardzo dobry przykład zasady **Single Responsibility Principle**.

------------------------------------------------------------------------

# W prawdziwej aplikacji

Dzisiaj audytor wykonuje:

``` text
event
  │
  ▼
logger
```

Ale równie dobrze mógłby:

-   zapisywać zdarzenia do pliku,
-   wysyłać je do bazy danych,
-   publikować do systemu telemetrycznego,
-   generować statystyki,
-   tworzyć historię działań użytkownika.

Cała reszta aplikacji pozostałaby bez zmian.

------------------------------------------------------------------------

# Największa zaleta

Do systemu można łatwo dopisywać kolejnych obserwatorów:

``` text
notify(event)
      │
      ├── Engine
      ├── SessionAuditObserver
      ├── StatisticsObserver
      ├── TelemetryObserver
      └── MetricsObserver
```

`SessionManagement` nie wymaga żadnych zmian.

To realizuje zasadę **Open/Closed Principle**.

------------------------------------------------------------------------

# Wzorce i zasady

`SessionAuditObserver` jest:

-   Concrete Observer,
-   implementacją `ISessionObserver`,
-   przykładem polimorfizmu,
-   przykładem SRP,
-   elementem wzorca Observer.

------------------------------------------------------------------------

# Podsumowanie

`SessionAuditObserver` jest obserwatorem technicznym.

Nie wykonuje logiki biznesowej. Odbiera zdarzenia publikowane przez
`SessionManagement` i zapisuje informacje o ich przebiegu.

Dzięki temu odpowiedzialność za audyt jest całkowicie oddzielona od
odpowiedzialności `Engine`, a do systemu można łatwo dodawać kolejne
reakcje na te same zdarzenia bez modyfikowania istniejącego kodu.
