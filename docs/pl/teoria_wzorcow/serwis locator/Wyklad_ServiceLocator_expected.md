# Wykład -- Ewolucja Service Locator: od wyjątków do `std::expected`

## Cel wykładu

W pierwszej wersji projektu `ServiceLocator` zwraca usługę lub rzuca
wyjątek. To rozwiązanie jest poprawne, ale nie zawsze najlepsze. W C++23
pojawiło się `std::expected`, które pozwala zwracać błąd jako część
wyniku funkcji.

------------------------------------------------------------------------

# Obecna implementacja

Interfejs:

``` cpp
template<typename TService>
TService& get();
```

Implementacja:

``` cpp
template<typename TService>
TService& get()
{
    auto it = services_.find(std::type_index(typeid(TService)));

    if (it == services_.end())
    {
        throw std::runtime_error(
            std::string("ServiceLocator: brak zarejestrowanej usługi typu ")
            + typeid(TService).name());
    }

    return *std::static_pointer_cast<TService>(it->second);
}
```

Użycie:

``` cpp
Logger& logger = ServiceLocator::instance().get<Logger>();
logger.log("Start programu");
```

Jeżeli usługi nie ma:

``` text
get<Logger>()
      |
      v
throw std::runtime_error(...)
```

------------------------------------------------------------------------

# Wady podejścia z wyjątkami

Takie API zakłada, że brak usługi jest sytuacją wyjątkową.

Jeżeli klient chce jedynie sprawdzić, czy usługa istnieje, musi używać:

``` cpp
try
{
    auto& logger = ServiceLocator::instance().get<Logger>();
    logger.log("Hello");
}
catch(...)
{
    ...
}
```

Wyjątki zaczynają służyć do zwykłego sterowania przepływem programu.

------------------------------------------------------------------------

# Czym jest `std::expected`?

`std::expected<T, E>` oznacza:

-   sukces → obiekt typu `T`,
-   błąd → wartość typu `E`.

Nie ma wyjątków. Klient sam decyduje, jak obsłużyć niepowodzenie.

------------------------------------------------------------------------

# Nowy interfejs

``` cpp
template<typename TService>
std::expected<
    std::reference_wrapper<TService>,
    ServiceLocatorError>
tryGet();
```

------------------------------------------------------------------------

# Typ błędu

``` cpp
enum class ServiceLocatorError
{
    ServiceNotFound,
    InvalidServiceType
};
```

------------------------------------------------------------------------

# Implementacja

``` cpp
template<typename TService>
std::expected<
    std::reference_wrapper<TService>,
    ServiceLocatorError>
tryGet()
{
    auto it = services_.find(std::type_index(typeid(TService)));

    if (it == services_.end())
        return std::unexpected(ServiceLocatorError::ServiceNotFound);

    auto service =
        std::dynamic_pointer_cast<TService>(it->second);

    if (!service)
        return std::unexpected(ServiceLocatorError::InvalidServiceType);

    return std::ref(*service);
}
```

------------------------------------------------------------------------

# Użycie

``` cpp
auto logger =
    ServiceLocator::instance().tryGet<Logger>();

if (!logger)
{
    std::cout << "Logger nie istnieje\n";
    return;
}

logger->get().log("Start programu");
```

Brak wyjątków.

------------------------------------------------------------------------

# Interfejs monadyczny C++23

`std::expected` oferuje operacje:

-   `transform()`
-   `and_then()`
-   `or_else()`

Przykład:

``` cpp
ServiceLocator::instance()
    .tryGet<Logger>()
    .transform([](auto logger)
    {
        logger.get().log("Start programu");
    })
    .or_else([](auto)
    {
        std::cout << "Logger nie został odnaleziony\n";
    });
```

Jeżeli pobranie usługi zakończy się sukcesem, wykona się `transform()`.
Jeżeli wystąpi błąd, wykona się `or_else()`.

Nie trzeba pisać wielu instrukcji `if`.

------------------------------------------------------------------------

# Czy usunąć `get()`?

Najlepszym rozwiązaniem jest pozostawienie dwóch metod.

## Usługi obowiązkowe

``` cpp
template<typename TService>
TService& getRequired();
```

Jeżeli usługi nie ma:

``` text
throw
```

Program jest źle skonfigurowany i należy zakończyć działanie.

------------------------------------------------------------------------

## Usługi opcjonalne

``` cpp
template<typename TService>
std::expected<
    std::reference_wrapper<TService>,
    ServiceLocatorError>
tryGet();
```

Klient sam decyduje, co zrobić.

------------------------------------------------------------------------

# Porównanie

## Wyjątki

``` cpp
try
{
    auto& logger = locator.getRequired<Logger>();
    logger.log("Hello");
}
catch(...)
{
    ...
}
```

## `std::expected`

``` cpp
auto logger = locator.tryGet<Logger>();

if (!logger)
    return;

logger->get().log("Hello");
```

## `std::expected` + monady

``` cpp
locator
    .tryGet<Logger>()
    .transform([](auto logger)
    {
        logger.get().log("Hello");
    })
    .or_else(reportError);
```

------------------------------------------------------------------------

# Wnioski

-   `throw` jest dobrym wyborem dla błędów konfiguracyjnych i usług
    obowiązkowych.
-   `std::expected` sprawdza się, gdy brak usługi jest przewidywalnym
    wynikiem operacji.
-   Interfejs monadyczny (`transform`, `and_then`, `or_else`) pozwala
    budować czytelne łańcuchy operacji bez wielu instrukcji `if`.
-   Pozostawienie zarówno `getRequired()`, jak i `tryGet()` daje dwa
    różne kontrakty API: **fail fast** dla usług wymaganych oraz **jawne
    obsługiwanie błędów** dla usług opcjonalnych.
