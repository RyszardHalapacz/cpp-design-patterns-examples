
# Lecture: Service Locator in C++

## Introduction

In this example `ServiceLocator` acts as a central service registry.

It allows:

- registering a service,
- storing different service types in one collection,
- retrieving a service by its type,
- using one shared registry instance.

In the code the services are:

- `Logger`,
- `FileLogger`,
- `DoSomething`.

---

# Common Base Class for Services

```cpp
class IService {
public:
    virtual ~IService() = default;
};
```

`IService` is the common base class for all services.

This allows `ServiceLocator` to store different classes under one type:

```cpp
std::shared_ptr<IService>
```

The virtual destructor is needed for two reasons:

1. it allows safely deleting derived classes through a pointer to `IService`,
2. it makes `IService` a polymorphic type, which enables RTTI, `typeid`,
   `dynamic_cast`, and `dynamic_pointer_cast`.

---

# Example Services

## Logger

```cpp
class Logger : public IService {
public:
    void log(const std::string& message) {
        std::cout << message;
    }
};
```

`Logger` prints messages to standard output.

---

## FileLogger

```cpp
class FileLogger : public IService {
public:
    explicit FileLogger(const std::string& filename)
        : filename_(filename)
    {
        std::cout
            << "[FileLogger] Executing command: create file \""
            << filename_
            << "\" (simulation — no real disk write)\n";
    }

    void log(const std::string& message) {
        std::cout
            << "[FileLogger] Executing command: append to file \""
            << filename_
            << "\": "
            << message;
    }

private:
    std::string filename_;
};
```

`FileLogger` simulates writing to a file.

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

This class shows that `ServiceLocator` does not have to store only loggers.

It can store any class that inherits from `IService`.

---

# Full `ServiceLocator` Class Code

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
            "TService must inherit from IService"
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
                    "ServiceLocator: no service registered for type "
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
                    "ServiceLocator: no service registered for type "
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
                    "ServiceLocator: service under this key is not of type "
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
                    "ServiceLocator: no service registered for type "
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

# The Services Storage Field

The most important field of the class is:

```cpp
std::unordered_map<
    std::type_index,
    std::shared_ptr<IService>
> services_;
```

The map stores pairs:

```text
service type → service object
```

For example:

```text
typeid(Logger)      → shared_ptr<Logger>
typeid(FileLogger)  → shared_ptr<FileLogger>
typeid(DoSomething) → shared_ptr<DoSomething>
```

Formally the map stores values of type:

```cpp
std::shared_ptr<IService>
```

but the actual objects are still of type:

```cpp
Logger
FileLogger
DoSomething
```

---

# `ServiceLocator` as a Singleton

The method:

```cpp
static ServiceLocator& instance() {
    static ServiceLocator locator;
    return locator;
}
```

creates one shared `ServiceLocator` instance.

The first call to:

```cpp
ServiceLocator::instance();
```

creates the object:

```cpp
static ServiceLocator locator;
```

Every subsequent call returns a reference to the same object.

The private constructor:

```cpp
private:
    ServiceLocator() = default;
```

prevents writing:

```cpp
ServiceLocator locator;
```

The client must use:

```cpp
ServiceLocator::instance();
```

---

# Registering a Service via `provide()`

Code:

```cpp
template <typename TService>
void provide(std::shared_ptr<TService> service) {
    static_assert(
        std::is_base_of<IService, TService>::value,
        "TService must inherit from IService"
    );

    services_[std::type_index(typeid(TService))] =
        std::move(service);
}
```

Example usage:

```cpp
auto consoleLogger =
    std::make_shared<Logger>();

ServiceLocator::instance()
    .provide<Logger>(consoleLogger);
```

## Step by Step

### Step 1

A `Logger` object is created:

```cpp
auto consoleLogger =
    std::make_shared<Logger>();
```

Variable type:

```cpp
std::shared_ptr<Logger>
```

### Step 2

The method is called:

```cpp
provide<Logger>(consoleLogger);
```

The compiler substitutes:

```cpp
TService = Logger
```

### Step 3

Inheritance is checked:

```cpp
std::is_base_of<IService, Logger>::value
```

If `Logger` did not inherit from `IService` the program would not compile.

### Step 4

A key is created:

```cpp
std::type_index(typeid(Logger))
```

### Step 5

The object goes into the map:

```cpp
services_[typeid(Logger)] = consoleLogger;
```

Simplified:

```text
Logger → Logger object
```

---

# Retrieving a Service via `get()`

Code:

```cpp
template <typename TService>
TService& get() {
    auto it = services_.find(
        std::type_index(typeid(TService))
    );

    if (it == services_.end()) {
        throw std::runtime_error(
            std::string(
                "ServiceLocator: no service registered for type "
            ) + typeid(TService).name()
        );
    }

    return *std::static_pointer_cast<TService>(
        it->second
    );
}
```

Example:

```cpp
Logger& logger =
    ServiceLocator::instance().get<Logger>();

logger.log("Hello\n");
```

The method:

1. looks for an entry under the key `typeid(Logger)`,
2. checks whether the service exists,
3. retrieves `shared_ptr<IService>`,
4. casts it to `shared_ptr<Logger>`,
5. dereferences the pointer,
6. returns `Logger&`.

No copy of the object is made.

A reference to the object stored by `ServiceLocator` is returned.

---

# Helper Functions

The code contains short functions that simplify access to services.

## `appLogger()`

```cpp
inline Logger& appLogger() {
    return ServiceLocator::instance().get<Logger>();
}
```

Instead of writing:

```cpp
ServiceLocator::instance()
    .get<Logger>()
    .log("Hello\n");
```

one can write:

```cpp
appLogger().log("Hello\n");
```

---

## `appFileLogger()`

```cpp
inline FileLogger& appFileLogger() {
    return ServiceLocator::instance().get<FileLogger>();
}
```

Usage:

```cpp
appFileLogger().log(
    "=== Program start ===\n"
);
```

---

## `appDoSomething()`

```cpp
inline DoSomething& appDoSomething() {
    return ServiceLocator::instance().get<DoSomething>();
}
```

---

# Registering Services in `main()`

Services are created at the beginning of the program:

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

`Logger` and `FileLogger` are then registered via the template version:

```cpp
ServiceLocator::instance()
    .provide<Logger>(consoleLogger);

ServiceLocator::instance()
    .provide<FileLogger>(fileLogger);
```

`DoSomething` is registered via the runtime version:

```cpp
ServiceLocator::instance()
    .provideRuntime(doSomething);
```

From this point all these services are available through `ServiceLocator`.

---

# Where Is Logger Used?

## Engine

```cpp
void start() {
    running_ = true;
    appLogger().log("[Engine] Start\n");
}
```

```cpp
void sortVector(size_t index) {
    if (index >= data_.size()) {
        appLogger().log(
            "[Engine] Invalid vector index\n"
        );
        return;
    }

    std::ostringstream oss;
    oss << "[Engine] Sorting with strategy: "
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
            "[Session] No Engine\n"
        );
        return;
    }

    sessionActive_ = true;

    appLogger().log(
        "[Session] Opening session\n"
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
            "[GUI] No access to AddVector\n"
        );
        return;
    }

    appLogger().log(
        "[GUI] Clicked AddVector\n"
    );

    (session_->*addVectorFunc_)(vec);
}
```

Each of these classes uses the same `Logger` object.

---

# Where Is FileLogger Used?

In `main()`:

```cpp
appFileLogger().log(
    "=== Program start ===\n"
);
```

and:

```cpp
appFileLogger().log(
    "=== Program end ===\n"
);
```

Flow:

```text
appFileLogger()
        ↓
ServiceLocator::instance()
        ↓
get<FileLogger>()
        ↓
find FileLogger in map
        ↓
return FileLogger&
        ↓
call log()
```

---

# Runtime Version

The code also contains a variant that uses RTTI.

## `provideRuntime()`

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

The method has no template parameter.

It accepts:

```cpp
std::shared_ptr<IService>
```

but thanks to:

```cpp
typeid(serviceRef)
```

it reads the actual runtime type of the object.

For a `DoSomething` object the key will be:

```cpp
typeid(DoSomething)
```

not:

```cpp
typeid(IService)
```

---

## `getRuntime<T>()`

```cpp
template <typename TService>
TService& getRuntime() {
    auto it = services_.find(
        std::type_index(typeid(TService))
    );

    if (it == services_.end()) {
        throw std::runtime_error(
            "Service not found"
        );
    }

    auto casted =
        std::dynamic_pointer_cast<TService>(
            it->second
        );

    if (!casted) {
        throw std::runtime_error(
            "Invalid service type"
        );
    }

    return *casted;
}
```

Unlike `static_pointer_cast`, `dynamic_pointer_cast` checks the type at runtime.

---

## Fully Non-Template `getRuntime()`

```cpp
std::shared_ptr<IService> getRuntime(
    const std::type_index& key
);
```

Example usage:

```cpp
std::shared_ptr<IService> service =
    ServiceLocator::instance().getRuntime(
        std::type_index(
            typeid(DoSomething)
        )
    );
```

The client then performs the cast itself:

```cpp
auto* casted =
    dynamic_cast<DoSomething*>(
        service.get()
    );
```

---

# Minimal Complete Example

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
            << "[FileLogger] Creating file: "
            << filename_
            << " (simulation)\n";
    }

    void log(const std::string& message) {
        std::cout
            << "[FileLogger] Appending to "
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
            "TService must inherit from IService"
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
                    "Service not found: "
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
        "Hello from FileLogger\n"
    );

    appDoSomething().do_();

    return 0;
}
```

---

# The Core Meaning of the Pattern

Without `ServiceLocator` dependencies would be passed explicitly:

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

With `ServiceLocator` the class can write:

```cpp
appLogger().log(...);
```

## Advantage

Access to services is simple and does not require passing them through many
constructors.

## Biggest Drawback

The dependencies of a class are hidden.

Looking at:

```cpp
class Engine
```

it is not immediately obvious that `Engine` needs a `Logger`.

You only find out after reading the implementation of the methods.

For this reason `Service Locator` is convenient but in larger systems is often
replaced by explicit dependency injection.
