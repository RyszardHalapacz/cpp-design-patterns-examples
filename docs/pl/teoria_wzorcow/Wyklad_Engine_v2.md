# Wykład: Klasa `Engine`

## Wprowadzenie

`Engine` jest jedną z najważniejszych klas w przykładzie. Łączy kilka
wzorców projektowych i odpowiada za wykonywanie operacji na danych.

Klasa:

-   przechowuje dane,
-   sortuje wektory,
-   korzysta ze wzorca Strategy,
-   korzysta z Factory,
-   jest obserwatorem (Observer),
-   loguje komunikaty przez Service Locator.

------------------------------------------------------------------------

# Dziedziczenie

``` cpp
class Engine : public ISessionObserver
```

`Engine` jest obserwatorem sesji i implementuje metodę:

``` cpp
void onSessionEvent(const SessionEvent& event) override;
```

Dzięki temu może reagować na zdarzenia wysyłane przez
`SessionManagement`.

------------------------------------------------------------------------

# Konstruktor

``` cpp
Engine()
    : sortStrategy_(
        SortStrategyFactory::create(
            SortStrategyId::Ascending))
{}
```

Po utworzeniu obiektu domyślną strategią jest `AscendingSortStrategy`.

------------------------------------------------------------------------

# Pola klasy

## running\_

``` cpp
bool running_ = false;
```

Przechowuje informację, czy silnik został uruchomiony.

Metody:

``` cpp
start();
stop();
```

zmieniają jego wartość.

------------------------------------------------------------------------

## data\_

``` cpp
std::vector<std::vector<int>> data_;
```

Przechowuje wszystkie wektory liczb całkowitych.

Przykład:

    [
        [5,2,8],
        [7,1,9],
        [4,6]
    ]

------------------------------------------------------------------------

## sortStrategy\_

``` cpp
std::unique_ptr<ISortStrategy> sortStrategy_;
```

Przechowuje aktualny algorytm sortowania.

Może wskazywać na:

-   `AscendingSortStrategy`
-   `DescendingSortStrategy`
-   `BubbleSortStrategy`

`Engine` zna jedynie interfejs `ISortStrategy`.

------------------------------------------------------------------------

# addVector()

``` cpp
void addVector(const std::vector<int>& vec)
```

Dodaje nowy wektor do kolekcji danych.

------------------------------------------------------------------------

# setSortStrategy()

``` cpp
void setSortStrategy(std::unique_ptr<ISortStrategy> strategy)
```

Podmienia aktualną strategię sortowania.

Najważniejsza instrukcja:

``` cpp
sortStrategy_ = std::move(strategy);
```

`std::unique_ptr` może być jedynie przenoszony, dlatego używany jest
`std::move`.

------------------------------------------------------------------------

# sortVector()

``` cpp
void sortVector(size_t index)
```

1.  Sprawdza poprawność indeksu.
2.  Loguje operację.
3.  Wywołuje aktualną strategię:

``` cpp
(*sortStrategy_)(data_[index]);
```

To właśnie tutaj realizowany jest wzorzec **Strategy**.

------------------------------------------------------------------------

# printData()

``` cpp
void printData() const
```

Buduje tekst zawierający wszystkie dane i przekazuje go do loggera.

Metoda nie zmienia stanu obiektu, dlatego została oznaczona jako
`const`.

------------------------------------------------------------------------

# onSessionEvent()

Najważniejsza metoda klasy.

Na podstawie typu zdarzenia wykonuje odpowiednią operację:

-   `VectorAdded` → `addVector()`
-   `SortRequested` → `sortVector()`
-   `PrintRequested` → `printData()`
-   `StrategyChangeRequested` → tworzy nową strategię przez
    `SortStrategyFactory`
-   `SessionClosing` → `stop()`

To właśnie tutaj współpracują wzorce **Observer**, **Factory** i
**Strategy**.

------------------------------------------------------------------------

# Wzorce wykorzystane przez Engine

## Strategy

``` cpp
std::unique_ptr<ISortStrategy>
```

Algorytm sortowania może być zmieniany podczas działania programu.

------------------------------------------------------------------------

## Factory

``` cpp
SortStrategyFactory::create(...)
```

Tworzy odpowiednią strategię sortowania.

------------------------------------------------------------------------

## Observer

``` cpp
class Engine : public ISessionObserver
```

`Engine` reaguje na zdarzenia wysyłane przez `SessionManagement`.

------------------------------------------------------------------------

## Service Locator

Logger pobierany jest przez:

``` cpp
appLogger().log(...);
```

Klasa nie przechowuje własnego loggera.

------------------------------------------------------------------------

# Schemat współpracy

    DummyGui
        │
        ▼
    SessionManagement
        │
        ▼
    Engine::onSessionEvent()
        │
        ▼
    sortVector()
        │
        ▼
    ISortStrategy
        │
        ▼
    Ascending / Descending / Bubble

------------------------------------------------------------------------

# Odpowiedzialności klasy

-   przechowywanie danych,
-   dodawanie wektorów,
-   sortowanie,
-   drukowanie danych,
-   reagowanie na zdarzenia,
-   utrzymywanie aktualnej strategii sortowania.

------------------------------------------------------------------------

# Podsumowanie

`Engine` jest centralnym elementem przykładu.

To właśnie w tej klasie spotykają się:

-   Strategy,
-   Factory,
-   Observer,
-   Service Locator.

Dzięki temu stanowi bardzo dobry przykład pokazujący, jak kilka wzorców
projektowych może współpracować w jednej klasie.

------------------------------------------------------------------------

# Pełny kod klasy `Engine`

Poniżej znajduje się pełny kod klasy `Engine` z przykładu omawianego w
tym wykładzie.

``` cpp
class Engine : public ISessionObserver {
public:
    Engine()
        : sortStrategy_(
            SortStrategyFactory::create(
                SortStrategyId::Ascending
            )
        )
    {}

    void start() {
        running_ = true;
        appLogger().log("[Engine] Start\n");
    }

    void stop() {
        running_ = false;
        appLogger().log("[Engine] Stop\n");
    }

    void addVector(const std::vector<int>& vec) {
        data_.push_back(vec);
    }

    void setSortStrategy(std::unique_ptr<ISortStrategy> strategy) {
        sortStrategy_ = std::move(strategy);

        std::ostringstream oss;
        oss << "[Engine] Ustawiono strategię sortowania: "
            << sortStrategy_->name() << "\n";

        appLogger().log(oss.str());
    }

    void sortVector(size_t index) {
        if (index >= data_.size()) {
            appLogger().log("[Engine] Niepoprawny indeks wektora\n");
            return;
        }

        std::ostringstream oss;
        oss << "[Engine] Sortuję strategią: "
            << sortStrategy_->name() << "\n";

        appLogger().log(oss.str());

        (*sortStrategy_)(data_[index]);
    }

    void printData() const {
        std::ostringstream oss;
        oss << "[Engine] Dane:\n";

        for (size_t i = 0; i < data_.size(); ++i) {
            oss << "  [" << i << "]: ";

            for (int value : data_[i]) {
                oss << value << " ";
            }

            oss << "\n";
        }

        appLogger().log(oss.str());
    }

    void onSessionEvent(const SessionEvent& event) override {
        appLogger().log("[Engine] Otrzymano powiadomienie\n");

        switch (event.type) {
        case SessionEventType::VectorAdded:
            addVector(event.vectorData);
            break;

        case SessionEventType::SortRequested:
            sortVector(event.index);
            break;

        case SessionEventType::PrintRequested:
            printData();
            break;

        case SessionEventType::StrategyChangeRequested:
            setSortStrategy(
                SortStrategyFactory::create(event.strategyId)
            );
            break;

        case SessionEventType::SessionClosing:
            stop();
            break;
        }
    }

private:
    bool running_ = false;
    std::vector<std::vector<int>> data_;
    std::unique_ptr<ISortStrategy> sortStrategy_;
};
```
