
# Wykład: Klasa `DummyGui`

## Wprowadzenie

`DummyGui` jest uproszczoną, dydaktyczną imitacją warstwy GUI.

Nie korzysta z Qt, ImGui ani żadnej innej biblioteki okienkowej. Zamiast prawdziwych przycisków ma metody takie jak:

```cpp
clickAddVector(...);
clickSortVector(...);
clickPrintData();
clickSetSortStrategy(...);
```

Metody te symulują działania użytkownika.

Najważniejszą cechą klasy jest to, że `DummyGui` nie wykonuje logiki biznesowej. Nie sortuje danych, nie przechowuje wektorów i nie tworzy strategii. Jedynie przekazuje żądania do `SessionManagement`.

---

# Najważniejszy mechanizm: puste wskaźniki na początku

Na samym początku `DummyGui` nie ma dostępu do żadnej operacji `SessionManagement`.

Przechowuje:

```cpp
SessionManagement* session_ = nullptr;
```

oraz kilka wskaźników do metod składowych:

```cpp
AddVectorFunc addVectorFunc_ = nullptr;
SortVectorFunc sortVectorFunc_ = nullptr;
PrintDataFunc printDataFunc_ = nullptr;
ExecuteBatchFunc executeBatchFunc_ = nullptr;
SetSortStrategyFunc setSortStrategyFunc_ = nullptr;
```

Są to początkowo puste, surowe wskaźniki.

Trzeba jednak precyzyjnie zaznaczyć, że nie są to zwykłe wskaźniki do wolnych funkcji. Są to **wskaźniki do metod składowych klasy `SessionManagement`**.

Przykład typu:

```cpp
using AddVectorFunc =
    void (SessionManagement::*)(const std::vector<int>&);
```

Oznacza:

> wskaźnik do metody klasy `SessionManagement`, która przyjmuje `const std::vector<int>&` i zwraca `void`.

Sam wskaźnik do metody nie wystarcza. Do wywołania potrzebny jest jeszcze konkretny obiekt `SessionManagement`.

Dlatego `DummyGui` przechowuje dwie rzeczy:

```text
adres obiektu SessionManagement
+
adres wybranej metody SessionManagement
```

Dopiero połączenie tych dwóch elementów pozwala wykonać operację.

---

# Kto przypisuje prawdziwe funkcje?

`DummyGui` nie wybiera sam, do jakich metod ma dostęp.

Robi to inna klasa:

```cpp
Configurator
```

Przykład:

```cpp
gui.connectAddVector(
    &session,
    &SessionManagement::addVectorFromGui
);
```

`Configurator` przekazuje:

1. adres konkretnego obiektu `SessionManagement`,
2. wskaźnik do konkretnej metody tego obiektu.

Analogicznie podpina pozostałe funkcje:

```cpp
gui.connectSortVector(
    &session,
    &SessionManagement::sortVectorFromGui
);

gui.connectPrintData(
    &session,
    &SessionManagement::printDataFromGui
);

gui.connectExecuteBatch(
    &session,
    &SessionManagement::executeBatch
);

gui.connectSetSortStrategy(
    &session,
    &SessionManagement::setSortStrategyFromGui
);
```

Dzięki temu `Configurator` decyduje, co wolno klasie `DummyGui`.

To nie jest tylko zwykłe „okablowanie”. Jest to także prosty mechanizm nadawania uprawnień.

`DummyGui` może wywołać tylko te operacje, które zostały mu jawnie przypisane.

---

# Stan przed konfiguracją

Bez wywołania `Configurator::configureGui()` obiekt wygląda logicznie tak:

```text
DummyGui
├── session_ = nullptr
├── addVectorFunc_ = nullptr
├── sortVectorFunc_ = nullptr
├── printDataFunc_ = nullptr
├── executeBatchFunc_ = nullptr
└── setSortStrategyFunc_ = nullptr
```

W takim stanie próba kliknięcia operacji nie powoduje błędu pamięci.

Metoda najpierw sprawdza dostęp:

```cpp
if (!session_ || !addVectorFunc_) {
    appLogger().log("[GUI] Brak dostępu do AddVector\n");
    return;
}
```

Dzięki temu klasa bezpiecznie odmawia wykonania operacji.

---

# Stan po konfiguracji

Po wykonaniu:

```cpp
configurator.configureGui(gui, session);
```

obiekt wygląda logicznie tak:

```text
DummyGui
├── session_ --------------------------> SessionManagement
├── addVectorFunc_ --------------------> addVectorFromGui
├── sortVectorFunc_ -------------------> sortVectorFromGui
├── printDataFunc_ --------------------> printDataFromGui
├── executeBatchFunc_ -----------------> executeBatch
└── setSortStrategyFunc_ --------------> setSortStrategyFromGui
```

Od tej chwili metody `click...()` mogą przekazywać żądania do sesji.

---

# Typy wskaźników do metod

Klasa definiuje aliasy:

```cpp
using AddVectorFunc =
    void (SessionManagement::*)(const std::vector<int>&);

using SortVectorFunc =
    void (SessionManagement::*)(size_t);

using PrintDataFunc =
    void (SessionManagement::*)();

using ExecuteBatchFunc =
    void (SessionManagement::*)(const CommandBatch&);

using SetSortStrategyFunc =
    void (SessionManagement::*)(SortStrategyId);
```

Każdy alias opisuje dokładną sygnaturę metody, którą można podpiąć.

Przykładowo do `AddVectorFunc` nie da się przypisać metody przyjmującej `size_t`, ponieważ jej typ jest inny.

Kompilator pilnuje więc zgodności typów.

---

# Metody `connect...()`

Każda metoda połączeniowa zapisuje:

- adres sesji,
- wskaźnik do wybranej metody.

Przykład:

```cpp
void connectAddVector(
    SessionManagement* session,
    AddVectorFunc func
) {
    session_ = session;
    addVectorFunc_ = func;
}
```

Ta metoda nie wykonuje dodawania wektora. Jedynie przygotowuje połączenie, które później wykorzysta `clickAddVector()`.

---

# Wywoływanie wskaźnika do metody

Najważniejsza składnia wygląda tak:

```cpp
(session_->*addVectorFunc_)(vec);
```

Można ją czytać następująco:

1. `session_` — wybierz konkretny obiekt `SessionManagement`,
2. `addVectorFunc_` — wybierz przypisaną metodę,
3. `->*` — połącz wskaźnik do obiektu ze wskaźnikiem do metody,
4. `(vec)` — wywołaj metodę z argumentem.

To nie jest zwykłe wywołanie przez wskaźnik do funkcji. Operator `->*` jest specjalnie przeznaczony do wskaźników do metod składowych.

---

# `clickAddVector()`

```cpp
void clickAddVector(const std::vector<int>& vec)
```

Metoda symuluje kliknięcie przycisku dodawania danych.

Najpierw sprawdza, czy:

- istnieje podłączona sesja,
- przypisano metodę dodawania wektora.

Następnie wywołuje przypisaną funkcję:

```cpp
(session_->*addVectorFunc_)(vec);
```

Pełny przepływ:

```text
Użytkownik
    │
    ▼
DummyGui::clickAddVector()
    │
    ▼
SessionManagement::addVectorFromGui()
    │
    ▼
notify(VectorAdded)
    │
    ▼
Engine::onSessionEvent()
    │
    ▼
Engine::addVector()
```

---

# `clickSortVector()`

```cpp
void clickSortVector(size_t index)
```

Symuluje kliknięcie przycisku sortowania.

Nie sortuje bezpośrednio. Wywołuje metodę przypisaną przez `Configurator`:

```cpp
(session_->*sortVectorFunc_)(index);
```

---

# `clickPrintData()`

```cpp
void clickPrintData()
```

Przekazuje żądanie wypisania danych do `SessionManagement`.

```cpp
(session_->*printDataFunc_)();
```

---

# `clickSetSortStrategy()`

```cpp
void clickSetSortStrategy(SortStrategyId id)
```

Przekazuje żądanie zmiany strategii.

`DummyGui` nie tworzy strategii. Nie korzysta bezpośrednio z `SortStrategyFactory`.

Przepływ wygląda tak:

```text
DummyGui
    │
    ▼
SessionManagement
    │
    ▼
Engine
    │
    ▼
SortStrategyFactory
    │
    ▼
ISortStrategy
```

---

# Współpraca z Builderem

`DummyGui` posiada także:

```cpp
CommandBatchBuilder batchBuilder_;
```

Dzięki temu może nie wykonywać operacji natychmiast, lecz zbierać je w paczkę.

## `queueAddVector()`

```cpp
DummyGui& queueAddVector(const std::vector<int>& vec)
```

Dodaje polecenie do buildera i zwraca `*this`.

Pozwala to budować płynny interfejs:

```cpp
gui
    .queueAddVector({3, 1, 2})
    .queueSortVector(0)
    .queuePrintData();
```

## `queueSortVector()`

Dodaje polecenie sortowania do paczki.

## `queuePrintData()`

Dodaje polecenie wypisania danych.

## `buildBatch()`

```cpp
CommandBatch buildBatch()
```

Kończy budowanie paczki i zwraca gotowy `CommandBatch`.

## `flushBatch()`

```cpp
void flushBatch()
```

Buduje paczkę, a następnie przekazuje ją do metody `SessionManagement::executeBatch()`.

Również tutaj operacja zadziała tylko wtedy, gdy `Configurator` przyznał GUI dostęp do `ExecuteBatch`.

---

# Czy `DummyGui` jest prawdziwym GUI?

Nie.

Jest atrapą warstwy prezentacji używaną do celów dydaktycznych i testowych.

Prawdziwy interfejs mógłby zostać wykonany w:

- Qt,
- ImGui,
- aplikacji konsolowej,
- interfejsie webowym,
- REST API.

Ważne jest to, że każda z tych warstw mogłaby korzystać z podobnego API `SessionManagement`.

---

# Czy `DummyGui` jest dobrze zaprojektowane?

Dydaktycznie mechanizm jest interesujący, ponieważ pokazuje:

- wskaźniki do metod składowych,
- ręczne okablowanie,
- kontrolę dostępnych operacji,
- odseparowanie GUI od `Engine`,
- współpracę z Builderem.

Trzeba jednak zaznaczyć, że jest to rozwiązanie dość sztuczne i niskopoziomowe.

W produkcyjnym GUI częściej zastosowano by:

- jawny interfejs fasady,
- referencję lub wskaźnik do tego interfejsu,
- `std::function`,
- system sygnałów i slotów,
- komendy,
- dependency injection.

Tutaj surowe wskaźniki do metod są użyte celowo, aby pokazać, że zakres możliwości GUI może zostać nadany dopiero podczas konfiguracji.

---

# Jakie wzorce wykorzystuje?

`DummyGui` nie implementuje bezpośrednio jednego klasycznego wzorca GoF, ale współpracuje z kilkoma wzorcami:

- **Facade** — komunikuje się z `SessionManagement`,
- **Builder** — tworzy `CommandBatch`,
- **Observer** — jego żądania są później zamieniane na zdarzenia,
- **Strategy** — pośrednio żąda zmiany strategii,
- **Factory** — pośrednio powoduje utworzenie strategii,
- **Dependency Injection / ręczne okablowanie** — `Configurator` przypisuje mu dostępne operacje.

---

# Najważniejsza zaleta

`DummyGui` nie zna `Engine` i nie wykonuje logiki biznesowej.

Wie jedynie, jakie operacje zostały mu udostępnione podczas konfiguracji.

Dzięki temu:

- GUI jest oddzielone od silnika,
- logika biznesowa nie trafia do warstwy prezentacji,
- można ograniczać dostępne operacje,
- łatwiej wymienić warstwę GUI.

---

# Pełny przepływ

```text
             Configurator
                  │
                  │ przypisuje obiekt sesji
                  │ i wskaźniki do metod
                  ▼
              DummyGui
                  │
             click...()
                  │
                  ▼
          SessionManagement
                  │
             notify(event)
                  │
                  ▼
               Engine
                  │
                  ▼
              Strategy
```

---

# Podsumowanie

`DummyGui` jest uproszczoną warstwą prezentacji.

Na początku przechowuje pusty wskaźnik do `SessionManagement` oraz puste wskaźniki do jego metod. Dopiero `Configurator` przypisuje konkretne funkcje, z których GUI może korzystać.

Dzięki temu GUI wie wyłącznie, na co pozwoliła mu konfiguracja. Nie zna `Engine`, nie tworzy strategii i nie wykonuje logiki biznesowej.

Jest to rozwiązanie celowo dydaktyczne: pokazuje ręczne okablowanie, wskaźniki do metod składowych, delegowanie operacji oraz odseparowanie warstwy prezentacji od reszty aplikacji.

---

# Pełny kod klasy `DummyGui`

```cpp
class DummyGui {
public:
    using AddVectorFunc = void (SessionManagement::*)(const std::vector<int>&);
    using SortVectorFunc = void (SessionManagement::*)(size_t);
    using PrintDataFunc = void (SessionManagement::*)();
    using ExecuteBatchFunc = void (SessionManagement::*)(const CommandBatch&);
    using SetSortStrategyFunc = void (SessionManagement::*)(SortStrategyId);

    void connectAddVector(SessionManagement* session, AddVectorFunc func) {
        session_ = session;
        addVectorFunc_ = func;
    }

    void connectSortVector(SessionManagement* session, SortVectorFunc func) {
        session_ = session;
        sortVectorFunc_ = func;
    }

    void connectPrintData(SessionManagement* session, PrintDataFunc func) {
        session_ = session;
        printDataFunc_ = func;
    }

    void connectExecuteBatch(SessionManagement* session, ExecuteBatchFunc func) {
        session_ = session;
        executeBatchFunc_ = func;
    }

    void connectSetSortStrategy(SessionManagement* session, SetSortStrategyFunc func) {
        session_ = session;
        setSortStrategyFunc_ = func;
    }

    void clickAddVector(const std::vector<int>& vec) {
        if (!session_ || !addVectorFunc_) {
            appLogger().log("[GUI] Brak dostępu do AddVector\n");
            return;
        }

        appLogger().log("[GUI] Kliknięto AddVector\n");
        (session_->*addVectorFunc_)(vec);
    }

    void clickSortVector(size_t index) {
        if (!session_ || !sortVectorFunc_) {
            appLogger().log("[GUI] Brak dostępu do SortVector\n");
            return;
        }

        appLogger().log("[GUI] Kliknięto SortVector\n");
        (session_->*sortVectorFunc_)(index);
    }

    void clickPrintData() {
        if (!session_ || !printDataFunc_) {
            appLogger().log("[GUI] Brak dostępu do PrintData\n");
            return;
        }

        appLogger().log("[GUI] Kliknięto PrintData\n");
        (session_->*printDataFunc_)();
    }

    void clickSetSortStrategy(SortStrategyId id) {
        if (!session_ || !setSortStrategyFunc_) {
            appLogger().log("[GUI] Brak dostępu do SetSortStrategy\n");
            return;
        }

        appLogger().log("[GUI] Kliknięto SetSortStrategy\n");
        (session_->*setSortStrategyFunc_)(id);
    }

    // ==================================
    // BUILDER — GUI zbiera polecenia zamiast wysyłać je natychmiast
    // ==================================
    DummyGui& queueAddVector(const std::vector<int>& vec) {
        appLogger().log("[GUI] Dodaję AddVector do paczki poleceń\n");
        batchBuilder_.addVector(vec);
        return *this;
    }

    DummyGui& queueSortVector(size_t index) {
        appLogger().log("[GUI] Dodaję SortVector do paczki poleceń\n");
        batchBuilder_.sortVector(index);
        return *this;
    }

    DummyGui& queuePrintData() {
        appLogger().log("[GUI] Dodaję PrintData do paczki poleceń\n");
        batchBuilder_.printData();
        return *this;
    }

    CommandBatch buildBatch() {
        appLogger().log("[GUI] Zamykam paczkę poleceń, gotowa do wysłania\n");
        return batchBuilder_.build();
    }

    // Wysyła zbudowaną paczkę do sesji — tylko jeśli Configurator przyznał dostęp
    void flushBatch() {
        if (!session_ || !executeBatchFunc_) {
            appLogger().log("[GUI] Brak dostępu do ExecuteBatch\n");
            return;
        }

        CommandBatch batch = buildBatch();
        appLogger().log("[GUI] Wysyłam paczkę poleceń do sesji\n");
        (session_->*executeBatchFunc_)(batch);
    }

private:
    SessionManagement* session_ = nullptr;

    AddVectorFunc addVectorFunc_ = nullptr;
    SortVectorFunc sortVectorFunc_ = nullptr;
    PrintDataFunc printDataFunc_ = nullptr;
    ExecuteBatchFunc executeBatchFunc_ = nullptr;
    SetSortStrategyFunc setSortStrategyFunc_ = nullptr;

    CommandBatchBuilder batchBuilder_;
};
```

---

# Kod `Configurator` odpowiedzialny za podłączenie funkcji

Poniższy kod pokazuje klasę, która przypisuje `DummyGui` prawdziwe metody `SessionManagement`.

```cpp
class Configurator {
public:
    void configureGui(DummyGui& gui, SessionManagement& session) {
        appLogger().log("[Configurator] Przyznaję GUI dostęp do wybranych funkcji\n");

        gui.connectAddVector(&session, &SessionManagement::addVectorFromGui);
        gui.connectSortVector(&session, &SessionManagement::sortVectorFromGui);
        gui.connectPrintData(&session, &SessionManagement::printDataFromGui);
        gui.connectExecuteBatch(&session, &SessionManagement::executeBatch);
        gui.connectSetSortStrategy(&session, &SessionManagement::setSortStrategyFromGui);
    }

    // Configurator jako jedyne miejsce, które ustala politykę: jakie strategie
    // sortowania w ogóle wolno podmieniać w trakcie działania programu
    void configureAllowedStrategies(SessionManagement& session,
                                     std::vector<SortStrategyId> allowed) {
        appLogger().log("[Configurator] Ustalam dozwolone strategie sortowania\n");
        session.setAllowedStrategies(std::move(allowed));
    }
};
```
