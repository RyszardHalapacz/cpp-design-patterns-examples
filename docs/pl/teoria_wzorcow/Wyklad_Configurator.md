# Wykład: Klasa `Configurator`

## Wprowadzenie

`Configurator` jest niewielką klasą, ale pełni bardzo ważną rolę w
architekturze aplikacji.

Nie wykonuje logiki biznesowej. Nie sortuje danych, nie zarządza sesją i
nie przechowuje danych. Jego jedynym zadaniem jest **połączenie
istniejących obiektów oraz skonfigurowanie ich współpracy**.

Można powiedzieć, że odpowiada za **okablowanie (wiring)** aplikacji.

------------------------------------------------------------------------

# Najważniejsze zadania

`Configurator` odpowiada za:

-   połączenie `DummyGui` z `SessionManagement`,
-   przyznanie GUI dostępu do wybranych operacji,
-   ustawienie dozwolonych strategii sortowania,
-   centralizację konfiguracji aplikacji.

------------------------------------------------------------------------

# `configureGui()`

Metoda:

``` cpp
void configureGui(DummyGui& gui, SessionManagement& session)
```

otrzymuje dwa już istniejące obiekty.

Nie tworzy ich -- jedynie je łączy.

Przypisuje GUI:

-   adres obiektu `SessionManagement`,
-   wskaźniki do wybranych metod tej klasy.

Dzięki temu `DummyGui` zaczyna mieć możliwość wykonywania tylko tych
operacji, które zostały mu jawnie udostępnione.

------------------------------------------------------------------------

# Stan przed konfiguracją

Po utworzeniu:

``` cpp
DummyGui gui;
```

GUI posiada:

-   `session_ == nullptr`,
-   wszystkie wskaźniki do metod ustawione na `nullptr`.

Nie może więc wykonać żadnej operacji.

------------------------------------------------------------------------

# Stan po konfiguracji

Po wykonaniu:

``` cpp
Configurator configurator;

configurator.configureGui(gui, session);
```

obiekt wygląda logicznie tak:

``` text
DummyGui
├── session_ --------------------------> SessionManagement
├── addVectorFunc_ --------------------> addVectorFromGui()
├── sortVectorFunc_ -------------------> sortVectorFromGui()
├── printDataFunc_ --------------------> printDataFromGui()
├── executeBatchFunc_ -----------------> executeBatch()
└── setSortStrategyFunc_ --------------> setSortStrategyFromGui()
```

------------------------------------------------------------------------

# Czy Configurator przyznaje uprawnienia?

W tym przykładzie można tak na to patrzeć.

GUI może wykonać wyłącznie te operacje, które zostały mu podłączone.

Jest to jednak **mechanizm architektoniczny**, a nie system
bezpieczeństwa użytkowników.

------------------------------------------------------------------------

# `configureAllowedStrategies()`

Druga metoda ustala politykę działania aplikacji.

Przykład:

``` cpp
configurator.configureAllowedStrategies(
    session,
    {
        SortStrategyId::Ascending,
        SortStrategyId::Descending
    }
);
```

Od tego momentu `SessionManagement` zaakceptuje jedynie wskazane
strategie.

------------------------------------------------------------------------

# Dlaczego używany jest `std::move`?

``` cpp
session.setAllowedStrategies(
    std::move(allowed)
);
```

Wektor `allowed` jest lokalnym obiektem.

Po przekazaniu dalej nie jest już potrzebny, więc jego zawartość może
zostać przeniesiona zamiast kopiowana.

------------------------------------------------------------------------

# Configurator a Factory

Factory:

-   tworzy obiekty.

Configurator:

-   nie tworzy obiektów,
-   otrzymuje gotowe obiekty,
-   łączy je,
-   ustawia konfigurację.

------------------------------------------------------------------------

# Configurator a Dependency Injection

`DummyGui` nie wybiera sam swoich zależności.

Są one dostarczane z zewnątrz przez:

``` cpp
configureGui(...)
```

Jest to przykład ręcznego Dependency Injection.

------------------------------------------------------------------------

# Configurator a Composition Root

Sam `Configurator` nie jest Composition Rootem.

Composition Rootem jest zazwyczaj `main()`, który:

1.  tworzy obiekty,
2.  tworzy `Configurator`,
3.  wywołuje jego metody.

------------------------------------------------------------------------

# Zalety

-   centralizacja konfiguracji,
-   jedno miejsce odpowiedzialne za okablowanie,
-   oddzielenie konfiguracji od logiki biznesowej,
-   łatwiejsza modyfikacja architektury.

------------------------------------------------------------------------

# Wady

Obiekty mogą istnieć chwilowo w stanie częściowo skonfigurowanym.

Dla przykładu `DummyGui` może zostać utworzone z pustymi wskaźnikami i
dopiero później zostać skonfigurowane.

W produkcyjnych aplikacjach częściej stosuje się constructor injection.

------------------------------------------------------------------------

# Schemat działania

``` text
                main()
                   │
                   ▼
             Configurator
             │          │
             ▼          ▼
        DummyGui   SessionManagement
             │
             ▼
        click...()
             │
             ▼
     SessionManagement
             │
             ▼
          Engine
```

------------------------------------------------------------------------

# Podsumowanie

`Configurator` odpowiada za składanie aplikacji z gotowych komponentów.

Nie tworzy logiki biznesowej, lecz decyduje, które elementy zostaną ze
sobą połączone oraz jaka polityka działania będzie obowiązywała w danej
konfiguracji.

Jest bardzo dobrym przykładem klasy odpowiedzialnej za ręczne
okablowanie zależności.

------------------------------------------------------------------------

# Pełny kod klasy `Configurator`

``` cpp
class Configurator {
public:
    void configureGui(
        DummyGui& gui,
        SessionManagement& session
    ) {
        appLogger().log(
            "[Configurator] Przyznaję GUI dostęp do wybranych funkcji\n"
        );

        gui.connectAddVector(&session, &SessionManagement::addVectorFromGui);
        gui.connectSortVector(&session, &SessionManagement::sortVectorFromGui);
        gui.connectPrintData(&session, &SessionManagement::printDataFromGui);
        gui.connectExecuteBatch(&session, &SessionManagement::executeBatch);
        gui.connectSetSortStrategy(&session, &SessionManagement::setSortStrategyFromGui);
    }

    void configureAllowedStrategies(
        SessionManagement& session,
        std::vector<SortStrategyId> allowed
    ) {
        appLogger().log(
            "[Configurator] Ustalam dozwolone strategie sortowania\n"
        );

        session.setAllowedStrategies(std::move(allowed));
    }
};
```
