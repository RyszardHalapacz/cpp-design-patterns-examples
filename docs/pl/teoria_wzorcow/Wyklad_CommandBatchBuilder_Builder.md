# Wykład: Builder na przykładzie `CommandBatchBuilder`

## Wprowadzenie

`CommandBatchBuilder` jest przykładem wzorca **Builder**.

Nie buduje pojedynczego złożonego obiektu, lecz **paczkę komend
(`CommandBatch`)**, która później zostaje przekazana do wykonania.

To praktyczny przykład Buildera spotykany w aplikacjach GUI, silnikach
oraz systemach kolejkowania poleceń.

------------------------------------------------------------------------

# Kod klasy

``` cpp
class CommandBatchBuilder {
public:
    CommandBatchBuilder& addVector(const std::vector<int>& vec) {
        commands_.push_back(Command{CommandType::AddVector, vec, 0});
        return *this;
    }

    CommandBatchBuilder& sortVector(size_t index) {
        commands_.push_back(Command{CommandType::SortVector, {}, index});
        return *this;
    }

    CommandBatchBuilder& printData() {
        commands_.push_back(Command{CommandType::PrintData, {}, 0});
        return *this;
    }

    CommandBatch build() {
        CommandBatch result = std::move(commands_);
        commands_.clear();
        return result;
    }

private:
    CommandBatch commands_;
};
```

------------------------------------------------------------------------

# Co buduje Builder?

Najpierw należy zrozumieć czym jest `CommandBatch`.

Najprościej można go przedstawić jako:

``` cpp
using CommandBatch = std::vector<Command>;
```

czyli listę poleceń.

    CommandBatch

    ├── Command
    ├── Command
    ├── Command
    └── Command

Builder nie wykonuje operacji.

Buduje jedynie opis operacji.

------------------------------------------------------------------------

# Pole klasy

``` cpp
CommandBatch commands_;
```

Jest to wewnętrzny bufor, do którego odkładane są kolejne komendy.

Na początku:

    []

Po:

``` cpp
builder.addVector(v);
```

    [ AddVector ]

Po:

``` cpp
builder.sortVector(3);
```

    [ AddVector, SortVector ]

Po:

``` cpp
builder.printData();
```

    [
        AddVector,
        SortVector,
        PrintData
    ]

------------------------------------------------------------------------

# Fluent Interface

Każda metoda zwraca:

``` cpp
return *this;
```

Dzięki temu można pisać:

``` cpp
builder
    .addVector(v1)
    .sortVector(0)
    .printData();
```

To właśnie tzw. **Fluent Interface**.

------------------------------------------------------------------------

# Metoda `addVector()`

Dodaje do listy opis operacji:

``` cpp
Command{
    CommandType::AddVector,
    vec,
    0
}
```

Nie wykonuje dodania wektora.

Jedynie zapisuje polecenie.

------------------------------------------------------------------------

# Metoda `sortVector()`

Dodaje:

``` cpp
Command{
    CommandType::SortVector,
    {},
    index
}
```

czyli informację:

    SortVector(index)

------------------------------------------------------------------------

# Metoda `printData()`

Dodaje komendę:

    PrintData

Nie drukuje danych.

Jedynie zapisuje polecenie.

------------------------------------------------------------------------

# Najważniejsza metoda --- `build()`

``` cpp
CommandBatch build() {
    CommandBatch result = std::move(commands_);
    commands_.clear();
    return result;
}
```

Przed wywołaniem:

    commands_

    [
     AddVector,
     SortVector,
     PrintData
    ]

Po:

``` cpp
auto batch = builder.build();
```

otrzymujemy:

    batch

    [
     AddVector,
     SortVector,
     PrintData
    ]

natomiast Builder jest ponownie pusty:

    commands_

    []

------------------------------------------------------------------------

# Dlaczego użyto `std::move()`?

``` cpp
CommandBatch result = std::move(commands_);
```

Dzięki temu nie kopiujemy całego `std::vector<Command>`.

Przenosimy jego zawartość do nowego obiektu, co jest znacznie
wydajniejsze.

------------------------------------------------------------------------

# Czy jest to klasyczny Builder GoF?

Nie całkiem.

Klasyczny Builder buduje zwykle pojedynczy złożony obiekt, np.:

    Builder

    ↓

    Car

Tutaj budowana jest kolekcja operacji:

    Builder

    ↓

    CommandBatch

    ↓

    lista poleceń

To bardzo praktyczna odmiana wzorca.

------------------------------------------------------------------------

# Zalety

-   czytelny interfejs,
-   możliwość łańcuchowego wywoływania metod,
-   oddzielenie procesu budowania od wykonania,
-   wydajne wykorzystanie `std::move`,
-   możliwość wielokrotnego wykorzystania Buildera.

------------------------------------------------------------------------

# Podsumowanie

`CommandBatchBuilder` pokazuje Buildera w praktycznej formie.

Zamiast budować samochód czy dom, buduje listę komend, która później
może zostać przekazana do wykonania przez inne elementy systemu.

Jest to przykład znacznie bliższy rzeczywistym aplikacjom niż klasyczne
przykłady z książek.
