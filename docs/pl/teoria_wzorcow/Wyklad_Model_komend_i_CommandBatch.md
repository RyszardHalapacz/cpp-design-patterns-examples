# Wykład: Model komend i `CommandBatch`

# Wprowadzenie

W projekcie zastosowano prosty model reprezentowania operacji w postaci
komend.

Zamiast wykonywać operacje natychmiast po kliknięciu przycisku,
aplikacja może najpierw zapisać ich opis, a dopiero później wykonać je
we właściwej kolejności.

Model składa się z trzech podstawowych elementów:

-   `CommandType`
-   `Command`
-   `CommandBatch`

oraz współpracuje z:

-   `CommandBatchBuilder`
-   `SessionManagement`

------------------------------------------------------------------------

# Dlaczego potrzebujemy modelu komend?

Użytkownik może wykonać kilka operacji:

1.  dodać wektor,
2.  posortować go,
3.  wyświetlić dane.

Można wykonać je od razu:

``` text
GUI
 │
 ▼
SessionManagement
```

lub najpierw zbudować ich opis:

``` text
GUI
 │
 ▼
CommandBatchBuilder
 │
 ▼
CommandBatch
 │
 ▼
SessionManagement
```

Dzięki temu operacje mogą zostać wykonane później.

------------------------------------------------------------------------

# `CommandType`

Przykład:

``` cpp
enum class CommandType
{
    AddVector,
    SortVector,
    PrintData
};
```

`CommandType` określa wyłącznie **rodzaj operacji**.

------------------------------------------------------------------------

# `Command`

Przykładowa struktura:

``` cpp
struct Command
{
    CommandType type;
    std::vector<int> vectorData;
    size_t index = 0;
};
```

Przykład:

``` text
Command
├── type = AddVector
├── vectorData = {5,2,8}
└── index = 0
```

lub

``` text
Command
├── type = SortVector
├── vectorData = {}
└── index = 2
```

------------------------------------------------------------------------

# `CommandBatch`

Najczęściej:

``` cpp
using CommandBatch = std::vector<Command>;
```

Jest to lista komend.

``` text
CommandBatch

├── AddVector
├── SortVector
└── PrintData
```

------------------------------------------------------------------------

# Budowanie paczki

Za tworzenie paczki odpowiada:

``` cpp
CommandBatchBuilder
```

Przykład:

``` cpp
CommandBatchBuilder builder;

builder
    .addVector({5,2,8})
    .sortVector(0)
    .printData();

CommandBatch batch = builder.build();
```

Builder nie wykonuje operacji.

Buduje jedynie ich opis.

------------------------------------------------------------------------

# Wykonanie paczki

Paczka trafia do:

``` cpp
SessionManagement::executeBatch(...)
```

Ta metoda wykonuje:

``` cpp
for(const Command& command : batch)
{
    ...
}
```

Każda komenda zamieniana jest na odpowiednią operację:

``` text
AddVector
    │
    ▼
addVectorFromGui()

SortVector
    │
    ▼
sortVectorFromGui()

PrintData
    │
    ▼
printDataFromGui()
```

------------------------------------------------------------------------

# Przepływ danych

``` text
Użytkownik
      │
      ▼
 DummyGui
      │
      ▼
CommandBatchBuilder
      │
      ▼
 CommandBatch
      │
      ▼
SessionManagement
      │
      ▼
 notify(...)
      │
      ▼
   Engine
```

------------------------------------------------------------------------

# Zależności

``` text
CommandType
      │
      ▼
   Command
      │
      ▼
 CommandBatch
      ▲
      │
CommandBatchBuilder
      │
      ▼
SessionManagement
```

------------------------------------------------------------------------

# Zalety

-   odroczone wykonywanie operacji,
-   możliwość kolejkowania poleceń,
-   łatwe rozszerzanie o nowe komendy,
-   oddzielenie budowania od wykonywania,
-   dobra współpraca ze wzorcami Builder i Command.

------------------------------------------------------------------------

# Podsumowanie

`CommandType` określa rodzaj operacji.

`Command` przechowuje dane pojedynczej operacji.

`CommandBatch` jest listą komend.

`CommandBatchBuilder` buduje paczkę komend, natomiast
`SessionManagement` interpretuje i wykonuje zapisane polecenia.
