# Wykład: Klasa `SessionManagement`

## Wprowadzenie

`SessionManagement` jest jedną z najważniejszych klas w całym
przykładzie. Nie wykonuje logiki biznesowej, lecz koordynuje współpracę
pomiędzy pozostałymi elementami systemu.

Jest centralnym punktem komunikacji pomiędzy GUI, silnikiem oraz
obserwatorami.

------------------------------------------------------------------------

# Czym jest `SessionManagement`?

Najprościej można powiedzieć:

> `SessionManagement` jest centralnym koordynatorem aplikacji.

Sam nie:

-   sortuje danych,
-   przechowuje danych,
-   tworzy strategii,
-   obsługuje GUI.

Jego zadaniem jest jedynie koordynowanie komunikacji.

              DummyGui
                  │
                  ▼
          SessionManagement
          ├──────────────┐
          │              │
          ▼              ▼
        Engine     SessionAuditObserver

------------------------------------------------------------------------

# Wzorce projektowe

## Facade

GUI komunikuje się wyłącznie z `SessionManagement`.

Zamiast:

``` cpp
engine.sortVector(...);
```

wywoływane jest:

``` cpp
session.sortVectorFromGui(...);
```

GUI nie zna szczegółów działania silnika.

------------------------------------------------------------------------

## Observer (Subject)

Klasa udostępnia klasyczne operacje:

``` cpp
attach();
detach();
notify();
```

oraz przechowuje listę obserwatorów:

``` cpp
std::vector<ISessionObserver*> observers_;
```

------------------------------------------------------------------------

## Mediator (częściowo)

GUI i Engine nie komunikują się bezpośrednio.

    GUI
     │
     ▼
    SessionManagement
     │
     ▼
    Engine

------------------------------------------------------------------------

## Dispatcher poleceń

Metoda `executeBatch()` zamienia kolejne komendy na zdarzenia wysyłane
do obserwatorów.

    CommandBatch
          │
          ▼
    Command
          │
          ▼
    SessionEvent
          │
          ▼
    notify()

------------------------------------------------------------------------

# Odpowiedzialności klasy

-   otwieranie sesji,
-   zamykanie sesji,
-   rejestracja obserwatorów,
-   rozsyłanie zdarzeń,
-   kontrola stanu sesji,
-   weryfikacja dozwolonych strategii,
-   wykonywanie batchy,
-   udostępnianie prostego API dla GUI.

------------------------------------------------------------------------

# Pola klasy

## engine\_

``` cpp
Engine* engine_;
```

`SessionManagement` nie jest właścicielem silnika.

Przechowuje jedynie wskaźnik do istniejącego obiektu.

------------------------------------------------------------------------

## sessionActive\_

``` cpp
bool sessionActive_;
```

Informacja o tym, czy sesja została uruchomiona.

------------------------------------------------------------------------

## observers\_

``` cpp
std::vector<ISessionObserver*> observers_;
```

Lista wszystkich obserwatorów.

Przykładowo:

-   `Engine`
-   `SessionAuditObserver`

------------------------------------------------------------------------

## allowedStrategies\_

``` cpp
std::vector<SortStrategyId>
```

Lista strategii dozwolonych przez konfigurację.

`SessionManagement` jedynie sprawdza, czy dana strategia została
dopuszczona przez `Configurator`.

------------------------------------------------------------------------

# connectToEngine()

Metoda:

1.  zapamiętuje wskaźnik do silnika,
2.  loguje operację,
3.  automatycznie rejestruje silnik jako obserwatora.

------------------------------------------------------------------------

# openSession()

Uruchamia sesję:

``` cpp
sessionActive_ = true;
```

oraz zleca uruchomienie silnika:

``` cpp
engine_->start();
```

------------------------------------------------------------------------

# closeSession()

Nie zatrzymuje bezpośrednio wszystkich komponentów.

Zamiast tego wysyła zdarzenie:

``` cpp
notify(SessionClosing);
```

Każdy obserwator sam decyduje, jak zareagować.

To klasyczny przykład wzorca Observer.

------------------------------------------------------------------------

# addVectorFromGui()

GUI wywołuje:

``` cpp
session.addVectorFromGui(...);
```

`SessionManagement` nie dodaje danych samodzielnie.

Tworzy zdarzenie `VectorAdded` i wysyła je do obserwatorów.

------------------------------------------------------------------------

# sortVectorFromGui()

Tworzy zdarzenie `SortRequested`, które trafia do `Engine`.

------------------------------------------------------------------------

# printDataFromGui()

Tworzy zdarzenie `PrintRequested`.

------------------------------------------------------------------------

# executeBatch()

Iteruje po wszystkich poleceniach z `CommandBatch`.

Każde polecenie zamieniane jest na odpowiedni `SessionEvent`, a
następnie przekazywane przez `notify()`.

Dzięki temu obserwatorzy nie wiedzą, czy polecenie pochodziło z GUI czy
z batcha.

------------------------------------------------------------------------

# setSortStrategyFromGui()

Najpierw sprawdza:

``` cpp
isStrategyAllowed(...)
```

Jeżeli strategia jest dozwolona, wysyłane jest zdarzenie:

``` cpp
StrategyChangeRequested
```

Dopiero `Engine` tworzy właściwy obiekt przy pomocy
`SortStrategyFactory`.

Schemat:

    GUI
     │
     ▼
    SessionManagement
     │
     ▼
    notify()
     │
     ▼
    Engine
     │
     ▼
    SortStrategyFactory
     │
     ▼
    ISortStrategy

------------------------------------------------------------------------

# attach()

Dodaje obserwatora do listy.

------------------------------------------------------------------------

# detach()

Usuwa obserwatora z listy.

------------------------------------------------------------------------

# notify()

Najważniejsza metoda klasy.

``` cpp
for(...)
{
    observer->onSessionEvent(event);
}
```

`SessionManagement` nie zna konkretnych klas obserwatorów.

Wie jedynie, że implementują interfejs:

``` cpp
ISessionObserver
```

------------------------------------------------------------------------

# checkSession()

Chroni API.

Jeżeli:

-   silnik nie został podłączony,
-   sesja nie została uruchomiona,

operacja nie zostanie wykonana.

------------------------------------------------------------------------

# Schemat współpracy

                    DummyGui
                        │
                        ▼
              SessionManagement
              │       │       │
              │       │       │
              ▼       ▼       ▼
         Observer   Facade  Batch
              │
              ▼
          notify(event)
              │
              ▼
            Engine
              │
              ▼
       SortStrategyFactory
              │
              ▼
          Strategy

------------------------------------------------------------------------

# Podsumowanie

`SessionManagement` pełni rolę centralnego koordynatora systemu.

Łączy w sobie elementy wzorców:

-   Facade,
-   Observer,
-   częściowo Mediator,
-   Dispatcher poleceń.

Największą zaletą tej klasy jest oddzielenie komunikacji od logiki
biznesowej. Dzięki temu GUI nie zna szczegółów implementacji silnika, a
`Engine` nie zna GUI. Każdy komponent odpowiada wyłącznie za własny
fragment systemu.


---

# Pełny kod klasy `SessionManagement`

Poniżej znajduje się pełna implementacja klasy z omawianego przykładu.

```cpp
class SessionManagement {
public:
    void connectToEngine(Engine& engine) {
        engine_ = &engine;

        appLogger().log("[Session] Łączenie z Engine\n");
        appLogger().log("[Session] Sprawdzenie konfiguracji\n");
        appLogger().log("[Session] Engine podłączony\n");

        // Engine automatycznie zapisuje się jako obserwator sesji
        attach(&engine);
    }

    void openSession() {
        if (!engine_) {
            appLogger().log("[Session] Brak Engine\n");
            return;
        }

        sessionActive_ = true;
        appLogger().log("[Session] Otwieram sesję\n");
        engine_->start();
    }

    void closeSession() {
        if (!engine_) {
            appLogger().log("[Session] Brak Engine\n");
            return;
        }

        appLogger().log("[Session] Zamykam sesję — powiadamiam wszystkich obserwatorów\n");
        sessionActive_ = false;

        // Każdy obserwator sam decyduje, co znaczy dla niego "sesja się kończy"
        notify(SessionEvent{SessionEventType::SessionClosing, {}, 0});

        // Sesja jest już martwa — nikt nie jest dłużej obserwowany
        observers_.clear();
    }

    void addVectorFromGui(const std::vector<int>& vec) {
        if (!checkSession()) {
            return;
        }

        appLogger().log("[Session] GUI chce dodać wektor\n");
        notify(SessionEvent{SessionEventType::VectorAdded, vec, 0});
    }

    void sortVectorFromGui(size_t index) {
        if (!checkSession()) {
            return;
        }

        appLogger().log("[Session] GUI chce sortować wektor\n");
        notify(SessionEvent{SessionEventType::SortRequested, {}, index});
    }

    void printDataFromGui() {
        if (!checkSession()) {
            return;
        }

        appLogger().log("[Session] GUI chce wypisać dane\n");
        notify(SessionEvent{SessionEventType::PrintRequested, {}, 0});
    }

    // BUILDER + odroczone wykonanie: sesja jedzie po kolei po paczce poleceń
    // i dla każdego z nich generuje normalny event (dokładnie tak jak przy
    // pojedynczych kliknięciach z GUI) — Observer nie widzi żadnej różnicy.
    void executeBatch(const CommandBatch& batch) {
        std::ostringstream header;
        header << "[Session] Otrzymano paczkę poleceń (" << batch.size() << ") — wykonuję po kolei\n";
        appLogger().log(header.str());

        for (const Command& command : batch) {
            switch (command.type) {
                case CommandType::AddVector:
                    addVectorFromGui(command.vectorData);
                    break;

                case CommandType::SortVector:
                    sortVectorFromGui(command.index);
                    break;

                case CommandType::PrintData:
                    printDataFromGui();
                    break;
            }
        }

        appLogger().log("[Session] Paczka poleceń wykonana w całości\n");
    }

    // Fasada nad Strategy — GUI/main nie musi znać klasy Engine ani żadnej
    // konkretnej klasy strategii. SessionManagement nie tworzy już strategii
    // (to robi teraz sam Engine, przez SortStrategyFactory) — tutaj tylko
    // sprawdzamy politykę Configuratora i rozsyłamy KOPIOWALNY SortStrategyId
    // przez ten sam mechanizm notify(), którym idą inne zdarzenia.
    void setSortStrategyFromGui(SortStrategyId id) {
        if (!engine_) {
            appLogger().log("[Session] Brak Engine\n");
            return;
        }

        if (!isStrategyAllowed(id)) {
            std::ostringstream oss;
            oss << "[Session] Strategia \"" << sortStrategyIdName(id)
                << "\" niedozwolona — Configurator jej nie autoryzował\n";
            appLogger().log(oss.str());
            return;
        }

        appLogger().log("[Session] GUI prosi o zmianę strategii sortowania\n");
        notify(SessionEvent{SessionEventType::StrategyChangeRequested, {}, 0, id});
    }

    // Configurator jako jedyny ustala, co wolno podmieniać w locie
    void setAllowedStrategies(std::vector<SortStrategyId> allowed) {
        allowedStrategies_ = std::move(allowed);
    }

    // ==================================
    // OBSERVER — zarządzanie subskrybentami
    // ==================================
    void attach(ISessionObserver* observer) {
        observers_.push_back(observer);
    }

    void detach(ISessionObserver* observer) {
        observers_.erase(
            std::remove(observers_.begin(), observers_.end(), observer),
            observers_.end());
    }

private:
    void notify(const SessionEvent& event) {
        for (ISessionObserver* observer : observers_) {
            observer->onSessionEvent(event);
        }
    }

    bool isStrategyAllowed(SortStrategyId id) const {
        return std::find(allowedStrategies_.begin(), allowedStrategies_.end(), id)
               != allowedStrategies_.end();
    }

    bool checkSession() const {
        if (!engine_) {
            appLogger().log("[Session] Brak Engine\n");
            return false;
        }

        if (!sessionActive_) {
            appLogger().log("[Session] Sesja nieaktywna\n");
            return false;
        }

        return true;
    }

private:
    Engine* engine_ = nullptr;
    bool sessionActive_ = false;
    std::vector<ISessionObserver*> observers_;
    std::vector<SortStrategyId> allowedStrategies_;
};
```
