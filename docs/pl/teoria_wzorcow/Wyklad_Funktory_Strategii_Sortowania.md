# Wykład -- Funktory strategii sortowania

## Cel

W projekcie wzorzec **Strategy** składa się z dwóch części:

-   interfejsu `ISortStrategy`,
-   konkretnych strategii będących **funktorami**.

Każda strategia implementuje operator `operator()`, dzięki czemu obiekt
można wywołać jak funkcję.

Przykładowo:

``` cpp
strategy(data);
```

Nie wywołujemy metody `sort()`, lecz operator `()`.

------------------------------------------------------------------------

# Dlaczego funktor?

Funktor to klasa przeciążająca operator wywołania:

``` cpp
class Example
{
public:
    void operator()(std::vector<int>& data)
    {
        // wykonaj operację
    }
};
```

Dzięki temu obiekt zachowuje się jak funkcja:

``` cpp
Example example;

example(data);
```

To bardzo często spotykany idiom w C++.

------------------------------------------------------------------------

# AscendingSortStrategy

Strategia odpowiedzialna za sortowanie rosnące.

Jej jedyną odpowiedzialnością jest uporządkowanie danych od
najmniejszego do największego.

Schemat:

``` cpp
class AscendingSortStrategy : public ISortStrategy
{
public:
    void operator()(std::vector<int>& data) override
    {
        std::sort(data.begin(), data.end());
    }
};
```

------------------------------------------------------------------------

# DescendingSortStrategy

Analogicznie działa strategia malejąca.

Zmienia jedynie sposób porównywania elementów.

Przykład:

``` cpp
class DescendingSortStrategy : public ISortStrategy
{
public:
    void operator()(std::vector<int>& data) override
    {
        std::sort(data.begin(), data.end(), std::greater<int>());
    }
};
```

Silnik nie wie, że używana jest inna strategia -- wywołuje jedynie
`operator()`.

------------------------------------------------------------------------

# BubbleSortStrategy

Ta strategia realizuje sortowanie bąbelkowe.

Najczęściej stosowana jest tutaj wyłącznie w celach dydaktycznych.

Schemat:

``` cpp
class BubbleSortStrategy : public ISortStrategy
{
public:
    void operator()(std::vector<int>& data) override
    {
        // implementacja Bubble Sort
    }
};
```

Algorytm różni się od poprzednich, ale interfejs pozostaje identyczny.

------------------------------------------------------------------------

# Co widzi Engine?

Engine posiada wskaźnik do interfejsu:

``` cpp
ISortStrategy* strategy_;
```

Podczas sortowania wykonuje tylko:

``` cpp
(*strategy_)(vector);
```

Nie interesuje go:

-   jaka klasa została ustawiona,
-   jaki algorytm sortowania zostanie wykonany,
-   ile trwa sortowanie.

Wie jedynie, że może wywołać `operator()`.

------------------------------------------------------------------------

# Zalety użycia funktorów

-   wspólny interfejs dla wszystkich algorytmów,
-   możliwość łatwego dodawania nowych strategii,
-   brak instrukcji `if` lub `switch` wybierających algorytm,
-   zgodność z ideą Open/Closed Principle,
-   bardzo naturalna integracja z wzorcem Strategy.

------------------------------------------------------------------------

# Podsumowanie

W projekcie każda konkretna strategia jest osobnym funktorem.

Każdy funktor:

-   dziedziczy po `ISortStrategy`,
-   implementuje `operator()`,
-   realizuje jeden konkretny algorytm sortowania.

Dzięki temu `Engine` może korzystać z dowolnej strategii bez znajomości
jej implementacji, co jest klasycznym przykładem zastosowania wzorca
**Strategy** w C++.
