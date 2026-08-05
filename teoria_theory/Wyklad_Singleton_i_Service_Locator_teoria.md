# Singleton --- wzorzec projektowy

## Czym jest Singleton?

Singleton to wzorzec kreacyjny, którego celem jest zagwarantowanie, że w
programie istnieje tylko jedna instancja określonej klasy.

Klasa sama kontroluje sposób tworzenia swojego obiektu i udostępnia
globalny punkt dostępu do tej instancji.

Najczęściej wygląda to koncepcyjnie tak:

``` cpp
SomeClass::instance()
```

Kod nie tworzy obiektu bezpośrednio, lecz pobiera wcześniej
przygotowaną, wspólną instancję.

------------------------------------------------------------------------

## Jaki problem rozwiązuje?

Singleton może być użyteczny, gdy system rzeczywiście powinien posiadać
dokładnie jeden wspólny obiekt, na przykład:

-   menedżer konfiguracji aplikacji,
-   rejestr globalnych ustawień,
-   obiekt reprezentujący dostęp do konkretnego urządzenia,
-   centralny system diagnostyczny,
-   menedżer zasobów procesu.

## Przykładowa implementacja

``` cpp
class ApplicationConfig {
public:
    static ApplicationConfig& instance() {
        static ApplicationConfig object;
        return object;
    }

    ApplicationConfig(const ApplicationConfig&) = delete;
    ApplicationConfig& operator=(const ApplicationConfig&) = delete;

private:
    ApplicationConfig() = default;
};
```

Od C++11 inicjalizacja lokalnej zmiennej statycznej jest bezpieczna
wątkowo.

## Zalety

-   jedna instancja,
-   prosty dostęp,
-   leniwa inicjalizacja,
-   kontrola nad tworzeniem obiektu.

## Wady

-   ukrywa zależności,
-   utrudnia testowanie,
-   tworzy globalny stan.

## Alternatywa

Najczęściej stosuje się Dependency Injection, gdzie zależności
przekazywane są przez konstruktor.

------------------------------------------------------------------------

# Service Locator --- wzorzec projektowy

## Czym jest?

Service Locator jest centralnym rejestrem usług.

Klient może pobrać potrzebny obiekt:

``` cpp
locator.get<Logger>();
locator.get<Database>();
```

## Rejestracja usług

``` cpp
locator.provide<Logger>(logger);
locator.provide<Database>(database);
```

## Zalety

-   centralizacja usług,
-   łatwa wymiana implementacji,
-   wygodny dostęp do wspólnych komponentów.

## Wady

-   ukrywa zależności,
-   utrudnia testowanie,
-   może stać się globalnym magazynem usług.

## Service Locator vs Dependency Injection

Service Locator:

``` cpp
auto& logger = locator.get<Logger>();
```

Dependency Injection:

``` cpp
class ReportGenerator {
public:
    explicit ReportGenerator(ILogger& logger)
        : logger_(logger) {}

private:
    ILogger& logger_;
};
```

Dependency Injection pokazuje zależności wprost, natomiast Service
Locator ukrywa je wewnątrz implementacji.

## Podsumowanie

Singleton odpowiada za istnienie jednej instancji obiektu.

Service Locator odpowiada za odnajdywanie usług.

Są to dwa różne wzorce i rozwiązują różne problemy.
