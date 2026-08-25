# Wykład: Factory na przykładzie `SortStrategyFactory`

## Wprowadzenie

`SortStrategyFactory` jest implementacją wzorca **Factory**, a
dokładniej **Simple Factory**.

Jej jedynym zadaniem jest tworzenie odpowiedniej strategii sortowania na
podstawie przekazanego identyfikatora. Nie sortuje danych i nie
przechowuje żadnego stanu.

------------------------------------------------------------------------

# Problem

Bez Factory klasa `Engine` musiałaby sama decydować, jaki obiekt
utworzyć:

``` cpp
if (strategy == Ascending)
    strategy_ = std::make_unique<AscendingSortStrategy>();
else if (strategy == Descending)
    strategy_ = std::make_unique<DescendingSortStrategy>();
else if (strategy == Bubble)
    strategy_ = std::make_unique<BubbleSortStrategy>();
```

Każda nowa strategia oznaczałaby modyfikację klasy `Engine`.

------------------------------------------------------------------------

# Pomysł Factory

Tworzenie obiektów zostaje przeniesione do osobnej klasy.

    Engine
        │
        ▼
    SortStrategyFactory
        │
        ▼
    Tworzy odpowiednią strategię

Od tej chwili `Engine` nie wie, jak tworzy się konkretna strategia.

------------------------------------------------------------------------

# Kod klasy

``` cpp
class SortStrategyFactory {
public:
    static std::unique_ptr<ISortStrategy>
    create(SortStrategyId id)
    {
        switch(id)
        {
        case SortStrategyId::Ascending:
            return std::make_unique<AscendingSortStrategy>();

        case SortStrategyId::Descending:
            return std::make_unique<DescendingSortStrategy>();

        case SortStrategyId::Bubble:
            return std::make_unique<BubbleSortStrategy>();
        }

        throw std::runtime_error("Unknown strategy");
    }
};
```

------------------------------------------------------------------------

# Co zwraca Factory?

Factory zwraca:

``` cpp
std::unique_ptr<ISortStrategy>
```

Nie zwraca konkretnej klasy (`AscendingSortStrategy`,
`BubbleSortStrategy` itd.), lecz wskaźnik do wspólnego interfejsu.

Dzięki temu kod korzystający z Factory nie musi znać konkretnej
implementacji.

------------------------------------------------------------------------

# Jak działa?

Przykład:

``` cpp
auto strategy =
    SortStrategyFactory::create(
        SortStrategyId::Bubble);
```

Factory:

1.  sprawdza identyfikator,
2.  tworzy odpowiedni obiekt,
3.  zwraca `std::unique_ptr<ISortStrategy>`.

------------------------------------------------------------------------

# Gdzie jest używana?

Najważniejsze miejsce to klasa `Engine`.

Domyślna strategia:

``` cpp
sortStrategy_ =
    SortStrategyFactory::create(
        SortStrategyId::Ascending);
```

Zmiana strategii:

``` cpp
sortStrategy_ =
    SortStrategyFactory::create(id);
```

`Engine` nigdy sam nie tworzy konkretnych strategii.

------------------------------------------------------------------------

# Dlaczego metoda jest `static`?

Factory nie przechowuje żadnych pól ani stanu.

Nie ma potrzeby tworzenia obiektu:

``` cpp
SortStrategyFactory factory;
```

Wystarczy wywołać:

``` cpp
SortStrategyFactory::create(...);
```

------------------------------------------------------------------------

# Dlaczego `std::unique_ptr`?

Factory tworzy nowy obiekt.

Właścicielem obiektu staje się `Engine`, który automatycznie zwolni
pamięć po zmianie strategii lub zniszczeniu obiektu.

Nie ma ryzyka wycieku pamięci.

------------------------------------------------------------------------

# Zalety

-   Oddzielenie logiki tworzenia od logiki działania.
-   `Engine` nie zna klas konkretnych strategii.
-   Łatwiejsza rozbudowa kodu.
-   Automatyczne zarządzanie pamięcią przez `std::unique_ptr`.

------------------------------------------------------------------------

# Wady

Factory nadal zna wszystkie klasy strategii.

Dodanie nowej strategii wymaga modyfikacji Factory, dlatego jest to
**Simple Factory**, a nie bardziej rozbudowane odmiany, takie jak
Factory Method czy Abstract Factory.

------------------------------------------------------------------------

# Diagram

                    Engine
                       │
                       ▼
            SortStrategyFactory
              │      │       │
              ▼      ▼       ▼
         Ascending Descending Bubble
              │
              ▼
    unique_ptr<ISortStrategy>
              │
              ▼
            Engine

------------------------------------------------------------------------

# Podsumowanie

`SortStrategyFactory` odpowiada wyłącznie za tworzenie odpowiedniej
strategii sortowania.

Dzięki temu `Engine` skupia się na wykonywaniu sortowania, a nie na
podejmowaniu decyzji, jaki obiekt należy utworzyć. Jest to prosty,
czytelny i bardzo często spotykany przykład wzorca **Simple Factory**.
