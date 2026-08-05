# Modyfikacja `ServiceLocator` do `std::expected` — pełna wersja C++23

## Zakres tego wykładu

Ten materiał jest kontynuacją wcześniejszego wykładu o `ServiceLocator`.
Nie omawiamy ponownie:

- idei wzorca Service Locator,
- idei Singletona,
- powodów użycia `IService`,
- podstaw działania `std::unordered_map`, `std::type_index` i RTTI.

Skupiamy się wyłącznie na przebudowie istniejącej klasy:

```cpp
class ServiceLocator;
```

z wersji, która sygnalizowała błędy za pomocą `throw`, do wersji C++23, która zwraca:

```cpp
std::expected<T, ServiceLocatorError>
```

Przebudujemy wszystkie operacje należące do klasy:

- `provide<TService>()`,
- `tryGet<TService>()`,
- `provideRuntime()`,
- `tryGetRuntime<TService>()`,
- `tryGetRuntime(const std::type_index&)`.

Na końcu znajduje się pełny, kompilowalny kod klasy oraz przykłady użycia każdego wariantu API.

---

# 1. Problem w poprzedniej wersji

W pierwotnej klasie operacje pobierające usługę zwracały bezpośrednio referencję albo wskaźnik:

```cpp
template <typename TService>
TService& get();

template <typename TService>
TService& getRuntime();

std::shared_ptr<IService> getRuntime(const std::type_index& key);
```

Jeżeli wpisu nie było w mapie, metoda wykonywała:

```cpp
throw std::runtime_error(...);
```

W wariancie runtime mogło wystąpić także nieudane rzutowanie:

```cpp
auto casted = std::dynamic_pointer_cast<TService>(it->second);

if (!casted) {
    throw std::runtime_error(...);
}
```

Po modyfikacji błędy nie będą już przenoszone poza funkcję jako wyjątki. Staną się częścią typu zwracanego.

Schemat zmiany wygląda następująco:

```text
stara wersja:

wynik albo throw

nowa wersja:

std::expected<wynik, ServiceLocatorError>
```

---

# 2. Wymagane nagłówki

Pełna wersja klasy korzysta z następujących elementów biblioteki standardowej:

```cpp
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>
```

Najważniejsze nowe nagłówki to:

```cpp
#include <expected>
#include <functional>
```

`<expected>` udostępnia:

```cpp
std::expected
std::unexpected
```

`<functional>` udostępnia:

```cpp
std::reference_wrapper
std::ref
```

---

# 3. Typ błędu `ServiceLocatorError`

Przed napisaniem klasy musimy zdefiniować typ opisujący niepowodzenie.

```cpp
enum class ServiceLocatorErrorCode
{
    NullService,
    ServiceNotFound,
    InvalidServiceType
};
```

Same kody mówią, *jaki rodzaj problemu wystąpił*:

- `NullService` — próba zarejestrowania pustego `shared_ptr`,
- `ServiceNotFound` — w mapie nie ma wpisu pod żądanym kluczem,
- `InvalidServiceType` — wpis istnieje, ale nie można bezpiecznie rzutować obiektu na żądany typ.

Sam kod błędu nie mówi jednak, jakiego typu dotyczyła operacja. Dlatego użyjemy struktury:

```cpp
struct ServiceLocatorError
{
    ServiceLocatorErrorCode code;
    std::string serviceType;
};
```

Pole:

```cpp
ServiceLocatorErrorCode code;
```

pozwala obsługiwać błąd programowo, na przykład przez `switch`.

Pole:

```cpp
std::string serviceType;
```

przechowuje nazwę typu lub klucza, którego dotyczył błąd.

Przykładowa wartość:

```cpp
ServiceLocatorError{
    ServiceLocatorErrorCode::ServiceNotFound,
    typeid(Logger).name()
};
```

Nie przechowujemy wyłącznie gotowego komunikatu tekstowego. Kod błędu jest ważny, ponieważ kod klienta może reagować inaczej na różne przypadki:

```cpp
switch (error.code)
{
case ServiceLocatorErrorCode::NullService:
    // błąd rejestracji
    break;

case ServiceLocatorErrorCode::ServiceNotFound:
    // brak wpisu
    break;

case ServiceLocatorErrorCode::InvalidServiceType:
    // niespójność typu
    break;
}
```

---

# 4. Pełny kod zmodyfikowanej klasy

Poniżej znajduje się kompletna wersja klasy. Jest to centralny listing wykładu.

```cpp
class ServiceLocator
{
public:
    using ServicePtr = std::shared_ptr<IService>;
    using RuntimeGetResult =
        std::expected<ServicePtr, ServiceLocatorError>;

    static ServiceLocator& instance()
    {
        static ServiceLocator locator;
        return locator;
    }

    template <typename TService>
    std::expected<void, ServiceLocatorError>
    provide(std::shared_ptr<TService> service)
    {
        static_assert(
            std::is_base_of_v<IService, TService>,
            "TService musi dziedziczyc po IService"
        );

        if (!service)
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::NullService,
                    typeid(TService).name()
                }
            );
        }

        const std::type_index key(typeid(TService));
        services_[key] = std::move(service);

        return {};
    }

    template <typename TService>
    std::expected<
        std::reference_wrapper<TService>,
        ServiceLocatorError
    >
    tryGet()
    {
        static_assert(
            std::is_base_of_v<IService, TService>,
            "TService musi dziedziczyc po IService"
        );

        const std::type_index key(typeid(TService));
        const auto it = services_.find(key);

        if (it == services_.end())
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::ServiceNotFound,
                    key.name()
                }
            );
        }

        return std::ref(
            *std::static_pointer_cast<TService>(it->second)
        );
    }

    std::expected<void, ServiceLocatorError>
    provideRuntime(ServicePtr service)
    {
        if (!service)
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::NullService,
                    "IService"
                }
            );
        }

        const IService& serviceRef = *service;
        const std::type_index key(typeid(serviceRef));

        services_[key] = std::move(service);

        return {};
    }

    template <typename TService>
    std::expected<
        std::reference_wrapper<TService>,
        ServiceLocatorError
    >
    tryGetRuntime()
    {
        static_assert(
            std::is_base_of_v<IService, TService>,
            "TService musi dziedziczyc po IService"
        );

        const std::type_index key(typeid(TService));
        const auto it = services_.find(key);

        if (it == services_.end())
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::ServiceNotFound,
                    key.name()
                }
            );
        }

        auto casted =
            std::dynamic_pointer_cast<TService>(it->second);

        if (!casted)
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::InvalidServiceType,
                    key.name()
                }
            );
        }

        return std::ref(*casted);
    }

    RuntimeGetResult
    tryGetRuntime(const std::type_index& key)
    {
        const auto it = services_.find(key);

        if (it == services_.end())
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::ServiceNotFound,
                    key.name()
                }
            );
        }

        return it->second;
    }

private:
    ServiceLocator() = default;

    std::unordered_map<
        std::type_index,
        ServicePtr
    > services_;
};
```

W następnych rozdziałach omawiamy każdą część tej klasy osobno.

---

# 5. Aliasy typów

Na początku klasy znajdują się dwa aliasy:

```cpp
using ServicePtr = std::shared_ptr<IService>;
```

oraz:

```cpp
using RuntimeGetResult =
    std::expected<ServicePtr, ServiceLocatorError>;
```

## `ServicePtr`

Zamiast wielokrotnie pisać:

```cpp
std::shared_ptr<IService>
```

klasa używa krótszej nazwy:

```cpp
ServicePtr
```

Alias nie tworzy nowego typu. Jest tylko inną nazwą tego samego typu.

Te dwa zapisy są równoważne:

```cpp
ServicePtr service;
```

```cpp
std::shared_ptr<IService> service;
```

## `RuntimeGetResult`

Bez aliasu sygnatura bezszablonowej metody wyglądałaby tak:

```cpp
std::expected<
    std::shared_ptr<IService>,
    ServiceLocatorError
>
tryGetRuntime(const std::type_index& key);
```

Po zastosowaniu aliasu:

```cpp
RuntimeGetResult
tryGetRuntime(const std::type_index& key);
```

Alias opisuje dokładnie kontrakt tej metody:

```text
sukces -> shared_ptr<IService>
błąd  -> ServiceLocatorError
```

---

# 6. `instance()`

Metoda pozostaje bez zmian:

```cpp
static ServiceLocator& instance()
{
    static ServiceLocator locator;
    return locator;
}
```

Migracja na `std::expected` nie wpływa na sposób uzyskiwania dostępu do obiektu `ServiceLocator`.

Nie ma tutaj wyniku, który mógłby oznaczać przewidywalne niepowodzenie. Dlatego ta metoda nie powinna zwracać `std::expected`.

Nie robimy więc czegoś takiego:

```cpp
std::expected<
    std::reference_wrapper<ServiceLocator>,
    ServiceLocatorError
>
instance();
```

Byłoby to sztuczne. Lokalna zmienna statyczna zostanie utworzona przy pierwszym użyciu, a metoda zawsze zwraca referencję do obiektu.

---

# 7. `provide<TService>()`

Pełna metoda:

```cpp
template <typename TService>
std::expected<void, ServiceLocatorError>
provide(std::shared_ptr<TService> service)
{
    static_assert(
        std::is_base_of_v<IService, TService>,
        "TService musi dziedziczyc po IService"
    );

    if (!service)
    {
        return std::unexpected(
            ServiceLocatorError{
                ServiceLocatorErrorCode::NullService,
                typeid(TService).name()
            }
        );
    }

    const std::type_index key(typeid(TService));
    services_[key] = std::move(service);

    return {};
}
```

## 7.1. Typ zwracany

Poprzednio metoda zwracała:

```cpp
void
```

Nowa wersja zwraca:

```cpp
std::expected<void, ServiceLocatorError>
```

`void` w pierwszym parametrze oznacza:

> Operacja może zakończyć się sukcesem, ale przy sukcesie nie ma dodatkowej wartości do zwrócenia.

Mamy zatem dwa stany:

```text
sukces -> expected zawiera stan sukcesu, ale bez wartości
błąd  -> expected zawiera ServiceLocatorError
```

## 7.2. `static_assert`

```cpp
static_assert(
    std::is_base_of_v<IService, TService>,
    "TService musi dziedziczyc po IService"
);
```

Ten fragment nie został zamieniony na `std::expected`.

Powód jest zasadniczy: brak dziedziczenia po `IService` jest błędem wykrywanym w czasie kompilacji, a `std::expected` opisuje wynik operacji wykonywanej w czasie działania programu.

Niepoprawne byłoby przenoszenie błędu kompilacyjnego do runtime:

```cpp
if constexpr (!std::is_base_of_v<IService, TService>)
{
    return std::unexpected(...);
}
```

Kod z nieprawidłowym typem powinien się nie kompilować.

## 7.3. Kontrola pustego `shared_ptr`

```cpp
if (!service)
```

W poprzedniej wersji można było wykonać:

```cpp
locator.provide<Logger>(nullptr);
```

Mapa zawierałaby wtedy klucz `Logger`, ale przypisana wartość byłaby pustym wskaźnikiem.
Późniejsze pobranie i dereferencja mogłyby doprowadzić do niezdefiniowanego zachowania.

Nowa wersja zatrzymuje taki błąd podczas rejestracji:

```cpp
return std::unexpected(
    ServiceLocatorError{
        ServiceLocatorErrorCode::NullService,
        typeid(TService).name()
    }
);
```

## 7.4. `std::unexpected`

`std::expected` reprezentuje sukces. Aby jawnie utworzyć stan błędu, używamy:

```cpp
std::unexpected(error)
```

W naszym przypadku:

```cpp
std::unexpected(
    ServiceLocatorError{
        ServiceLocatorErrorCode::NullService,
        typeid(TService).name()
    }
)
```

## 7.5. Utworzenie klucza

```cpp
const std::type_index key(typeid(TService));
```

Klucz jest identyczny jak w poprzedniej implementacji. Migracja na `std::expected` nie zmienia sposobu indeksowania usług.

## 7.6. Umieszczenie usługi w mapie

```cpp
services_[key] = std::move(service);
```

Jeżeli wpis pod danym kluczem nie istnieje, zostaje utworzony.
Jeżeli istnieje, poprzedni wskaźnik zostaje zastąpiony nowym.

Zastąpienie istniejącej usługi nie jest w tej wersji błędem. Jest świadomie dozwoloną operacją.

## 7.7. Zwrócenie sukcesu

```cpp
return {};
```

Dla typu:

```cpp
std::expected<void, ServiceLocatorError>
```

pusty inicjalizator tworzy stan sukcesu.

Można to odczytać jako:

```text
rejestracja zakończyła się poprawnie
```

## 7.8. Użycie `provide()`

```cpp
auto result = ServiceLocator::instance().provide<Logger>(
    std::make_shared<Logger>()
);

if (!result)
{
    const ServiceLocatorError& error = result.error();
    // obsługa błędu
}
```

Przy poprawnej usłudze:

```cpp
result.has_value() == true
```

Przy pustym wskaźniku:

```cpp
result.has_value() == false
result.error().code == ServiceLocatorErrorCode::NullService
```

---

# 8. `tryGet<TService>()`

Pełna metoda:

```cpp
template <typename TService>
std::expected<
    std::reference_wrapper<TService>,
    ServiceLocatorError
>
tryGet()
{
    static_assert(
        std::is_base_of_v<IService, TService>,
        "TService musi dziedziczyc po IService"
    );

    const std::type_index key(typeid(TService));
    const auto it = services_.find(key);

    if (it == services_.end())
    {
        return std::unexpected(
            ServiceLocatorError{
                ServiceLocatorErrorCode::ServiceNotFound,
                key.name()
            }
        );
    }

    return std::ref(
        *std::static_pointer_cast<TService>(it->second)
    );
}
```

## 8.1. Zmiana nazwy

Poprzednia metoda nazywała się:

```cpp
get<TService>()
```

Nowa metoda nazywa się:

```cpp
tryGet<TService>()
```

Przedrostek `try` informuje klienta API, że operacja może nie zwrócić żądanej usługi i należy sprawdzić wynik.

Sama nazwa nie zapewnia bezpieczeństwa, ale komunikuje kontrakt funkcji.

## 8.2. Typ sukcesu

Metoda nie może zwracać:

```cpp
std::expected<TService&, ServiceLocatorError>
```

`std::expected` nie może przechowywać typu referencyjnego jako `T`.

Dlatego używamy:

```cpp
std::reference_wrapper<TService>
```

Pełny typ wyniku to:

```cpp
std::expected<
    std::reference_wrapper<TService>,
    ServiceLocatorError
>
```

Znaczenie:

```text
sukces -> referencja opakowana w reference_wrapper
błąd  -> ServiceLocatorError
```

## 8.3. Dlaczego nie zwracamy `shared_ptr<TService>`?

Możliwa byłaby również sygnatura:

```cpp
std::expected<
    std::shared_ptr<TService>,
    ServiceLocatorError
>
```

W tym wykładzie zachowujemy jednak semantykę poprzedniej metody `get()`, która zwracała:

```cpp
TService&
```

`std::reference_wrapper<TService>` jest najbliższym odpowiednikiem referencji wewnątrz `std::expected`.

Różnica semantyczna jest ważna:

- `reference_wrapper` daje dostęp do obiektu, ale nie współdzieli własności,
- `shared_ptr` zwiększa licznik właścicieli i może przedłużyć życie usługi.

W naszej klasie właścicielem usługi pozostaje mapa `services_`.

## 8.4. Wyszukiwanie

```cpp
const std::type_index key(typeid(TService));
const auto it = services_.find(key);
```

Mechanizm wyszukiwania nie ulega zmianie.

## 8.5. Brak usługi

```cpp
if (it == services_.end())
```

Poprzednio następował `throw`. Teraz zwracamy:

```cpp
return std::unexpected(
    ServiceLocatorError{
        ServiceLocatorErrorCode::ServiceNotFound,
        key.name()
    }
);
```

Funkcja kończy się normalnym `return`. Nie ma mechanizmu rozwijania stosu i nie jest wymagany `try/catch`.

## 8.6. `static_pointer_cast`

```cpp
std::static_pointer_cast<TService>(it->second)
```

W szablonowym wariancie `provide<TService>()` klucz jest tworzony na podstawie dokładnie tego samego `TService`:

```cpp
services_[typeid(TService)] = service;
```

Dlatego po znalezieniu wpisu pod kluczem `typeid(TService)` klasa zakłada spójność własnej mapy i używa `static_pointer_cast`.

To jest ta sama decyzja, która występowała w poprzedniej wersji klasy.

## 8.7. `std::ref`

Po rzutowaniu otrzymujemy:

```cpp
std::shared_ptr<TService>
```

Dereferencja daje:

```cpp
TService&
```

Następnie:

```cpp
std::ref(...)
```

opakowuje referencję w:

```cpp
std::reference_wrapper<TService>
```

Cały zapis:

```cpp
return std::ref(
    *std::static_pointer_cast<TService>(it->second)
);
```

## 8.8. Klasyczne użycie

```cpp
auto result =
    ServiceLocator::instance().tryGet<Logger>();

if (!result)
{
    const auto& error = result.error();
    // obsługa błędu
    return;
}

Logger& logger = result->get();
logger.log("Hello\n");
```

`result->get()` działa dlatego, że wartością `expected` jest `reference_wrapper<Logger>`.

Można także napisać:

```cpp
Logger& logger = result.value().get();
```

Różnica jest taka, że `value()` sprawdza stan i może rzucić `std::bad_expected_access`, jeśli zostanie wywołane na błędzie. Po wcześniejszym `if (!result)` oba zapisy są poprawne.

---

# 9. `provideRuntime()`

Pełna metoda:

```cpp
std::expected<void, ServiceLocatorError>
provideRuntime(ServicePtr service)
{
    if (!service)
    {
        return std::unexpected(
            ServiceLocatorError{
                ServiceLocatorErrorCode::NullService,
                "IService"
            }
        );
    }

    const IService& serviceRef = *service;
    const std::type_index key(typeid(serviceRef));

    services_[key] = std::move(service);

    return {};
}
```

## 9.1. Typ argumentu

Metoda przyjmuje:

```cpp
ServicePtr service
```

czyli:

```cpp
std::shared_ptr<IService> service
```

W odróżnieniu od `provide<TService>()` metoda nie otrzymuje konkretnego typu jako parametru szablonu.

## 9.2. Typ zwracany

Tak samo jak `provide<TService>()`, metoda zwraca:

```cpp
std::expected<void, ServiceLocatorError>
```

Rejestracja nie produkuje wartości użytkowej. Może jednak zakończyć się błędem, jeżeli wskaźnik jest pusty.

## 9.3. Kontrola pustego wskaźnika

Ta kontrola musi wystąpić przed dereferencją:

```cpp
if (!service)
```

Bez niej następna linia:

```cpp
const IService& serviceRef = *service;
```

dereferencjonowałaby pusty wskaźnik.

W błędzie zapisujemy typ `"IService"`, ponieważ metoda nie ma parametru `TService`, a dla pustego wskaźnika nie da się odczytać rzeczywistego typu obiektu.

## 9.4. Ustalenie typu runtime

```cpp
const IService& serviceRef = *service;
const std::type_index key(typeid(serviceRef));
```

Po potwierdzeniu, że wskaźnik nie jest pusty, można bezpiecznie wykonać dereferencję.

Ponieważ `IService` jest polimorficzny, `typeid(serviceRef)` zwraca dynamiczny typ obiektu.

Dla:

```cpp
std::shared_ptr<IService> service =
    std::make_shared<DoSomething>();
```

kluczem będzie:

```cpp
typeid(DoSomething)
```

## 9.5. Rejestracja

```cpp
services_[key] = std::move(service);
```

Obiekt zostaje zapisany pod kluczem ustalonym w runtime.

## 9.6. Sukces

```cpp
return {};
```

Tak samo jak w `provide<TService>()`, oznacza to poprawne zakończenie operacji.

## 9.7. Użycie

```cpp
std::shared_ptr<IService> service =
    std::make_shared<DoSomething>();

auto result =
    ServiceLocator::instance().provideRuntime(service);

if (!result)
{
    // obsługa błędu rejestracji
}
```

---

# 10. `tryGetRuntime<TService>()`

Pełna metoda:

```cpp
template <typename TService>
std::expected<
    std::reference_wrapper<TService>,
    ServiceLocatorError
>
tryGetRuntime()
{
    static_assert(
        std::is_base_of_v<IService, TService>,
        "TService musi dziedziczyc po IService"
    );

    const std::type_index key(typeid(TService));
    const auto it = services_.find(key);

    if (it == services_.end())
    {
        return std::unexpected(
            ServiceLocatorError{
                ServiceLocatorErrorCode::ServiceNotFound,
                key.name()
            }
        );
    }

    auto casted =
        std::dynamic_pointer_cast<TService>(it->second);

    if (!casted)
    {
        return std::unexpected(
            ServiceLocatorError{
                ServiceLocatorErrorCode::InvalidServiceType,
                key.name()
            }
        );
    }

    return std::ref(*casted);
}
```

## 10.1. Kontrakt

Metoda zwraca taki sam typ jak `tryGet<TService>()`:

```cpp
std::expected<
    std::reference_wrapper<TService>,
    ServiceLocatorError
>
```

Różnica nie dotyczy wyniku, lecz sposobu weryfikacji typu.

## 10.2. Wyszukiwanie wpisu

```cpp
const std::type_index key(typeid(TService));
const auto it = services_.find(key);
```

Jeżeli wpis nie istnieje, zwracamy:

```cpp
ServiceLocatorErrorCode::ServiceNotFound
```

## 10.3. Bezpieczne rzutowanie runtime

```cpp
auto casted =
    std::dynamic_pointer_cast<TService>(it->second);
```

W przeciwieństwie do `static_pointer_cast`, `dynamic_pointer_cast` wykonuje kontrolę typu podczas działania programu.

Rezultat ma typ:

```cpp
std::shared_ptr<TService>
```

Jeżeli rzutowanie się nie powiedzie, wskaźnik jest pusty:

```cpp
if (!casted)
```

## 10.4. Błąd typu

W poprzedniej wersji występował:

```cpp
throw std::runtime_error(...);
```

W nowej wersji:

```cpp
return std::unexpected(
    ServiceLocatorError{
        ServiceLocatorErrorCode::InvalidServiceType,
        key.name()
    }
);
```

To drugi możliwy błąd tej metody:

```text
1. ServiceNotFound
2. InvalidServiceType
```

## 10.5. Sukces

```cpp
return std::ref(*casted);
```

`casted` utrzymuje obiekt przy życiu podczas wykonywania funkcji. Mapa również nadal przechowuje własny `shared_ptr`.

Do `expected` trafia `reference_wrapper<TService>`.

## 10.6. Użycie

```cpp
auto result =
    ServiceLocator::instance()
        .tryGetRuntime<DoSomething>();

if (!result)
{
    const ServiceLocatorError& error = result.error();
    return;
}

result->get().do_();
```

---

# 11. Bezszablonowe `tryGetRuntime(key)`

Pełna metoda:

```cpp
RuntimeGetResult
tryGetRuntime(const std::type_index& key)
{
    const auto it = services_.find(key);

    if (it == services_.end())
    {
        return std::unexpected(
            ServiceLocatorError{
                ServiceLocatorErrorCode::ServiceNotFound,
                key.name()
            }
        );
    }

    return it->second;
}
```

## 11.1. Typ zwracany

Alias:

```cpp
using RuntimeGetResult =
    std::expected<ServicePtr, ServiceLocatorError>;
```

rozwija się do:

```cpp
std::expected<
    std::shared_ptr<IService>,
    ServiceLocatorError
>
```

Ta metoda nie zwraca `reference_wrapper`, ponieważ jej wcześniejsza wersja zwracała:

```cpp
std::shared_ptr<IService>
```

Zachowujemy więc jej poprzednią semantykę.

## 11.2. Argument `key`

```cpp
const std::type_index& key
```

Klient sam określa, pod jakim kluczem ma zostać wykonane wyszukiwanie.

Przykład:

```cpp
std::type_index(typeid(DoSomething))
```

## 11.3. Brak wpisu

```cpp
if (it == services_.end())
```

zwraca:

```cpp
std::unexpected(
    ServiceLocatorError{
        ServiceLocatorErrorCode::ServiceNotFound,
        key.name()
    }
)
```

## 11.4. Sukces

```cpp
return it->second;
```

`it->second` ma typ:

```cpp
std::shared_ptr<IService>
```

Pasuje bezpośrednio do typu wartości w `RuntimeGetResult`.

Skopiowanie `shared_ptr` zwiększa licznik współdzielonej własności. Oznacza to, że pobrany obiekt pozostanie przy życiu tak długo, jak długo klient zachowuje zwrócony wskaźnik, nawet gdyby wpis został później usunięty z rejestru.

## 11.5. Użycie

```cpp
auto result =
    ServiceLocator::instance().tryGetRuntime(
        std::type_index(typeid(DoSomething))
    );

if (!result)
{
    return;
}

std::shared_ptr<IService> service = *result;

auto* concrete =
    dynamic_cast<DoSomething*>(service.get());

if (concrete)
{
    concrete->do_();
}
```

`ServiceLocator` nie wykonuje tutaj końcowego rzutowania na konkretną usługę. Zwraca tylko `shared_ptr<IService>`. Odpowiedzialność za dalszą interpretację typu należy do klienta.

---

# 12. Pole `services_`

```cpp
std::unordered_map<
    std::type_index,
    ServicePtr
> services_;
```

Po rozwinięciu aliasu:

```cpp
std::unordered_map<
    std::type_index,
    std::shared_ptr<IService>
> services_;
```

Samo pole nie musi być zmieniane podczas migracji na `std::expected`.

`std::expected` zmienia sposób zwracania wyników i błędów przez publiczne metody. Nie zmienia sposobu przechowywania usług.

---

# 13. Prywatny konstruktor

```cpp
ServiceLocator() = default;
```

Ta część również pozostaje bez zmian.

Nie zwraca wyniku i nie wykonuje operacji, która wymagałaby jawnej obsługi błędu w `std::expected`.

---

# 14. Pełny kod wraz z typami pomocniczymi

Poniższy listing zawiera wszystko, co jest wymagane do użycia zmodyfikowanej klasy:

- nagłówki,
- `IService`,
- kody błędów,
- strukturę błędu,
- kompletną klasę `ServiceLocator`.

```cpp
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>

class IService
{
public:
    virtual ~IService() = default;
};

enum class ServiceLocatorErrorCode
{
    NullService,
    ServiceNotFound,
    InvalidServiceType
};

struct ServiceLocatorError
{
    ServiceLocatorErrorCode code;
    std::string serviceType;
};

class ServiceLocator
{
public:
    using ServicePtr = std::shared_ptr<IService>;
    using RuntimeGetResult =
        std::expected<ServicePtr, ServiceLocatorError>;

    static ServiceLocator& instance()
    {
        static ServiceLocator locator;
        return locator;
    }

    template <typename TService>
    std::expected<void, ServiceLocatorError>
    provide(std::shared_ptr<TService> service)
    {
        static_assert(
            std::is_base_of_v<IService, TService>,
            "TService musi dziedziczyc po IService"
        );

        if (!service)
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::NullService,
                    typeid(TService).name()
                }
            );
        }

        const std::type_index key(typeid(TService));
        services_[key] = std::move(service);

        return {};
    }

    template <typename TService>
    std::expected<
        std::reference_wrapper<TService>,
        ServiceLocatorError
    >
    tryGet()
    {
        static_assert(
            std::is_base_of_v<IService, TService>,
            "TService musi dziedziczyc po IService"
        );

        const std::type_index key(typeid(TService));
        const auto it = services_.find(key);

        if (it == services_.end())
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::ServiceNotFound,
                    key.name()
                }
            );
        }

        return std::ref(
            *std::static_pointer_cast<TService>(it->second)
        );
    }

    std::expected<void, ServiceLocatorError>
    provideRuntime(ServicePtr service)
    {
        if (!service)
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::NullService,
                    "IService"
                }
            );
        }

        const IService& serviceRef = *service;
        const std::type_index key(typeid(serviceRef));

        services_[key] = std::move(service);

        return {};
    }

    template <typename TService>
    std::expected<
        std::reference_wrapper<TService>,
        ServiceLocatorError
    >
    tryGetRuntime()
    {
        static_assert(
            std::is_base_of_v<IService, TService>,
            "TService musi dziedziczyc po IService"
        );

        const std::type_index key(typeid(TService));
        const auto it = services_.find(key);

        if (it == services_.end())
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::ServiceNotFound,
                    key.name()
                }
            );
        }

        auto casted =
            std::dynamic_pointer_cast<TService>(it->second);

        if (!casted)
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::InvalidServiceType,
                    key.name()
                }
            );
        }

        return std::ref(*casted);
    }

    RuntimeGetResult
    tryGetRuntime(const std::type_index& key)
    {
        const auto it = services_.find(key);

        if (it == services_.end())
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::ServiceNotFound,
                    key.name()
                }
            );
        }

        return it->second;
    }

private:
    ServiceLocator() = default;

    std::unordered_map<
        std::type_index,
        ServicePtr
    > services_;
};
```

---

# 15. Funkcja zamieniająca błąd na tekst

Sama klasa zwraca strukturalny błąd. Kod wyświetlający komunikat może znajdować się poza klasą:

```cpp
std::string toString(const ServiceLocatorError& error)
{
    switch (error.code)
    {
    case ServiceLocatorErrorCode::NullService:
        return "ServiceLocator: pusty wskaznik uslugi typu "
            + error.serviceType;

    case ServiceLocatorErrorCode::ServiceNotFound:
        return "ServiceLocator: brak uslugi typu "
            + error.serviceType;

    case ServiceLocatorErrorCode::InvalidServiceType:
        return "ServiceLocator: niepoprawny typ uslugi dla klucza "
            + error.serviceType;
    }

    return "ServiceLocator: nieznany blad";
}
```

Oddzielenie kodu błędu od tekstu daje dwie możliwości:

```cpp
if (error.code == ServiceLocatorErrorCode::ServiceNotFound)
{
    // reakcja programowa
}
```

oraz:

```cpp
std::cerr << toString(error) << '\n';
```

`ServiceLocator` nie musi sam logować swoich błędów. Jest to istotne zwłaszcza wtedy, gdy usługą, której nie udało się pobrać, jest właśnie `Logger`.

---

# 16. Pełne przykłady wszystkich metod

Załóżmy następujące usługi:

```cpp
class Logger : public IService
{
public:
    void log(const std::string& message)
    {
        std::cout << message;
    }
};

class DoSomething : public IService
{
public:
    void do_()
    {
        std::cout << "do something\n";
    }
};
```

## 16.1. `provide<Logger>()`

```cpp
auto provideResult =
    ServiceLocator::instance().provide<Logger>(
        std::make_shared<Logger>()
    );

if (!provideResult)
{
    std::cerr
        << toString(provideResult.error())
        << '\n';
}
```

## 16.2. `tryGet<Logger>()`

```cpp
auto loggerResult =
    ServiceLocator::instance().tryGet<Logger>();

if (!loggerResult)
{
    std::cerr
        << toString(loggerResult.error())
        << '\n';
}
else
{
    loggerResult->get().log("Hello\n");
}
```

## 16.3. `provideRuntime()`

```cpp
std::shared_ptr<IService> runtimeService =
    std::make_shared<DoSomething>();

auto runtimeProvideResult =
    ServiceLocator::instance().provideRuntime(
        std::move(runtimeService)
    );

if (!runtimeProvideResult)
{
    std::cerr
        << toString(runtimeProvideResult.error())
        << '\n';
}
```

## 16.4. `tryGetRuntime<DoSomething>()`

```cpp
auto typedRuntimeResult =
    ServiceLocator::instance()
        .tryGetRuntime<DoSomething>();

if (!typedRuntimeResult)
{
    std::cerr
        << toString(typedRuntimeResult.error())
        << '\n';
}
else
{
    typedRuntimeResult->get().do_();
}
```

## 16.5. `tryGetRuntime(type_index)`

```cpp
auto untypedRuntimeResult =
    ServiceLocator::instance().tryGetRuntime(
        std::type_index(typeid(DoSomething))
    );

if (!untypedRuntimeResult)
{
    std::cerr
        << toString(untypedRuntimeResult.error())
        << '\n';
}
else
{
    std::shared_ptr<IService> service =
        *untypedRuntimeResult;

    auto* concrete =
        dynamic_cast<DoSomething*>(service.get());

    if (concrete)
    {
        concrete->do_();
    }
}
```

---

# 17. Użycie operacji monadycznych

`std::expected` w C++23 udostępnia operacje monadyczne. W tej klasie najbardziej naturalne są `transform()` i `or_else()`.

## 17.1. `transform()` dla pobranej usługi

```cpp
ServiceLocator::instance()
    .tryGet<Logger>()
    .transform([](std::reference_wrapper<Logger> logger)
    {
        logger.get().log("Hello\n");
    });
```

Lambda wykona się tylko wtedy, gdy `tryGet<Logger>()` zwróci sukces.

Jeżeli wynikiem będzie `ServiceNotFound`, `transform()` nie wywoła lambdy i przekaże błąd dalej.

## 17.2. `or_else()` dla błędu

```cpp
ServiceLocator::instance()
    .tryGet<Logger>()
    .or_else([](const ServiceLocatorError& error)
        -> std::expected<
            std::reference_wrapper<Logger>,
            ServiceLocatorError
        >
    {
        std::cerr << toString(error) << '\n';
        return std::unexpected(error);
    });
```

Ważne: lambda przekazana do `or_else()` musi zwrócić typ zgodny z wymaganym wynikiem operacji. Samo:

```cpp
.or_else([](const auto& error)
{
    std::cerr << toString(error);
});
```

nie jest uniwersalnie poprawnym przykładem dla `std::expected`, ponieważ typ wyniku lambdy musi spełniać kontrakt `or_else()`.

## 17.3. `and_then()`

`and_then()` jest przeznaczone dla kolejnego kroku, który również zwraca `std::expected`.

Przykładowa funkcja:

```cpp
enum class LogError
{
    EmptyMessage
};

std::expected<void, LogError>
writeMessage(
    std::reference_wrapper<Logger> logger,
    const std::string& message
)
{
    if (message.empty())
    {
        return std::unexpected(LogError::EmptyMessage);
    }

    logger.get().log(message);
    return {};
}
```

Nie można bezpośrednio połączyć tego z `tryGet<Logger>()`, ponieważ oba `expected` mają różne typy błędu:

```cpp
ServiceLocatorError
LogError
```

Aby utworzyć jeden łańcuch, należy użyć wspólnego typu błędu albo przekształcić błędy do jednego typu.

Przykład ze wspólnym błędem:

```cpp
std::expected<void, ServiceLocatorError>
useLogger(std::reference_wrapper<Logger> logger)
{
    logger.get().log("Hello\n");
    return {};
}

ServiceLocator::instance()
    .tryGet<Logger>()
    .and_then(useLogger);
```

Przepływ:

```text
tryGet<Logger>()
       |
       | sukces
       v
useLogger(...)

lub

tryGet<Logger>()
       |
       | błąd
       v
useLogger nie zostaje wywołane
```

## 17.4. `transform_error()`

C++23 udostępnia także:

```cpp
transform_error()
```

Pozwala zmienić typ lub postać błędu.

Przykład zamiany struktury na tekst:

```cpp
auto result =
    ServiceLocator::instance()
        .tryGet<Logger>()
        .transform_error([](const ServiceLocatorError& error)
        {
            return toString(error);
        });
```

Typ `result` staje się:

```cpp
std::expected<
    std::reference_wrapper<Logger>,
    std::string
>
```

Wartość sukcesu pozostaje taka sama, ale błąd zmienia się z:

```cpp
ServiceLocatorError
```

na:

```cpp
std::string
```

---

# 18. Pełny kompilowalny przykład programu

Poniższy program zawiera kompletną klasę i używa wszystkich jej publicznych metod.

```cpp
#include <expected>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>

class IService
{
public:
    virtual ~IService() = default;
};

class Logger : public IService
{
public:
    void log(const std::string& message)
    {
        std::cout << message;
    }
};

class DoSomething : public IService
{
public:
    void do_()
    {
        std::cout << "do something\n";
    }
};

enum class ServiceLocatorErrorCode
{
    NullService,
    ServiceNotFound,
    InvalidServiceType
};

struct ServiceLocatorError
{
    ServiceLocatorErrorCode code;
    std::string serviceType;
};

std::string toString(const ServiceLocatorError& error)
{
    switch (error.code)
    {
    case ServiceLocatorErrorCode::NullService:
        return "ServiceLocator: pusty wskaznik uslugi typu "
            + error.serviceType;

    case ServiceLocatorErrorCode::ServiceNotFound:
        return "ServiceLocator: brak uslugi typu "
            + error.serviceType;

    case ServiceLocatorErrorCode::InvalidServiceType:
        return "ServiceLocator: niepoprawny typ uslugi dla klucza "
            + error.serviceType;
    }

    return "ServiceLocator: nieznany blad";
}

class ServiceLocator
{
public:
    using ServicePtr = std::shared_ptr<IService>;
    using RuntimeGetResult =
        std::expected<ServicePtr, ServiceLocatorError>;

    static ServiceLocator& instance()
    {
        static ServiceLocator locator;
        return locator;
    }

    template <typename TService>
    std::expected<void, ServiceLocatorError>
    provide(std::shared_ptr<TService> service)
    {
        static_assert(
            std::is_base_of_v<IService, TService>,
            "TService musi dziedziczyc po IService"
        );

        if (!service)
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::NullService,
                    typeid(TService).name()
                }
            );
        }

        const std::type_index key(typeid(TService));
        services_[key] = std::move(service);

        return {};
    }

    template <typename TService>
    std::expected<
        std::reference_wrapper<TService>,
        ServiceLocatorError
    >
    tryGet()
    {
        static_assert(
            std::is_base_of_v<IService, TService>,
            "TService musi dziedziczyc po IService"
        );

        const std::type_index key(typeid(TService));
        const auto it = services_.find(key);

        if (it == services_.end())
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::ServiceNotFound,
                    key.name()
                }
            );
        }

        return std::ref(
            *std::static_pointer_cast<TService>(it->second)
        );
    }

    std::expected<void, ServiceLocatorError>
    provideRuntime(ServicePtr service)
    {
        if (!service)
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::NullService,
                    "IService"
                }
            );
        }

        const IService& serviceRef = *service;
        const std::type_index key(typeid(serviceRef));

        services_[key] = std::move(service);

        return {};
    }

    template <typename TService>
    std::expected<
        std::reference_wrapper<TService>,
        ServiceLocatorError
    >
    tryGetRuntime()
    {
        static_assert(
            std::is_base_of_v<IService, TService>,
            "TService musi dziedziczyc po IService"
        );

        const std::type_index key(typeid(TService));
        const auto it = services_.find(key);

        if (it == services_.end())
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::ServiceNotFound,
                    key.name()
                }
            );
        }

        auto casted =
            std::dynamic_pointer_cast<TService>(it->second);

        if (!casted)
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::InvalidServiceType,
                    key.name()
                }
            );
        }

        return std::ref(*casted);
    }

    RuntimeGetResult
    tryGetRuntime(const std::type_index& key)
    {
        const auto it = services_.find(key);

        if (it == services_.end())
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::ServiceNotFound,
                    key.name()
                }
            );
        }

        return it->second;
    }

private:
    ServiceLocator() = default;

    std::unordered_map<
        std::type_index,
        ServicePtr
    > services_;
};

int main()
{
    ServiceLocator& locator =
        ServiceLocator::instance();

    auto loggerRegistration =
        locator.provide<Logger>(
            std::make_shared<Logger>()
        );

    if (!loggerRegistration)
    {
        std::cerr
            << toString(loggerRegistration.error())
            << '\n';
        return 1;
    }

    auto logger = locator.tryGet<Logger>();

    if (!logger)
    {
        std::cerr
            << toString(logger.error())
            << '\n';
        return 1;
    }

    logger->get().log("Logger pobrany przez tryGet\n");

    std::shared_ptr<IService> runtimeService =
        std::make_shared<DoSomething>();

    auto runtimeRegistration =
        locator.provideRuntime(
            std::move(runtimeService)
        );

    if (!runtimeRegistration)
    {
        std::cerr
            << toString(runtimeRegistration.error())
            << '\n';
        return 1;
    }

    auto typedRuntime =
        locator.tryGetRuntime<DoSomething>();

    if (!typedRuntime)
    {
        std::cerr
            << toString(typedRuntime.error())
            << '\n';
        return 1;
    }

    typedRuntime->get().do_();

    auto untypedRuntime =
        locator.tryGetRuntime(
            std::type_index(typeid(DoSomething))
        );

    if (!untypedRuntime)
    {
        std::cerr
            << toString(untypedRuntime.error())
            << '\n';
        return 1;
    }

    auto* concrete =
        dynamic_cast<DoSomething*>(
            untypedRuntime->get()
        );

    if (!concrete)
    {
        std::cerr
            << "Klient: nieudane rzutowanie DoSomething\n";
        return 1;
    }

    concrete->do_();

    locator.tryGet<Logger>()
        .transform([](std::reference_wrapper<Logger> service)
        {
            service.get().log(
                "Logger pobrany przez transform\n"
            );
        });

    return 0;
}
```

Program należy kompilować w trybie C++23, na przykład:

```bash
g++ -std=c++23 -Wall -Wextra -pedantic main.cpp
```

---

# 19. Zestawienie starego i nowego API

## Rejestracja szablonowa

Stara wersja:

```cpp
template <typename TService>
void provide(std::shared_ptr<TService> service);
```

Nowa wersja:

```cpp
template <typename TService>
std::expected<void, ServiceLocatorError>
provide(std::shared_ptr<TService> service);
```

Nowa wersja potrafi jawnie zgłosić `NullService`.

## Pobranie szablonowe

Stara wersja:

```cpp
template <typename TService>
TService& get();
```

Nowa wersja:

```cpp
template <typename TService>
std::expected<
    std::reference_wrapper<TService>,
    ServiceLocatorError
>
tryGet();
```

Nowa wersja zwraca `ServiceNotFound` zamiast rzucać wyjątek.

## Rejestracja runtime

Stara wersja:

```cpp
void provideRuntime(std::shared_ptr<IService> service);
```

Nowa wersja:

```cpp
std::expected<void, ServiceLocatorError>
provideRuntime(ServicePtr service);
```

Nowa wersja sprawdza pusty wskaźnik przed `typeid(*service)`.

## Pobranie runtime z szablonem

Stara wersja:

```cpp
template <typename TService>
TService& getRuntime();
```

Nowa wersja:

```cpp
template <typename TService>
std::expected<
    std::reference_wrapper<TService>,
    ServiceLocatorError
>
tryGetRuntime();
```

Możliwe błędy:

```text
ServiceNotFound
InvalidServiceType
```

## Pobranie runtime bez szablonu

Stara wersja:

```cpp
std::shared_ptr<IService>
getRuntime(const std::type_index& key);
```

Nowa wersja:

```cpp
RuntimeGetResult
tryGetRuntime(const std::type_index& key);
```

Czyli:

```cpp
std::expected<
    std::shared_ptr<IService>,
    ServiceLocatorError
>
```

---

# 20. Najważniejsze decyzje projektowe w tej modyfikacji

## 20.1. `static_assert` pozostaje

Błędy możliwe do wykrycia podczas kompilacji nadal są błędami kompilacji.
`std::expected` nie zastępuje systemu typów.

## 20.2. Rejestracja również zwraca `expected`

Pierwotne `provide()` i `provideRuntime()` zwracały `void`, ale przyjmowały `shared_ptr`. Pusty wskaźnik jest przewidywalnym błędem wejścia, dlatego pełna migracja obejmuje także te metody.

## 20.3. Pobranie referencji używa `reference_wrapper`

`std::expected<T&, E>` jest niedozwolone. `std::reference_wrapper<T>` zachowuje semantykę dostępu przez referencję bez przekazywania klientowi współwłasności.

## 20.4. Bezszablonowy runtime nadal zwraca `shared_ptr`

Ta metoda już wcześniej przekazywała klientowi współwłasność. Migracja na `std::expected` nie powinna bez potrzeby zmieniać jej semantyki.

## 20.5. Błąd ma kod i dane

`ServiceLocatorError` nie jest tylko napisem. Kod błędu umożliwia reakcję programową, a `serviceType` dostarcza kontekstu diagnostycznego.

## 20.6. Klasa nie loguje własnych błędów

`ServiceLocator` zwraca błąd klientowi. Nie próbuje pobrać `Loggera`, aby zgłosić brak `Loggera`, ponieważ prowadziłoby to do błędnego koła zależności.

## 20.7. `value()` nie powinno zastąpić kontroli wyniku

Można napisać:

```cpp
locator.tryGet<Logger>().value().get().log("Hello");
```

ale przy błędzie `value()` rzuci `std::bad_expected_access`.

Taki zapis niweczy główną korzyść migracji. Poprawny klient powinien:

- sprawdzić wynik przez `if`, albo
- użyć operacji monadycznych.

---

# 21. Ostateczna postać kontraktów

Po migracji publiczne API klasy ma następujące kontrakty:

```cpp
// Rejestracja typu znanego w czasie kompilacji.
// Sukces: brak wartości.
// Błąd: NullService.
template <typename TService>
std::expected<void, ServiceLocatorError>
provide(std::shared_ptr<TService> service);
```

```cpp
// Pobranie typu znanego w czasie kompilacji.
// Sukces: referencja do usługi.
// Błąd: ServiceNotFound.
template <typename TService>
std::expected<
    std::reference_wrapper<TService>,
    ServiceLocatorError
>
tryGet();
```

```cpp
// Rejestracja według dynamicznego typu obiektu.
// Sukces: brak wartości.
// Błąd: NullService.
std::expected<void, ServiceLocatorError>
provideRuntime(ServicePtr service);
```

```cpp
// Pobranie z kontrolą typu w runtime.
// Sukces: referencja do usługi.
// Błędy: ServiceNotFound, InvalidServiceType.
template <typename TService>
std::expected<
    std::reference_wrapper<TService>,
    ServiceLocatorError
>
tryGetRuntime();
```

```cpp
// Pobranie bezszablonowe według przekazanego klucza.
// Sukces: współdzielony wskaźnik do IService.
// Błąd: ServiceNotFound.
RuntimeGetResult
tryGetRuntime(const std::type_index& key);
```

---

# Podsumowanie

Modyfikacja nie zmienia sposobu przechowywania usług ani mechanizmu wyszukiwania po `std::type_index`. Zmienia kontrakt wszystkich operacji, które mogą się nie udać.

Najważniejsze rezultaty przebudowy:

```text
throw runtime_error
        ↓
std::unexpected(ServiceLocatorError)
```

```text
TService&
        ↓
expected<reference_wrapper<TService>, Error>
```

```text
shared_ptr<IService>
        ↓
expected<shared_ptr<IService>, Error>
```

```text
void provide(...)
        ↓
expected<void, Error> provide(...)
```

Po zmianie każde przewidywalne niepowodzenie jest widoczne bezpośrednio w sygnaturze funkcji. Klient może obsłużyć wynik klasycznie przez `if`, przekształcić błąd przez `transform_error()` albo zbudować łańcuch operacji za pomocą `transform()` i `and_then()`.
