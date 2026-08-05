
# Wykład: Service Locator w C++

## Wprowadzenie

W tym przykładzie `ServiceLocator` pełni rolę centralnego rejestru usług.

Pozwala:

- zarejestrować usługę,
- przechowywać różne typy usług w jednej kolekcji,
- pobierać usługę na podstawie jej typu,
- korzystać z jednej współdzielonej instancji rejestru.

W kodzie jako usługi występują:

- `Logger`,
- `FileLogger`,
- `DoSomething`.

---

# Wspólna klasa bazowa usług

```cpp
class IService {
public:
    virtual ~IService() = default;
};
```

`IService` jest wspólną klasą bazową dla wszystkich usług.

Dzięki temu `ServiceLocator` może przechowywać różne klasy pod jednym typem:

```cpp
std::shared_ptr<IService>
```

Wirtualny destruktor jest potrzebny z dwóch powodów:

1. pozwala bezpiecznie usuwać klasy pochodne przez wskaźnik do `IService`,
2. sprawia, że `IService` jest typem polimorficznym, co umożliwia używanie RTTI, `typeid`, `dynamic_cast` i `dynamic_pointer_cast`.

---

# Przykładowe usługi

## Logger

```cpp
class Logger : public IService {
public:
    void log(const std::string& message) {
        std::cout << message;
    }
};
```

`Logger` wypisuje wiadomości na standardowe wyjście.

---

## FileLogger

```cpp
class FileLogger : public IService {
public:
    explicit FileLogger(const std::string& filename)
        : filename_(filename)
    {
        std::cout
            << "[FileLogger] Wykonuję komendę: utwórz plik \""
            << filename_
            << "\" (symulacja — bez realnego zapisu na dysk)\n";
    }

    void log(const std::string& message) {
        std::cout
            << "[FileLogger] Wykonuję komendę: dopisz do pliku \""
            << filename_
            << "\": "
            << message;
    }

private:
    std::string filename_;
};
```

`FileLogger` symuluje zapis do pliku.

---

## DoSomething

```cpp
class DoSomething : public IService {
public:
    void do_() {
        std::cout << "do something\n";
    }
};
```

Ta klasa pokazuje, że `ServiceLocator` nie musi przechowywać wyłącznie loggerów.

Może przechowywać dowolne klasy dziedziczące po `IService`.

---

# Pełny kod klasy ServiceLocator

```cpp
class ServiceLocator {
public:
    static ServiceLocator& instance() {
        static ServiceLocator locator;
        return locator;
    }

    template <typename TService>
    void provide(std::shared_ptr<TService> service) {
        static_assert(
            std::is_base_of<IService, TService>::value,
            "TService musi dziedziczyć po IService"
        );

        services_[std::type_index(typeid(TService))] =
            std::move(service);
    }

    template <typename TService>
    TService& get() {
        auto it = services_.find(
            std::type_index(typeid(TService))
        );

        if (it == services_.end()) {
            throw std::runtime_error(
                std::string(
                    "ServiceLocator: brak zarejestrowanej usługi typu "
                ) + typeid(TService).name()
            );
        }

        return *std::static_pointer_cast<TService>(
            it->second
        );
    }

    void provideRuntime(std::shared_ptr<IService> service) {
        const IService& serviceRef = *service;

        const std::type_index key(
            typeid(serviceRef)
        );

        services_[key] = std::move(service);
    }

    template <typename TService>
    TService& getRuntime() {
        auto it = services_.find(
            std::type_index(typeid(TService))
        );

        if (it == services_.end()) {
            throw std::runtime_error(
                std::string(
                    "ServiceLocator: brak zarejestrowanej usługi typu "
                ) + typeid(TService).name()
            );
        }

        auto casted =
            std::dynamic_pointer_cast<TService>(
                it->second
            );

        if (!casted) {
            throw std::runtime_error(
                std::string(
                    "ServiceLocator: usługa pod tym kluczem nie jest typu "
                ) + typeid(TService).name()
            );
        }

        return *casted;
    }

    std::shared_ptr<IService> getRuntime(
        const std::type_index& key
    ) {
        auto it = services_.find(key);

        if (it == services_.end()) {
            throw std::runtime_error(
                std::string(
                    "ServiceLocator: brak zarejestrowanej usługi typu "
                ) + key.name()
            );
        }

        return it->second;
    }

private:
    ServiceLocator() = default;

    std::unordered_map<
        std::type_index,
        std::shared_ptr<IService>
    > services_;
};
```

---

# Pole przechowujące usługi

Najważniejszym polem klasy jest:

```cpp
std::unordered_map<
    std::type_index,
    std::shared_ptr<IService>
> services_;
```

Mapa przechowuje pary:

```text
typ usługi → obiekt usługi
```

Przykładowo:

```text
typeid(Logger)      → shared_ptr<Logger>
typeid(FileLogger)  → shared_ptr<FileLogger>
typeid(DoSomething) → shared_ptr<DoSomething>
```

Formalnie mapa przechowuje wartości typu:

```cpp
std::shared_ptr<IService>
```

ale rzeczywiste obiekty nadal są typu:

```cpp
Logger
FileLogger
DoSomething
```

---

# ServiceLocator jako Singleton

Metoda:

```cpp
static ServiceLocator& instance() {
    static ServiceLocator locator;
    return locator;
}
```

tworzy jedną współdzieloną instancję `ServiceLocator`.

Pierwsze wywołanie:

```cpp
ServiceLocator::instance();
```

tworzy obiekt:

```cpp
static ServiceLocator locator;
```

Każde następne wywołanie zwraca referencję do tego samego obiektu.

Prywatny konstruktor:

```cpp
private:
    ServiceLocator() = default;
```

uniemożliwia napisanie:

```cpp
ServiceLocator locator;
```

Klient musi użyć:

```cpp
ServiceLocator::instance();
```

---

# Rejestrowanie usługi przez provide()

Kod:

```cpp
template <typename TService>
void provide(std::shared_ptr<TService> service) {
    static_assert(
        std::is_base_of<IService, TService>::value,
        "TService musi dziedziczyć po IService"
    );

    services_[std::type_index(typeid(TService))] =
        std::move(service);
}
```

Przykład użycia:

```cpp
auto consoleLogger =
    std::make_shared<Logger>();

ServiceLocator::instance()
    .provide<Logger>(consoleLogger);
```

## Co dzieje się krok po kroku?

### Krok 1

Tworzony jest obiekt `Logger`:

```cpp
auto consoleLogger =
    std::make_shared<Logger>();
```

Typ zmiennej:

```cpp
std::shared_ptr<Logger>
```

### Krok 2

Wywoływana jest metoda:

```cpp
provide<Logger>(consoleLogger);
```

Kompilator podstawia:

```cpp
TService = Logger
```

### Krok 3

Sprawdzane jest dziedziczenie:

```cpp
std::is_base_of<IService, Logger>::value
```

Jeżeli `Logger` nie dziedziczyłby po `IService`, program nie skompilowałby się.

### Krok 4

Tworzony jest klucz:

```cpp
std::type_index(typeid(Logger))
```

### Krok 5

Obiekt trafia do mapy:

```cpp
services_[typeid(Logger)] = consoleLogger;
```

W uproszczeniu:

```text
Logger → obiekt Loggera
```

---

# Pobieranie usługi przez get()

Kod:

```cpp
template <typename TService>
TService& get() {
    auto it = services_.find(
        std::type_index(typeid(TService))
    );

    if (it == services_.end()) {
        throw std::runtime_error(
            std::string(
                "ServiceLocator: brak zarejestrowanej usługi typu "
            ) + typeid(TService).name()
        );
    }

    return *std::static_pointer_cast<TService>(
        it->second
    );
}
```

Przykład:

```cpp
Logger& logger =
    ServiceLocator::instance().get<Logger>();

logger.log("Hello\n");
```

Metoda:

1. szuka wpisu pod kluczem `typeid(Logger)`,
2. sprawdza, czy usługa istnieje,
3. pobiera `shared_ptr<IService>`,
4. rzutuje go na `shared_ptr<Logger>`,
5. dereferencjonuje wskaźnik,
6. zwraca `Logger&`.

Nie jest tworzona kopia obiektu.

Zwracana jest referencja do obiektu przechowywanego przez `ServiceLocator`.

---

# Funkcje pomocnicze

W kodzie znajdują się krótkie funkcje upraszczające dostęp do usług.

## appLogger()

```cpp
inline Logger& appLogger() {
    return ServiceLocator::instance().get<Logger>();
}
```

Zamiast pisać:

```cpp
ServiceLocator::instance()
    .get<Logger>()
    .log("Hello\n");
```

można napisać:

```cpp
appLogger().log("Hello\n");
```

---

## appFileLogger()

```cpp
inline FileLogger& appFileLogger() {
    return ServiceLocator::instance().get<FileLogger>();
}
```

Użycie:

```cpp
appFileLogger().log(
    "=== Start programu ===\n"
);
```

---

## appDoSomething()

```cpp
inline DoSomething& appDoSomething() {
    return ServiceLocator::instance().get<DoSomething>();
}
```

---

# Rejestrowanie usług w main()

Usługi są tworzone na początku programu:

```cpp
auto consoleLogger =
    std::make_shared<Logger>();

auto fileLogger =
    std::make_shared<FileLogger>(
        "engine_log.txt"
    );

auto doSomething =
    std::make_shared<DoSomething>();
```

Następnie `Logger` i `FileLogger` są rejestrowane przez wersję szablonową:

```cpp
ServiceLocator::instance()
    .provide<Logger>(consoleLogger);

ServiceLocator::instance()
    .provide<FileLogger>(fileLogger);
```

`DoSomething` jest rejestrowane przez wersję runtime:

```cpp
ServiceLocator::instance()
    .provideRuntime(doSomething);
```

Od tego momentu wszystkie te usługi są dostępne przez `ServiceLocator`.

---

# Gdzie Logger jest używany?

## PlacementEngine

```cpp
class PlacementEngine {
public:
    void run() {
        appLogger().log("[PlacementEngine] Run\n");
    }
};
```

`PlacementEngine` nie otrzymuje loggera w konstruktorze.

Pobiera go globalnie przez `ServiceLocator`.

---

## Engine

```cpp
void start() {
    running_ = true;
    appLogger().log("[Engine] Start\n");
}
```

```cpp
void stop() {
    running_ = false;
    appLogger().log("[Engine] Stop\n");
}
```

```cpp
void sortVector(size_t index) {
    if (index >= data_.size()) {
        appLogger().log(
            "[Engine] Niepoprawny indeks wektora\n"
        );
        return;
    }

    std::ostringstream oss;
    oss << "[Engine] Sortuję strategią: "
        << sortStrategy_->name()
        << "\n";

    appLogger().log(oss.str());

    (*sortStrategy_)(data_[index]);
}
```

---

## SessionManagement

```cpp
void openSession() {
    if (!engine_) {
        appLogger().log(
            "[Session] Brak Engine\n"
        );
        return;
    }

    sessionActive_ = true;

    appLogger().log(
        "[Session] Otwieram sesję\n"
    );

    engine_->start();
}
```

---

## DummyGui

```cpp
void clickAddVector(
    const std::vector<int>& vec
) {
    if (!session_ || !addVectorFunc_) {
        appLogger().log(
            "[GUI] Brak dostępu do AddVector\n"
        );
        return;
    }

    appLogger().log(
        "[GUI] Kliknięto AddVector\n"
    );

    (session_->*addVectorFunc_)(vec);
}
```

---

## Configurator

```cpp
void configureGui(
    DummyGui& gui,
    SessionManagement& session
) {
    appLogger().log(
        "[Configurator] Przyznaję GUI dostęp do wybranych funkcji\n"
    );

    gui.connectAddVector(
        &session,
        &SessionManagement::addVectorFromGui
    );
}
```

---

## SessionAuditObserver

```cpp
SessionAuditObserver() {
    appLogger().log(
        "[Audit] Tworzę obserwatora audytu\n"
    );
}
```

---

## SessionEstablisher

```cpp
void establish() {
    appLogger().log(
        "[SessionEstablisher] Rozpoczynam ustanawianie sesji\n"
    );

    if (!checkPreconditions()) {
        appLogger().log(
            "[SessionEstablisher] Warunki wstępne niespełnione\n"
        );
        return;
    }

    connect();
    configure();
    finalizeSetup();

    appLogger().log(
        "[SessionEstablisher] Sesja ustanowiona\n"
    );
}
```

Każda z tych klas korzysta z tego samego obiektu `Logger`.

---

# Gdzie FileLogger jest używany?

W funkcji `main()`:

```cpp
appFileLogger().log(
    "=== Start programu ===\n"
);
```

oraz:

```cpp
appFileLogger().log(
    "=== Koniec programu ===\n"
);
```

Przepływ:

```text
appFileLogger()
        ↓
ServiceLocator::instance()
        ↓
get<FileLogger>()
        ↓
wyszukanie FileLoggera w mapie
        ↓
zwrócenie FileLogger&
        ↓
wywołanie log()
```

---

# Wersja runtime

Kod zawiera także wariant wykorzystujący RTTI.

## provideRuntime()

```cpp
void provideRuntime(
    std::shared_ptr<IService> service
) {
    const IService& serviceRef = *service;

    const std::type_index key(
        typeid(serviceRef)
    );

    services_[key] = std::move(service);
}
```

Metoda nie ma parametru szablonu.

Przyjmuje:

```cpp
std::shared_ptr<IService>
```

ale dzięki:

```cpp
typeid(serviceRef)
```

odczytuje rzeczywisty typ obiektu.

Dla obiektu `DoSomething` kluczem będzie:

```cpp
typeid(DoSomething)
```

a nie:

```cpp
typeid(IService)
```

---

## getRuntime<T>()

```cpp
template <typename TService>
TService& getRuntime() {
    auto it = services_.find(
        std::type_index(typeid(TService))
    );

    if (it == services_.end()) {
        throw std::runtime_error(
            "Brak usługi"
        );
    }

    auto casted =
        std::dynamic_pointer_cast<TService>(
            it->second
        );

    if (!casted) {
        throw std::runtime_error(
            "Niepoprawny typ usługi"
        );
    }

    return *casted;
}
```

W przeciwieństwie do `static_pointer_cast`, `dynamic_pointer_cast` sprawdza typ w czasie działania programu.

---

## Całkowicie bezszablonowe getRuntime()

```cpp
std::shared_ptr<IService> getRuntime(
    const std::type_index& key
);
```

Przykład użycia:

```cpp
std::shared_ptr<IService> service =
    ServiceLocator::instance().getRuntime(
        std::type_index(
            typeid(DoSomething)
        )
    );
```

Następnie klient sam wykonuje rzutowanie:

```cpp
auto* casted =
    dynamic_cast<DoSomething*>(
        service.get()
    );
```

---

# Minimalny kompletny przykład

```cpp
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <type_traits>
#include <unordered_map>

class IService {
public:
    virtual ~IService() = default;
};

class Logger : public IService {
public:
    void log(const std::string& message) {
        std::cout << message;
    }
};

class FileLogger : public IService {
public:
    explicit FileLogger(
        const std::string& filename
    )
        : filename_(filename)
    {
        std::cout
            << "[FileLogger] Tworzę plik: "
            << filename_
            << " (symulacja)\n";
    }

    void log(const std::string& message) {
        std::cout
            << "[FileLogger] Dopisuję do "
            << filename_
            << ": "
            << message;
    }

private:
    std::string filename_;
};

class DoSomething : public IService {
public:
    void do_() {
        std::cout << "do something\n";
    }
};

class ServiceLocator {
public:
    static ServiceLocator& instance() {
        static ServiceLocator locator;
        return locator;
    }

    template <typename TService>
    void provide(
        std::shared_ptr<TService> service
    ) {
        static_assert(
            std::is_base_of_v<
                IService,
                TService
            >,
            "TService musi dziedziczyć po IService"
        );

        services_[
            std::type_index(
                typeid(TService)
            )
        ] = std::move(service);
    }

    template <typename TService>
    TService& get() {
        const auto key =
            std::type_index(
                typeid(TService)
            );

        auto it = services_.find(key);

        if (it == services_.end()) {
            throw std::runtime_error(
                std::string(
                    "Brak usługi typu: "
                ) + typeid(TService).name()
            );
        }

        return *std::static_pointer_cast<
            TService
        >(it->second);
    }

private:
    ServiceLocator() = default;

    std::unordered_map<
        std::type_index,
        std::shared_ptr<IService>
    > services_;
};

inline Logger& appLogger() {
    return ServiceLocator::instance()
        .get<Logger>();
}

inline FileLogger& appFileLogger() {
    return ServiceLocator::instance()
        .get<FileLogger>();
}

inline DoSomething& appDoSomething() {
    return ServiceLocator::instance()
        .get<DoSomething>();
}

int main() {
    auto logger =
        std::make_shared<Logger>();

    auto fileLogger =
        std::make_shared<FileLogger>(
            "engine_log.txt"
        );

    auto doSomething =
        std::make_shared<DoSomething>();

    ServiceLocator::instance()
        .provide<Logger>(logger);

    ServiceLocator::instance()
        .provide<FileLogger>(fileLogger);

    ServiceLocator::instance()
        .provide<DoSomething>(doSomething);

    appLogger().log(
        "[Logger] Hello\n"
    );

    appFileLogger().log(
        "Hello z FileLoggera\n"
    );

    appDoSomething().do_();

    return 0;
}
```

---

# Najważniejszy sens wzorca

Bez `ServiceLocator` zależności byłyby przekazywane jawnie:

```cpp
class Engine {
public:
    explicit Engine(Logger& logger)
        : logger_(logger)
    {}

private:
    Logger& logger_;
};
```

W wersji z `ServiceLocator` klasa może napisać:

```cpp
appLogger().log(...);
```

## Zaleta

Dostęp do usług jest prosty i nie wymaga przekazywania ich przez wiele konstruktorów.

## Największa wada

Zależności klasy są ukryte.

Patrząc na:

```cpp
class Engine
```

nie widać od razu, że `Engine` potrzebuje `Loggera`.

Dowiadujemy się o tym dopiero po przeczytaniu implementacji metod.

Z tego powodu `Service Locator` jest wygodny, ale w większych systemach bywa zastępowany przez jawne wstrzykiwanie zależności, czyli `Dependency Injection`.
